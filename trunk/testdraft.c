#include "entrelacs/entrelacs.h"
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "space.h"
struct s_printArrowC { int size; int max; char* buffer; };

int printArrow(Arrow a, void *ctx) {
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
    return 0;
}

int main(int argc, char* argv[]) {
	xl_init();
    char output[255];
    Arrow root = xl_tag("root");
    //xl_tag("unroot");
    //xl_tag("children");
    //Arrow h = xl_arrow(xl_tag("hello"), xl_tag("world"));
    Arrow root2 = xl_btag(5, "root");
    assert(root == root2);
	xl_commit();
	return 0;
}