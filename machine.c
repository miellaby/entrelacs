#include <string.h>
#include <assert.h>

#include "space.h"
#include "sexp.h"
#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"

static Arrow let = 0, get = 0, escape = 0, lambda = 0, operator = 0, continuation = 0, M = 0, arrow = 0;
static Arrow arrowFromSexp(sexp_t* sx) {
  sexp_t* ssx;
  
  Arrow a, n;
  
  if (sx->list) {
     n = arrowFromSexp(sx->list);
  } else if (sx->ty == SEXP_VALUE && sx->aty == SEXP_BASIC
             && sx->val_used == 4 && !strcmp("Eve", sx->val)) {
     n = Eve();
  } else {
     n = btag(sx->val_used, sx->val);
  }
  
  a = n;
  while (sx->next) {
     sx = sx->next;
     if (sx->list) { // list
        n = arrowFromSexp(sx->list);
     } else if (sx->ty == SEXP_VALUE && sx->aty == SEXP_BASIC
                && sx->val_used == 4 && !strcmp("Eve", sx->val)) {
        n = Eve();
     } else {
        n = btag(sx->val_used, sx->val);
     }
     a = arrow(a, n);
  }
  
  return a;     
}

static sexp_t *sexpFromArrow(Arrow a) {
   if (isEve(a)) {
       return new_sexp_atom("Eve", 3, SEXP_BASIC);
       
   } else if (typeOf(a) == XL_ARROW) {
       sexp_t *sexpHead = sexpFromArrow(headOf(a));
       sexp_t *sexpTail = sexpFromArrow(tailOf(a));
       sexpTail->next = sexpHead;
       return new_sexp_list(sexpTail);
       
   } else {
       uint32_t tagSize;
       char* tag = btagOf(a, &tagSize);
       sexp_t* sexp = new_sexp_atom(tag, tagSize, SEXP_DQUOTE);
       free(tag);
       return sexp;
   }
}

