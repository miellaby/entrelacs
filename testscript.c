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
    print(a, NULL);
    Arrow command = head(a);
    Arrow arg = tail(a);

    if (command == root) {
       fprintf(stderr, "rooting ");
       print(arg, NULL);
       root(arg);
       commit();
    } else if (command == unroot) {
       fprintf(stderr, "unrooting ");
       print(arg, NULL);
       unroot(arg);
       commit();
    } else if (command == childrenOf) {
       fprintf(stderr, "childrenOf ");
       print(arg, NULL);
       fprintf(stderr, " ==> {\n");
       childrenOfCB(arg, print, Eve());
       fprintf(stderr, "}\n");
    } else {
       fprintf(stderr, "Unknown command: ");
       print(command, NULL);
       assert(1);
    }
  }
  
  return EXIT_SUCCESS;
}
