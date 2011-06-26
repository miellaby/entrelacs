#include "entrelacs/entrelacs.h"
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "space.h"
struct s_printArrowC { int size; int max; char* buffer; };

int printArrow(Arrow a, void *ctx) {
    struct s_printArrowC *context = (struct s_printArrowC *)ctx;
    fprintf(stderr, "Arrow %06x ", a);
    if (!context) { 
      struct s_printArrowC context0 = { 0, 0, NULL };
      geoalloc(&context0.buffer, &context0.max, &context0.size, sizeof(char), 1);
      context0.buffer[0] = '\0';
      printArrow(a, (void *)&context0);
      fprintf(stderr, "%s\n", context0.buffer);
      return 0;
    }

    char *s;
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
    return 0;
}

int main(int argc, char* argv[]) {
	xl_init();
    char output[255];
	Arrow helloWorld = xl_arrow(xl_tag("hello"), xl_tag("world"));
	char *s1 = xl_tagOf(xl_tailOf(helloWorld));
	char *s2 = xl_tagOf(xl_headOf(helloWorld));
	snprintf(output, 255, "%s %s", s1, s2);
    puts(output);
	assert(0 == strcmp(output, "hello world"));
	free(s1);
	free(s2);
    xl_root(helloWorld);
	xl_commit();
	assert(xl_isRooted(helloWorld));
    Arrow helloDude = xl_arrow(xl_tag("hello"), xl_tag("dude"));
    xl_root(helloDude);
    xl_childrenOf(xl_tag("hello"), printArrow, NULL);
    xl_unroot(helloWorld);
	assert(!xl_isRooted(helloWorld));
    xl_childrenOf(xl_tag("hello"), printArrow, NULL);
    xl_unroot(helloDude);
	xl_commit();

	return 0;
}