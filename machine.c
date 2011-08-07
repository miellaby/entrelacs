#include <string.h>
#include <assert.h>

#include "space.h"
#include "sexp.h"
#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"

#if !(defined PRODUCTION) || defined DEBUG_MACHINE
#define ONDEBUG(w) (fprintf(stderr, "%s:%d ", __FILE__, __LINE__), w)
#else
#define ONDEBUG(w)
#endif
#define dputs(S) ONDEBUG(fprintf(stderr, "%s\n", S))

static Arrow let = 0, escape = 0, resolveOp = 0, lambda = 0, lambdax = 0, operator = 0, continuation = 0, selfM = 0, arrowOp = 0, systemEnvironment = 0;

static Arrow printArrow(Arrow arrow, void* context) {
  char* program = xl_programOf(arrow);
  fprintf(stderr, "%s\n", program);
  free(program);
  return arrow;
}

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
  assert(s->curlen);
  char *program = (char *)malloc(sizeof(char) * s->curlen + 1); 
  assert(program);
  strncpy(program, s->base, s->curlen);
  program[s->curlen] = '\0';
  
  sdestroy(s);
  
  return program;
}
static void machine_init();

Arrow xl_operator(XLCallBack hookp, char* contextp) {
  char hooks[64];
  snprintf(hooks, 64, "%p", hookp);
  Arrow hook = tag(hooks);
  char contexts[64];
  Arrow context = (contextp ? (snprintf(contexts, 64, "%p", contextp),  tag(contexts)) : Eve());
  return a(operator, a(hook, context));
}

Arrow xl_continuation(XLCallBack hookp, char* contextp) {
  char hooks[64];
  snprintf(hooks, 64, "%p", hookp);
  Arrow hook = tag(hooks);
  char contexts[64];
  Arrow context = (contextp ? (snprintf(contexts, 64, "%p", contextp),  tag(contexts)) : Eve());
  return a(continuation, a(hook, context));
}

static Arrow _resolve(Arrow a, Arrow e, Arrow M) {
  int type = typeOf(a);
  if (type == XL_EVE) return Eve();
  if (a == selfM) return M;
  
  Arrow x = a;
  if (type == XL_ARROW) {
    Arrow t = tailOf(a);
    if (t == arrowOp) {
       Arrow t = headOf(a);
       Arrow t1 = tailOf(t);
       Arrow t2 = headOf(t);
       return a(_resolve(t1, e, M), _resolve(t2, e, M));
    } else if (t == escape) {
       return headOf(a);
    } else if (t == lambda) {
       return arrow(headOf(a), e);
    } else if (t == lambdax) {
       return arrow(lambdax, arrow(headOf(a), e));
    } else if (t == resolveOp) {
       x = headOf(a);
    } else {
      return a;
    }
  }
  // environment matching loop
  Arrow se = e;
  while (!isEve(se)) {
    Arrow b = tailOf(se);
    Arrow bx = tailOf(b);
    if (bx == x)
      return headOf(b);
    se = headOf(se);
  }
  return a;
}

static Arrow resolve(Arrow a, Arrow e, Arrow M) {
  ONDEBUG((fprintf(stderr, "resolve a="), printArrow(a, NULL), fprintf(stderr, " in e="), printArrow(e, NULL)));
  Arrow w = _resolve(a, e, M);
  ONDEBUG((fprintf(stderr, "resolved in w="), printArrow(w, NULL)));
  return w;
}



static int isTrivial(Arrow s) {
  int type = typeOf(s);
  if (type != XL_ARROW) return 1; // true
  Arrow t = tailOf(s);
  if (t == lambda || t == lambdax || t == arrowOp || t == escape || t == resolveOp) return 1;
  return 0;
}


