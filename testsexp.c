#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>

#include "sexp.h"
#include "entrelacs/entrelacs.h"

Arrow arrowFromSexp(sexp_t* sx) {
  sexp_t *ssx;
  
  Arrow a, n;
  
  if (sx->list) {
     a = arrowFromSexp(sx->list);
  } else {
     a = xl_btag(sx->val_used, sx->val);
  }
  
  while (sx->next) {
     sx = sx->next;
     if (sx->list) { // list
        n = arrowFromSexp(sx->list);
     } else {
        n = xl_btag(sx->val_used, sx->val);
     }
     a = xl_arrow(a, n);
  }
  return a;     
}

sexp_t *sexpFromArrow(Arrow a) {
   if (xl_typeOf(a) == XL_ARROW) {
       sexp_t *sexpHead = new_sexp_list(sexpFromArrow(xl_headOf(a)));
       sexp_t *sexpTail = new_sexp_list(sexpFromArrow(xl_tailOf(a)));
       sexpTail->next = sexpHead;
       return sexpHead;
   } else {
       uint32_t tagSize;
       char * tag = xl_btagOf(a, &tagSize);
       sexp_t *sexp = new_sexp_atom(tag, tagSize, SEXP_DQUOTE);
       free(tag);
       return sexp;
   }
}
int printArrowAsSexp(Arrow a, void *ctx) {
   sexp_t* sx = sexpFromArrow(a);
   char buffer[1024];
   print_sexp(buffer, 1024, sx);
   return 0;
}

int main(int argc, char **argv) {
  int fd;
  char pstr[1024];
  sexp_t *sx, *param;
  sexp_iowrap_t *iow;
  
  xl_init();
  
  fd = open("test.sexp",O_RDONLY);
  iow = init_iowrap(fd);
  sx = read_one_sexp(iow);
  
  Arrow root = xl_tag("root");
  Arrow childrenOf = xl_tag("childrenOf");
  Arrow unroot = xl_tag("unroot");
  while (sx) {
    Arrow a = arrowFromSexp(sx);
    Arrow command = xl_headOf(a);
    Arrow arg = xl_tailOf(a);

    switch (command) {
      root:
       xl_root(arg);
       break;
      unroot:
       xl_unroot(arg);
       break;
      children:
       xl_childrenOf(arg, printArrowAsSexp, NULL);
       break;
      default:
       fprintf(stderr, "Unkown command %s\n", xl_tagOf(command));
       assert(1);
    }
    xl_commit();
    
    destroy_sexp(sx);
    sx = read_one_sexp(iow);
  }
  
  destroy_iowrap(iow);

  exit(EXIT_SUCCESS);
}