Arrow xl_program(char* program) { // EL string

  if (!program || !program[0]) return Eve(); /* null or empty string: return Eve */  

  pcont_t *pc = NULL;
  
  pc = cparse_sexp(program, strlen(program), pc);
  
  if (pc == NULL)
     return Eve(); /* see sexp_errno */
  
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

Arrow xl_operator(XLCallBack hookp, char* contextp) {
  char hooks[64];
  snprintf(hooks, 64, "%p", hookp);
  char contexts[64];
  snprintf(contexts, 64, "%p", contextp);
  Arrow hook = tag(hooks);
  Arrow context = tag(contexts);
  return a(operator, a(hook, context));
}

Arrow xl_continuation(XLCallBack hookp, char* contextp) {
  char hooks[64];
  snprintf(hooks, 64, "%p", hookp);
  char contexts[64];
  snprintf(contexts, 64, "%p", contextp);
  Arrow hook = tag(hooks);
  Arrow context = tag(contexts);
  return a(continuation, a(hook, context));
}

static Arrow resolve(Arrow a, Arrow e, Arrow M) {
  int type = typeOf(a);
  if (type == XL_EVE) return Eve();
  if (a == M) return M;
  
  Arrow x = a;
  if (type == XL_ARROW) {
    Arrow t = tailOf(a);
    if (t == arrow) {
       Arrow t = headOf(a);
       Arrow t1= tailOf(t);
       Arrow t2 = tailOf(t);
       return a(resolve(t1, e, M), resolve(t2, e, M));
    } else if (t == escape) {
       return headOf(a);
    } else if (t == lambda) {
       return arrow(headOf(a), e);
    } else if (t == get) {
       x = headOf(a);
    } else {
      return a;
    }
  }
  Arrow se = e;
  while (!isEve(se)) {
    Arrow b = tailOf(se);
    Arrow bx = tailOf(b);
    if (bx == x)
      return headOf(b);
    se = headOf(e);
  }
  return a;
}

static int isTrivial(Arrow s) {
  int type = typeOf(s);
  if (type != XL_ARROW) return 1; // true
  Arrow t = tailOf(s);
  if (t == lambda || t == arrow || t == escape || t == get) return 1;
  return 0;
}


static Arrow transition(Arrow M) { // M = (p, (e, k))
  Arrow p = tailOf(M);
  Arrow ke = headOf(M);
  Arrow e = tailOf(ke);
  Arrow k = headOf(ke);
  
  if (isTrivial(p)) { // Trivial expression
     // p = v
     
     if (isEve(k)) return M; // stop, program result = resolve(v, e, M)
     
     if (tail(k) == continuation) { //special system continuation #e#
        // k = (continuation (<hook> <context>))
        char *hooks = tagOf(tailOf(headOf(k)));
        char *contexts = tagOf(headOf(headOf(k)));
        XLCallBack hook;
        char *context;
        int n = sscanf(hooks, "%p", &hook);
        assert(n == 1);
        n = sscanf(contexts, "%p", &context);
        assert(n == 1);
        M = hook(M, context);
        free(hooks);
        free(contexts);
        return M;
     } else {
       // k = ((x (ss ee)) kk)
       Arrow w = resolve(p, e, M);
       Arrow kk = head(k);
       Arrow f = tail(k);
       Arrow x = tail(f);
       Arrow ss = tail(head(f));
       Arrow ee = head(head(f));
       M = a(ss, a(a(a(x, w), ee), kk)); // unstack continuation, postponed let solved
       return M;
     }
  } else if (tail(p) == let) { //let expression family
     // p = (let ((x v) s))
     Arrow v = head(tail(head(p)));
     Arrow x = tail(tail(head(p)));
     Arrow s = head(head(p));

     if (tail(x) == get) {  // #e#
       x = resolve(x, e, M);
     }

     // FIXME x == "@M" case

     if (isTrivial(v)) { // Trivial let rule
       // p = (let ((x t) s))
       Arrow t = v;
       Arrow w = resolve(t, e, M);
       M = a(s, a(a(a(x, w), e), k));
       return M;
       
     } else /* !isTrivial(v) */ { // Non trivial let rules
       // p = (let ((x (v0 v1) s)) : application in let
       Arrow v0 = tail(v);
       Arrow v1 = head(v);

       if (!isTrivial(v0)) { // "tail nested applications in let" rule #e#
         // p == (let ((x ((vv0 vv1) v1)) s))
         Arrow vv0 = tail(v0);
         if (vv0 == let) {
            // error
            M=a(a(escape, a(tag("illegal"), p)), Eve());
            return M;
         }
         Arrow vv1 = head(v0);
         // rewriting program to stage vv0, vv1, and (vv0 vv1) evaluation
         // pp = (let (((p Eve) vv0) (let (((Eve p) vv1) (let ((p ((get (p Eve)) (get (Eve p)))) (let ((x ((get p) v1)))))))))))
         Arrow pp = a(let, a(a(a(p, Eve()), vv0), a(let, a(a(a(Eve(), p), vv1), a(let, a(a(p, a(a(get, a(p, Eve())), a(get, a(Eve(), p)))), a(let, a(a(x, a(a(get, p), v1)), s))))))));
         M = a(pp, a(e, k));
         return M;
         
       } else if (!isTrivial(v1)) { // "head nested application in let" rule #e#
         // p == (let ((x (t0 s1)) s))
         Arrow t0 = v0;
         Arrow s1 = v1;
         // rewriting program to sage v1 and (t0 v1) evaluation
         // pp = (let ((p s1) (let ((x (t0 (get p))) s))))
         Arrow pp = a(let, a(a(p, s1), a(let, a(a(x, a(t0, a(get, p))), s))));
         M = a(pp, a(e, k));
         return M;
         
       } else /* isTrivial(v1)) */ { // Serious let rule
         // p = (let ((x t) s)) where t = (t0 t1)
         Arrow t0 = v0;
         Arrow t1 = v1;
         Arrow w = resolve(t1, e, M);
         Arrow C = resolve(t0, e, M);
         if (tail(C) == operator) { // System call special case
           // C = (operator (hook context))
           char* hooks = str(tail(head(C)));
           char* contexts = str(head(head(C)));
           XLCallBack hook;
           char *context;
           int n = sscanf(hooks, "%p", &hook);
           assert(n == 1);
           n = sscanf(contexts, "%p", &context);
           assert(n == 1);
           Arrow r = hook(w, context);
           Arrow ne = a(a(x, r), e);
           M = a(s, a(ne, k));
           return M;
           
         } else { // closure case
           // C = ((y ss) ee)
           Arrow ee = head(C);
           Arrow y = tail(tail(C));
           Arrow ss = head(tail(C));
           M = a(ss, a(a(a(y, w), ee), a(a(x, a(s, e)), k))); // stacks up a continuation
           return M;
           
         }
       }
     }
  } else if (!isTrivial(tail(p))) { // Not trivial function in application #e#
     // p = (s v)
     Arrow s = tail(p);
     Arrow v = head(p);
     // rewriting program with a let
     // pp = (let ((p s) ((get p) v)))
     Arrow pp = a(let, a(a(p, s), a(a(get, p), v)));
     M = a(pp, a(e, k));
     return M;

  } else if (!isTrivial(head(p))) { // Not trivial parameter in application #e#
     // p = (t s)
     Arrow t = tail(p);
     Arrow s = head(p);
     // rewriting program with a let
     // pp = (let ((p s) (t (get p))))
     Arrow pp = a(let, a(a(p, s), a(t, a(get, p))));
     M = a(pp, a(e, k));
     return M;

  } else { // Trivial application rule
     // p = (t0 t1)
     // Stacking not needed
     Arrow t0 = tail(p);
     Arrow t1 = head(p);
     Arrow w = resolve(t1, e, M);
     Arrow C = resolve(t0, e, M);
     if (tail(C) == operator) { // System call case
       // C = (operator (hook context))
       char* hooks = str(tail(head(C)));
       char* contexts = str(head(head(C)));
       XLCallBack hook;
       char *context;
       int n = sscanf(hooks, "%p", &hook);
       assert(n == 1);
       n = sscanf(contexts, "%p", &context);
       assert(n == 1);
       Arrow r = hook(w, context);
       M = a(r, a(e, k));
       return M;
       
     } else { // closure case
       // C = ((x ss) ee)
       Arrow x = tail(tail(C));
       Arrow ss = head(tail(C));
       Arrow ee = head(C);
       M = a(ss, a(a(a(x, w), ee), k));
       return M;
     }
  }
}

Arrow xl_run(Arrow rootStack, Arrow M) {
  M = transition(M);
  while (!isEve(head(head(M)))) {
    M = transition(M);
  }
  Arrow p = tail(M);
  Arrow e = tail(head(M));
  return resolve(p, e, M);
}

Arrow xl_eval(Arrow rootStack, Arrow program) {
  M = a(program, Eve());
  return xl_run(rootStack, M);
}

void xl_init() {
  int rc = space_init();

  let = tag("let");
  get = tag("get");
  escape = tag("escape");
  lambda = tag("lambda");
  operator = tag("operator");
  continuation = tag("continuation");
  M = tag("@M");
  arrow = tag("arrow");
  
  Arrow reserved = tag("reserved");
  root(a(reserved,let));
  root(a(reserved,get));
  root(a(reserved,escape));
  root(a(reserved,lambda));
  root(a(reserved,operator));
  root(a(reserved,continuation));
  root(a(reserved,M));
  root(a(reserved,arrow));
  xl_commit();
}