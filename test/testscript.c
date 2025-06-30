#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>
#include "log/log.h"

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"

static Arrow printCB(Arrow arrow, Arrow context) {
    fprintf(stderr, "%O\n", arrow);
    return arrow;
}

int main(int argc, char **argv) {
  FILE* fd;
  char buffer[1024];
  log_init(NULL, "server,session,machine,space=debug");

  xl_init();
  xl_begin();
  DEFATOM(root);
  DEFATOM(childrenOf);
  DEFATOM(unroot);
  
  fd = fopen("test/testrc", "r");
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
  xl_over();
  return EXIT_SUCCESS;
}
