#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "mem.h" // geoalloc


#define test_title(T) fprintf(stderr, T "\n")

struct s_printArrowC { int size; int max; char* buffer; };

Arrow printArrow(Arrow a, void *ctx) {
    struct s_printArrowC *context = (struct s_printArrowC *)ctx;
    if (!context) { 
      struct s_printArrowC context0 = { 0, 0, NULL };
      geoalloc(&context0.buffer, &context0.max, &context0.size, sizeof(char), 1);
      context0.buffer[0] = '\0';
      printArrow(a, (void *)&context0);
      fprintf(stderr, "%s\n", context0.buffer);
      return 0;
    }


	if (xl_isRooted(a)) {
        int size = context->size;
        geoalloc(&context->buffer, &context->max, &context->size, sizeof(char), size + 1);
        sprintf(context->buffer + size - 1, "_");
	}
	
    enum e_xlType t = xl_typeOf(a);
    if (t == XL_TAG) {
        int l;
        char* s = xl_btagOf(a, &l);
        int size = context->size;
        geoalloc(&context->buffer, &context->max, &context->size, sizeof(char), size + l - 1 + 2);
        sprintf(context->buffer + size - 1, "\"%s\"", s);
        free(s);
    } else if (t == XL_ARROW) {
        int size;
        size = context->size;
        geoalloc(&context->buffer, &context->max, &context->size, sizeof(char), size + 1);
        sprintf(context->buffer + size - 1, "(");
        printArrow(xl_tailOf(a), (void *)context);
        size = context->size;
        geoalloc(&context->buffer, &context->max, &context->size, sizeof(char), size + 2);
        sprintf(context->buffer + size - 1, ", ");
        printArrow(xl_headOf(a), (void *)context);
        size = context->size;
        geoalloc(&context->buffer, &context->max, &context->size, sizeof(char), size + 1);
        sprintf(context->buffer + size - 1, ")");
    }
    return a;
}

int basic() {
    // assimilate arrows
    test_title("assimilate arrows");
    DEFTAG(hello); // Arrow hello = xl_tag("hello");
    DEFTAG(world);
    DEFA(hello, world); // Arrow hello_world = xl_arrow(hello, world);
    
    // check regular arrows
    test_title("check regular arrows");
    assert(typeOf(hello_world) == XL_ARROW);
    assert(tail(hello_world) == hello);
    assert(head(hello_world) == world);
    
    // Check tags
    test_title("check tags");
    assert(typeOf(hello) == XL_TAG && typeOf(world) == XL_TAG);
	char *s1 = str(hello);
	char *s2 = str(world);
	assert(!strcmp("hello", s1) && !strcmp("world", s2));
	free(s1);
	free(s2);
    
    // check rooting
    test_title("check rooting");
    root(hello_world);
    assert(isRooted(hello_world));
    
    // check GC
    test_title("check GC");
	DEFTAG(loose);
    DEFA(hello, loose);
    commit();
    assert(typeOf(loose) == XL_UNDEF);
    assert(typeOf(hello_loose) == XL_UNDEF);
    
    // check rooting persistency
    test_title("check rooting persistency");
    assert(isRooted(hello_world));
    
    // check deduplication
    test_title("check deduplication");
    Arrow original = hello_world;
    {
       DEFTAG(hello);
       DEFTAG(world);
       DEFA(hello, world);
       assert(original == hello_world);
    }
    
    // check arrow connection
    test_title("arrow connection");
    DEFTAG(dude);
    DEFA(hello, dude);
    root(hello_dude);
    
    childrenOf(hello, printArrow, NULL);
    
    // check unrooting
    test_title("check unrooting");
    unroot(hello_world);
    assert(!isRooted(hello_world));
    
    // check GC after unrooting
    test_title("check GC after unrooting");
    commit();
    assert(XL_UNDEF == typeOf(hello_world));
    assert(XL_UNDEF == typeOf(world));
    assert(XL_TAG == typeOf(hello));

    // check arrow disconnection
    test_title("check arrow disconnection");
    childrenOf(hello, printArrow, NULL);
    
    unroot(hello_dude);
	commit();

    return 0;
}

int stress() {
    char buffer[50];
    Arrow tags[1000];
    Arrow pairs[500];
    
    // deduplication stress
    test_title("deduplication stress");
    for (int i = 0 ; i < 200; i++) {
        snprintf(buffer, 50, "This is the tag #%d", i);
        tags[i] = tag(buffer);
        if (i % 2) {
          int j = (i - 1) / 2; 
          pairs[j] = arrow(tags[i - 1], tags[i]);
          printArrow(tailOf(pairs[j]), NULL);
          printArrow(headOf(pairs[j]), NULL);
          printArrow(pairs[j], NULL);
       }
    }
    
    for (int i = 0 ; i < 200; i++) {
        snprintf(buffer, 50, "This is the tag #%d", i);
        Arrow tagi = tag(buffer);
        assert(tags[i] == tagi);
        if (i % 2) {
          int j = (i - 1) / 2; 
          Arrow pairj = arrow(tags[i - 1], tags[i]);
          assert(pairs[j] == pairj);
        }
    }
    
    // rooting stress (also preserve this material from GC for further tests)
    test_title("rooting stress");
    for (int i = 0 ; i < 200; i++) {
        root(tags[i]);
        if (i % 2) {
          int j = (i - 1) / 2; 
          root(pairs[j]);
        }
    }
    commit();
    
    // connection stress
    test_title("connection stress");
    DEFTAG(connectMe);
    root(connectMe);
    for (int i = 0 ; i < 200; i++) {
        Arrow child = arrow(connectMe, tags[i]);
        root(child);
    }
    for (int j = 0 ; j < 100; j++) {
        printArrow(pairs[j], NULL);
        Arrow child = arrow(connectMe, pairs[j]);
        root(child);
    }
    childrenOf(connectMe, printArrow, NULL);
    
    // disconnection stress
    test_title("disconnection stress");
    for (int i = 0 ; i < 200; i++) {
        Arrow child = arrow(connectMe, tags[i]);
        unroot(child);
    }
    for (int j = 0 ; j < 100; j++) {
        Arrow child = arrow(connectMe, pairs[j]);
        unroot(child);
    }
    childrenOf(connectMe, printArrow, NULL);
    
    // connecting stress (big depth)
    test_title("connecting stress (big depth)");
    Arrow loose = tag("save me!");
    Arrow a  = loose;
    for (int i = 0 ; i < 2; i++) {
        if (i % 2)
            a = arrow(a, tags[i]);
        else
            a = arrow(tags[i], a);
    }
    for (int j = 0 ; j < 100; j++) {
        if (j % 2)
            a = arrow(a, pairs[j]);
        else
            a = arrow(pairs[j], a);
    }
    root(a);
    commit();

    assert(typeOf(loose) == XL_TAG);
    printArrow(a, NULL);

    // disconnecting stress (big depth)
    test_title("disconnecting stress (big depth)");
    unroot(a);
    commit();
    assert(typeOf(loose) == XL_UNDEF);

    // unrooting stress
    test_title("unrooting stress");
    for (int i = 0 ; i < 200; i++) {
        unroot(tags[i]);
        if (i % 2) {
          int j = (i - 1) / 2; 
          unroot(pairs[j]);
        }
    }
    commit();
    
    return 0;
}

int main(int argc, char* argv[]) {
    xl_init();
    basic();
    stress();
	return 0;
}
