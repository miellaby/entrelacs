#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>

#include "entrelacs/entrelacs.h"

static int print(Arrow arrow, void* context) {
  char* program = xl_programOf(arrow);
  fprintf(stderr, " %s\n", program);
  free(program);
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  FILE* fd;
  char buffer[1024];
  
  xl_init();
  char* str, programs[] = {
    "(let (join (lambda x (lambda y (arrow (x y)))) (root join))",
    NULL
  };
  Arrow root = xl_tag("root");
  Arrow childrenOf = xl_tag("childrenOf");
  Arrow unroot = xl_tag("unroot");
  for (int i = 0; str = programs[i]; i++) {
    
    fprintf(stderr, "%s", str);

    Arrow p = xl_program(str);
    Arrow r = xl_eval(Eve, p);
    print(r, NULL);
    xl_commit();
  }
  
  return EXIT_SUCCESS;
}
