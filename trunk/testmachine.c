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
  char *str, *programs[] = {
    "(root (escape (+ 2 2)))",
    "(isRooted (escape (+ 2 2)))",
    "(unroot (escape (+ 2 2)))",
    "(isRooted (escape (+ 2 2)))",
    "(let ((t (lambda (x (arrow (x 2))))) (t 3)))",
    //"(let ((gset (lambda (x (lambda (y (root (arrow (x y)))))))) (gset (escape gset) gset)))",
    //"(let ((gget (lambda (x (childrenOf x (lambda c (isRooted (c (headOf(c) Eve)))))))) (root (arrow (espace gget) gget))))",
    //"(let ((join (lambda (x (lambda (y (arrow (x y))))))) (root (arrow ((escape join) join)))))",
    //"(childrenOf join (lambda a (isRooted a (a Eve))"
    NULL
  };
  for (int i = 0; str = programs[i]; i++) {
    
    fprintf(stderr, "Evaluating %s ...\n", str);

    Arrow p = xl_program(str);
    print(p, NULL);
    Arrow r = xl_eval(xl_Eve(), p);
    
    fprintf(stderr, "%s done. result = ", str, r);
    print(r, NULL);
    xl_commit();
  }
  
  return EXIT_SUCCESS;
}
