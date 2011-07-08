#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>

#include "entrelacs/entrelacs.h"

static Arrow print(Arrow arrow, void* context) {
  char* program = xl_programOf(arrow);
  fprintf(stderr, " %s\n", program);
  free(program);
  return arrow;
}

int main(int argc, char **argv) {
  FILE* fd;
  char buffer[1024];
  
  xl_init();
  Arrow root = xl_tag("root");
  Arrow childrenOf = xl_tag("childrenOf");
  Arrow unroot = xl_tag("unroot");
  
  fd = fopen("testrc", "r");
  assert(fd);

  while (fgets(buffer, 1024, fd)) {
    fprintf(stderr, "%s", buffer);

    Arrow a = xl_program(buffer);
    Arrow command = xl_headOf(a);
    Arrow arg = xl_tailOf(a);

    if (command == root) {
       xl_root(arg);
       fprintf(stderr, "rooting\n");
       xl_commit();
    } else if (command == unroot) {
       xl_unroot(arg);
       fprintf(stderr, "unrooting\n");
       xl_commit();
    } else if (command == childrenOf) {
       fprintf(stderr, " ==> {\n");
       xl_childrenOf(arg, print, NULL);
       fprintf(stderr, "}\n");
    } else {
       fprintf(stderr, "Unknown command %s\n", xl_tagOf(command));
       assert(1);
    }
  }
  
  return EXIT_SUCCESS;
}
