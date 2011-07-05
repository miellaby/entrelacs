#include <string.h>
#include <assert.h>

#include "sexp.h"
#include "entrelacs/entrelacs.h"

static Arrow arrowFromSexp(sexp_t* sx) {
  sexp_t *ssx;
  
  Arrow a, n;
  
  if (sx->list) {
     n = arrowFromSexp(sx->list);
  } else if (sx->ty == SEXP_VALUE && sx->aty == SEXP_BASIC
             && sx->val_used == 4 && !strcmp("Eve", sx->val)) {
     n = xl_Eve();
  } else {
     n = xl_btag(sx->val_used, sx->val);
  }
  
  a = n;
  while (sx->next) {
     sx = sx->next;
     if (sx->list) { // list
        n = arrowFromSexp(sx->list);
     } else if (sx->ty == SEXP_VALUE && sx->aty == SEXP_BASIC
                && sx->val_used == 4 && !strcmp("Eve", sx->val)) {
        n = xl_Eve();
     } else {
        n = xl_btag(sx->val_used, sx->val);
     }
     a = xl_arrow(a, n);
  }
  
  return a;     
}

static sexp_t *sexpFromArrow(Arrow a) {
   if (xl_isEve(a)) {
       return new_sexp_atom("Eve", 3, SEXP_BASIC);
       
   } else if (xl_typeOf(a) == XL_ARROW) {
       sexp_t *sexpHead = sexpFromArrow(xl_headOf(a));
       sexp_t *sexpTail = sexpFromArrow(xl_tailOf(a));
       sexpTail->next = sexpHead;
       return new_sexp_list(sexpTail);
       
   } else {
       uint32_t tagSize;
       char * tag = xl_btagOf(a, &tagSize);
       sexp_t *sexp = new_sexp_atom(tag, tagSize, SEXP_DQUOTE);
       free(tag);
       return sexp;
   }
}

Arrow xl_program(char* program) { // EL string

  if (!program || !program[0]) return xl_Eve(); /* null or empty string: return Eve */  

  pcont_t *pc = NULL;
  
  pc = cparse_sexp(program, strlen(program), pc);
  
  if (pc == NULL)
     return xl_Eve(); /* see sexp_errno */
  
  sexp_t *sx = pc->last_sexp;

  Arrow a = arrowFromSexp(sx);
  
  destroy_continuation(pc);
  destroy_sexp(sx);
  
  return a;
}

char* xl_programOf(Arrow a) { // returned value to be freed by the user
  sexp_t *sx = sexpFromArrow(a);
  
  CSTRING *s = NULL;
  print_sexp_cstr(&s, sx, 256);
  
  char *program = malloc(sizeof(char) * s->curlen); 
  assert(program);
  strcpy(program, s->base);
  
  sdestroy(s);
  
  return program;
}