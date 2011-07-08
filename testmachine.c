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
  char *str, *programs[] = {
    "(escape (+ 2 2))",
    //"(let ((gset (lambda (x (lambda (y (root (arrow (x y)))))))) (gset (escape gset) gset)))",
    //"(let ((gget (lambda (x (childrenOf x (lambda c (isRooted (c (headOf(c) Eve)))))))) (root (arrow (espace gget) gget))))",
    //"(let ((join (lambda (x (lambda (y (arrow (x y))))))) (root (arrow ((escape join) join)))))",
    //"(childrenOf join (lambda a (isRooted a (a Eve))"
    NULL
  };
  for (int i = 0; str = programs[i]; i++) {
    
    fprintf(stderr, "Evaluating %s ...\n", str);

    Arrow p = xl_program(str);
    Arrow r = xl_eval(xl_Eve(), p);
    
    fprintf(stderr, "%s done. result = ", str, r);
    print(r, NULL);
    xl_commit();
  }
  
  return EXIT_SUCCESS;
}