static Arrow transition(Arrow M) { // M = (p, (e, k))
  ONDEBUG((fprintf(stderr, "transition M="), printArrow(M, NULL)));

  Arrow p = tailOf(M);
  Arrow ke = headOf(M);
  Arrow e = tailOf(ke);
  Arrow k = headOf(ke);
  
  if (isTrivial(p)) { // Trivial expression
     // p == v
     dputs("p == v");

     if (isEve(k)) return M; // stop, program result = resolve(v, e, M)
     
     if (tail(k) == continuation) { //special system continuation #e#
        // k == (continuation (<hook> <context>))
        dputs("k == (continuation (<hook> <context>))");
        char *hooks = tagOf(tailOf(headOf(k)));
        char *contexts = tagOf(headOf(headOf(k)));
        XLCallBack hook;
        char *context;
        int n = sscanf(hooks, "%p", &hook);
        assert(n == 1);
        free(hooks);
        if (contexts) {
           n = sscanf(contexts, "%p", &context);
           assert(n == 1);
           free(contexts);
        } else
           context = NULL;
        M = hook(M, context);
        return M;
     } else {
       // k == ((x (ss ee)) kk)
       dputs("k == ((x (ss ee)) kk)");
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
     // p == (let ((x v) s))
     dputs("p == (let ((x v) s))");
     Arrow v = head(tail(head(p)));
     Arrow x = tail(tail(head(p)));
     Arrow s = head(head(p));

     if (tail(x) == resolveOp) {  // #e#
       dputs("x == (resolve a)");
       x = resolve(x, e, M);
     }

     // FIXME x == "@M" case

     if (isTrivial(v)) { // Trivial let rule
       // p == (let ((x t) s))
       dputs("p == (let ((x t) s))");
       Arrow t = v;
       Arrow w = resolve(t, e, M);
       M = a(s, a(a(a(x, w), e), k));
       return M;
       
     } else /* !isTrivial(v) */ { // Non trivial let rules
       // p == (let ((x (v0 v1) s)) : application in let
       dputs("p == (let ((x (v0 v1) s))");
       Arrow v0 = tail(v);
       Arrow v1 = head(v);

       if (!isTrivial(v0)) { // "tail nested applications in let" rule #e#
         // p == (let ((x ((vv0 vv1) v1)) s))
         dputs("p == (let ((x ((vv0 vv1) v1)) s))");
         Arrow vv0 = tail(v0);
         if (vv0 == let) {
            // error
            M=a(a(escape, a(tag("illegal"), p)), Eve());
            return M;
         }
         Arrow vv1 = head(v0);
         // rewriting program to stage vv0, vv1, and (vv0 vv1) evaluation
         // pp = (let (((p Eve) vv0) (let (((Eve p) vv1) (let ((p ((resolve (p Eve)) (resolve (Eve p)))) (let ((x ((resolve p) v1)))))))))))
         Arrow pp = a(let, a(a(a(p, Eve()), vv0), a(let, a(a(a(Eve(), p), vv1), a(let, a(a(p, a(a(resolveOp, a(p, Eve())), a(resolveOp, a(Eve(), p)))), a(let, a(a(x, a(a(resolveOp, p), v1)), s))))))));
         M = a(pp, a(e, k));
         return M;
         
       } else if (!isTrivial(v1)) { // "head nested application in let" rule #e#
         // p == (let ((x (t0 s1)) s))
         dputs("p == (let ((x (t0 s1)) s))");
         Arrow t0 = v0;
         Arrow s1 = v1;
         // rewriting program to stage s1 and (t0 s1) evaluation
         // pp = (let ((p s1) (let ((x (t0 (resolve p))) s))))
         Arrow pp = a(let, a(a(p, s1), a(let, a(a(x, a(t0, a(resolveOp, p))), s))));
         M = a(pp, a(e, k));
         return M;
         
       } else /* isTrivial(v1)) */ { // Serious let rule
         // p == (let ((x t) s)) where t = (t0 t1)
         dputs("p == (let ((x t) s)) where t = (t0 t1)");
         Arrow t0 = v0;
         Arrow t1 = v1;
         Arrow C = resolve(t0, e, M);
         if (tail(C) == operator) { // System call special case
           // C = (operator (hook context))
           dputs("C = (operator (hook context))");
           char* hooks = str(tail(head(C)));
           char* contexts = str(head(head(C)));
           XLCallBack hook;
           char *context;
           int n = sscanf(hooks, "%p", &hook);
           free(hooks);
           assert(n == 1);
           if (contexts) {
              n = sscanf(contexts, "%p", &context);
              assert(n == 1);
              free(contexts);
           } else
              context = NULL;
           Arrow w = resolve(t1, e, M);
           Arrow r = hook(w, context);
           Arrow ne = a(a(x, r), e);
           M = a(s, a(ne, k));
           return M;
         } else { // closure case
           Arrow w;
           if (tail(C) == lambdax) { // special closure where applied arrow is escaped by default (like let)
             // C == (lambdax ((y ss) ee))
             dputs("C == (lambdax ((y ss) ee))");
             C = head(C);
             if (tail(t1) == resolveOp) {
                dputs("x == (resolve a)");
                w = resolve(t1, e, M);
             } else
                w = t1; // Not resolving t1
           } else {
             // C == ((y ss) ee)
             dputs("C == ((y ss) ee)");
             w = resolve(t1, e, M);
           }
           Arrow ee = head(C);
           Arrow y = tail(tail(C));
           Arrow ss = head(tail(C));
           M = a(ss, a(a(a(y, w), ee), a(a(x, a(s, e)), k))); // stacks up a continuation
           return M;
           
         }
       }
     }
  } else if (!isTrivial(tail(p))) { // Not trivial function in application #e#
     // p == (s v)
     dputs("p == (s v)");
     Arrow s = tail(p);
     Arrow v = head(p);
     // rewriting program with a let
     // pp = (let ((p s) ((resolve p) v)))
     Arrow pp = a(let, a(a(p, s), a(a(resolveOp, p), v)));
     M = a(pp, a(e, k));
     return M;

  } else if (!isTrivial(head(p))) { // Not trivial parameter in application #e#
     // p == (t s)
     dputs("p == (t s)");
     Arrow t = tail(p);
     Arrow s = head(p);
     // rewriting program with a let
     // pp = (let ((p s) (t (resolve p))))
     Arrow pp = a(let, a(a(p, s), a(t, a(resolveOp, p))));
     M = a(pp, a(e, k));
     return M;

  } else { // Trivial application rule
     // p == (t0 t1)
     dputs("p == (t0 t1)");
     // Stacking not needed
     Arrow t0 = tail(p);
     Arrow t1 = head(p);
     Arrow C = resolve(t0, e, M);
     if (tail(C) == operator) { // System call case
       // C == (operator (hook context))
       dputs("C == (operator (hook context))");
       char* hooks = str(tail(head(C)));
       char* contexts = str(head(head(C)));
       XLCallBack hook;
       char *context;
       int n = sscanf(hooks, "%p", &hook);
       assert(n == 1);
       free(hooks);
       if (contexts) {
          n = sscanf(contexts, "%p", &context);
          assert(n == 1);
          free(contexts);
       } else
          context = NULL;
       Arrow w = resolve(t1, e, M);
       Arrow r = hook(w, context);
       M = a(r, a(e, k));
       return M;
       
     } else { // closure case
       Arrow w;
       if (tail(C) == lambdax) { // special closure where applied arrow is escaped by default (like let)
         // C == (lambdax ((y ss) ee))
         dputs("C == (lambdax ((y ss) ee))");
         C = head(C);
         if (tail(t1) == resolveOp) {
           dputs("x == (resolve a)");
           w = resolve(t1, e, M);
         } else
           w = t1; // Not resolving t1 by default
       } else {
         // C == ((x ss) ee)
         dputs("C == ((x ss) ee)");
         w = resolve(t1, e, M);
       }
       Arrow x = tail(tail(C));
       Arrow ss = head(tail(C));
       Arrow ee = head(C);
       M = a(ss, a(a(a(x, w), ee), k));
       return M;   
     }
  }
}

Arrow xl_run(Arrow rootStack, Arrow M) {
  machine_init();
  M = transition(M);
  while (!isEve(head(head(M))) || !isTrivial(tail(M))) {
    M = transition(M);
  }
  dputs("finish");
  Arrow p = tail(M);
  Arrow e = tail(head(M));
  return resolve(p, e, M);
}

Arrow xl_eval(Arrow rootStack, Arrow program) {
  machine_init();
  Arrow M = a(program, a(systemEnvironment, Eve()));
  return xl_run(rootStack, M);
}

Arrow tailOfCB(Arrow arrow, void* context) {
   return a(escape, tail(arrow));
}

Arrow headOfCB(Arrow arrow, void* context) {
   return a(escape, head(arrow));
}

Arrow childrenOf_CB(Arrow arrow, void* context) { // warning: childrenOfCB already in use
   // usage: (childrenOf parent)
   XLEnum e = xl_childrenOf(arrow);
   Arrow list = Eve();
   while (xl_enumNext(e)) {
      Arrow child = xl_enumGet(e);
      list = a(child, list);
   }
   xl_freeEnum(e);
   return a(escape, list);
}

Arrow rootCB(Arrow arrow, void* context) {
   return a(escape, root(arrow));
}

Arrow unrootCB(Arrow arrow, void* context) {
   return a(escape, unroot(arrow));
}

Arrow isRootedCB(Arrow arrow, void* context) {
   return a(escape, isRooted(arrow));
}

Arrow ifCB(Arrow arrow, void *context) {
   if (isEve(arrow))
      return xl_operator(tailOfCB, NULL);
   else
      return xl_operator(headOfCB, NULL);
}

static struct fnMap_s {char *s; XLCallBack fn;} systemFns[] = {
 {"tailOf", tailOfCB},
 {"headOf", headOfCB},
 {"childrenOf", childrenOf_CB},
 {"root", rootCB},
 {"unroot", unrootCB},
 {"isRooted", isRootedCB},
 {"if", ifCB},
 {NULL, NULL}
};

static void machine_init() {
  if (let) return;
  let = tag("let");
  resolveOp = tag("resolve");
  escape = tag("escape");
  lambda = tag("lambda");
  lambdax = tag("lambdax");
  operator = tag("operator");
  continuation = tag("continuation");
  selfM = tag("@M");
  arrowOp = tag("arrow");
  
  Arrow reserved = tag("reserved");
  root(a(reserved, let));
  root(a(reserved, resolveOp));
  root(a(reserved, escape));
  root(a(reserved, lambda));
  root(a(reserved, lambdax));
  root(a(reserved, operator));
  root(a(reserved, continuation));
  root(a(reserved, selfM));
  root(a(reserved, arrowOp));
  
  systemEnvironment = Eve();
  for (int i = 0; systemFns[i].s != NULL ; i++) {
    Arrow op = a(tag(systemFns[i].s), xl_operator(systemFns[i].fn, NULL));
    systemEnvironment= a(op, systemEnvironment);
  }
  root(a(reserved, systemEnvironment));
}

void xl_init() {
  int rc = space_init();
}
