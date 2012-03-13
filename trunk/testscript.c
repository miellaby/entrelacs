#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"

static Arrow print(Arrow arrow, Arrow context) {
  char* uri = uriOf(arrow);
  fprintf(stderr, "%s\n", uri);
  free(uri);
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

    Arrow a = uri(buffer);
    fprintf(stderr, " => ");
    print(a, EVE);
    Arrow command = head(a);
    Arrow arg = tail(a);

    if (command == root) {
       fprintf(stderr, "rooting ");
       print(arg, EVE);
       root(arg);
       commit();
    } else if (command == unroot) {
       fprintf(stderr, "unrooting ");
       print(arg, EVE);
       unroot(arg);
       commit();
    } else if (command == childrenOf) {
       fprintf(stderr, "childrenOf ");
       print(arg, EVE);
       fprintf(stderr, " ==> {\n");
       childrenOfCB(arg, print, EVE);
       fprintf(stderr, "}\n");
    } else {
       fprintf(stderr, "Unknown command: ");
       print(command, EVE);
       assert(1);
    }
  }
  
  return EXIT_SUCCESS;
}
