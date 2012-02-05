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
    "(run Eve)",
    "(@M)",
    "(undefinedThing Eve)",
    "(arrow x 2)",
    "(headOf Eve)",
    "(tailOf Eve)",
    "(tailOf escape 1 2 3 4 5)",
    "(headOf escape 1 2 3 4 5)",
    "(root arrow hello world)",
    "(childrenOf escape hello)",
    "(let (x 3) arrow x 2)",
    "(load (escape x 3) arrow x 2)",
    "(let (x headOf escape 1 2 3 4 5) arrow x 2)",
    "(let (x undefinedThing escape 1 2 3 4 5) arrow x 2)",
    "(let (x 3) escape arrow x 2)",
    "((lambda x x) 2)",
    "(let (y eval escape (lambda x arrow x x) 2) y)",
    "((lambda x arrow 2 x) 4)",
    "(let (identity lambda x x) identity 2)",
    "(let (myHeadOf lambda x headOf x) myHeadOf escape 1 2 3 4 5)",
    "(let (f lambda y arrow y 3) f 2)",
    "(let (p escape headOf escape 1 2 3 4 5) eval p)",
    "(let (myEscape lambdax raw raw) myEscape x 2)",
    "(let (exp escape lambda y arrow y 3) (eval exp) 2)",
    "(if arrow Eve escape hello goodbye)",
    "(if arrow world escape hello goodbye)",
    "(root escape + 2 2)",
    "(isRooted escape + 2 2)",
    "(unroot escape + 2 2)",
    "(isRooted escape + 2 2)",
    "(childrenOf escape reserved)",
    "((lambda x x) let (myHeadOf lambda x headOf x) myHeadOf escape 1 2 3 4 5)",
    "(let (t lambda x x) t let (myHeadOf lambda x headOf x) myHeadOf escape 1 2 3 4 5)",
    "(let (x let (myHeadOf lambda x headOf x) myHeadOf escape 1 2 3 4 5) arrow x 2)",
    "(let (crawlp escape lambda list if arrow list escape ((eval crawlp) headOf list) Eve) let (crawl eval crawlp) crawl Eve)",
    "(let (crawlp escape lambda list if arrow list escape ((eval crawlp) headOf list) Eve) let (crawl eval crawlp) crawl escape 1 2 3 4 5 Eve)",
    "(childrenOf escape set)",
    "(let (set lambdax variable lambda value root arrow variable value) (set set) set)",
    "(commit Eve)",
    "(childrenOf escape set)",
    "(set)",
    "(let (firstRootedp escape lambda list if arrow list escape (let (cond isRooted tailOf list) if arrow cond escape (tailOf list) (eval firstRootedp) headOf list) Eve) \
        let (firstRooted eval firstRootedp) \
        let (myGet lambdax variable \
            let (links childrenOf variable) \
                 let (link firstRooted links) \
                          headOf link) ((myGet set) myGet) myGet)",
    "((set join) lambda x lambda y arrow x y)",
    "((join 2) 3)",
    "(((myGet join) 2) 3)",
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
  }
  
  return EXIT_SUCCESS;
}
