#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"

static Arrow printCB(Arrow arrow, Arrow context) {
    fprintf(stderr, "%O\n", arrow);
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
    Arrow a = uri(buffer);
    fprintf(stderr, "%s assimilated as %O\n", buffer, a);
    Arrow command = head(a);
    Arrow arg = tail(a);

    if (command == root) {
       fprintf(stderr, "rooting %O\n", arg);
       root(arg);
       commit();
    } else if (command == unroot) {
       fprintf(stderr, "unrooting %O\n", arg);
       unroot(arg);
       commit();
    } else if (command == childrenOf) {
       fprintf(stderr, "childrenOf %O\n", arg);
       fprintf(stderr, " ==> {\n");
       childrenOfCB(arg, printCB, EVE);
       fprintf(stderr, "}\n");
    } else {
       fprintf(stderr, "Unknown command: %O", command);
       assert(1);
    }
  }
  
  return EXIT_SUCCESS;
}
