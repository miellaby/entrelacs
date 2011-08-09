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
    "(let ((x 3) (arrow (x 2))))",
    "(let ((x 3) (escape (arrow (x 2)))))",
    "(let ((x (lambda (y (arrow (y 3))))) (x 2)))",
    "(load ((escape (x 3)) (arrow (x 2))))",
    // "(root (escape (+ 2 2)))",
    // "(isRooted (escape (+ 2 2)))",
    // "(unroot (escape (+ 2 2)))",
    // "(isRooted (escape (+ 2 2)))",
    // "(let ((t (lambda (x (arrow (x 2))))) (t 3)))",
    // "(childrenOf reserved)",
    // "(let ((set (lambdax (variable (lambda (value (root (arrow (variable value)))))))) (set set set)))",
    // "(if Eve (escape (hello goodbye)))",
    // "(if world (escape (hello goodbye)))",
    //"((lambda (x (arrow (2 x)))) 4)",
    //"(let ((crawl ((lambdax (list ((if list ((self (headOf list)) Eve))))))) (crawl Eve)))",
    // "(letr ((firstRooted (lambda (list (if (tail list) ((tail list) (firstRooted (head list))))))) \
       // (letr ((get (lambdax (variable \
            // (let ((links (childrenOf variable)) \
                 // (let ((link (firstRooted variables)) \
                         // (head link)))))))) ((get set) get get)))))",
    //"(set join (lambda (x (lambda (y (arrow (x y)))))))",
    //"(childrenOf join (lambda a (isRooted a (a Eve))"
    NULL
  };
  for (int i = 0; str = programs[i]; i++) {
    
    fprintf(stderr, "Evaluating %s ...\n", str);

    Arrow p = xl_program(str);
    assert(!xl_isEve(p));
    print(p, NULL);
    Arrow r = xl_eval(xl_Eve(), p);
    
    fprintf(stderr, "%s done. result = ", str, r);
    print(r, NULL);
    xl_commit();
  }
  
  return EXIT_SUCCESS;
}
