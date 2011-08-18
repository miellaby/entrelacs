#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"

static Arrow print(Arrow arrow, Arrow context) {
  char* program = programOf(arrow);
  fprintf(stderr, " %s\n", program);
  free(program);
  return arrow;
}

int main(int argc, char **argv) {
  FILE* fd;
  char buffer[1024];
  
  xl_init();
  DEFTAG(root);
  DEFTAG(childrenOf);
  DEFTAG(unroot);
  
  fd = fopen("testrc", "r");
  assert(fd);

  while (fgets(buffer, 1024, fd)) {
    fprintf(stderr, "%s", buffer);

    Arrow a = program(buffer);
    Arrow command = head(a);
    Arrow arg = tail(a);

    if (command == root) {
       fprintf(stderr, "rooting\n");
       root(arg);
       commit();
    } else if (command == unroot) {
       fprintf(stderr, "unrooting\n");
       unroot(arg);
       commit();
    } else if (command == childrenOf) {
       fprintf(stderr, " ==> {\n");
       childrenOfCB(arg, print, Eve());
       fprintf(stderr, "}\n");
    } else {
       fprintf(stderr, "Unknown command %s\n", str(command));
       assert(1);
    }
  }
  
  return EXIT_SUCCESS;
}
