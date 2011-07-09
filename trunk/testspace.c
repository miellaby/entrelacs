#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "mem.h" // geoalloc

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
    DEFTAG(hello); // Arrow hello = xl_tag("hello");
    DEFTAG(world);
    DEFA(hello, world); // Arrow hello_world = xl_arrow(hello, world);
    
    // check arrow types
    
    // check regular arrows
    assert(typeOf(hello_world) == XL_ARROW);
    assert(tail(hello_world) == hello);
    assert(head(hello_world) == world);
    
    // Check tags
    assert(typeOf(hello) == XL_TAG && typeOf(world) == XL_TAG);
	char *s1 = str(hello);
	char *s2 = str(world);
	assert(!strcmp("hello", s1) && !strcmp("world", s2));
	free(s1);
	free(s2);
    
    // check rooting
    root(hello_world);
	commit();

    // check deduplication
    Arrow original = hello_world;
    {
       DEFTAG(hello);
       DEFTAG(world);
       DEFA(hello, world);
       assert(original == hello_world);
    }
    
    // check rooting persistency
    assert(isRooted(hello_world));
    
    // check arrow connection
    DEFTAG(dude);
    DEFA(hello, dude);
    root(hello_dude);
    
    childrenOf(hello, printArrow, NULL);
    
    // check unrooting
    unroot(hello_world);
    assert(!isRooted(hello_world));
    
    // check Garbage Collection
    commit();
    assert(XL_UNDEF == typeOf(hello_world));
    assert(XL_UNDEF == typeOf(world));
    assert(XL_TAG == typeOf(hello));

    // check arrow disconnection
    childrenOf(hello, printArrow, NULL);
    
    unroot(hello_dude);
	commit();
}

int stress() {
    char buffer[50];
    Arrow arrows[1000];
    
    // deduplication stress
    for (int i = 0 ; i < 200; i++) {
        snprintf(buffer, 50, "This is the tag #%d", i);
        arrows[i] = tag(buffer);
    }
    for (int i = 0 ; i < 200; i++) {
        snprintf(buffer, 50, "This is the tag #%d", i);
        Arrow tagi = tag(buffer);
        assert(arrows[i] == tagi);
        root(tagi);
    }
    commit();
    
    // connectivity stress
    DEFTAG(connectMe);
    root(connectMe);
    for (int i = 0 ; i < 200; i++) {
        Arrow child = arrow(connectMe, arrows[i]);
        root(child);
    }
    childrenOf(connectMe, printArrow, NULL);
    commit();
    // disconnectivity stress
    for (int i = 0 ; i < 200; i++) {
        Arrow child = arrow(connectMe, arrows[i]);
        unroot(child);
     //   unroot(arrows[i]);
    }
    childrenOf(connectMe, printArrow, NULL);
}

int main(int argc, char* argv[]) {
    xl_init();
    basic();
    stress();
	return 0;
}
