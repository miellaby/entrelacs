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

static Arrow let = 0, load = 0, escape = 0, var = 0, evalOp = 0, lambda = 0, lambdax = 0, operator = 0, continuation = 0, selfM = 0, arrowOp = 0, systemEnvironment = 0;

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
  if (sx == NULL)
     return Eve(); /* No completed sexp */
     
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
  ONDEBUG((fprintf(stderr, "   _resolve a="), printArrow(a, NULL)));
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
    } else if (t == var) {
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
  if (t == lambda || t == lambdax || t == arrowOp || t == escape || t == var) return 1;
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
     } else if (tail(k) == evalOp) { // #e# special "eval" continuation
       // k == (eval kk)
       Arrow kk = head(k);
       Arrow w = resolve(p, e, M);
       M = a(w, a(e, kk)); // unstack continuation and reinject evaluation result as input expression
       
     } else {
       // k == ((x (ss ee)) kk)
       dputs("k == ((x (ss ee)) kk)");
       Arrow w = resolve(p, e, M);
       Arrow kk = head(k);
       Arrow f = tail(k);
       Arrow x = tail(f);
       Arrow ss = tail(head(f));
       Arrow ee = head(head(f));
       if (isEve(x)) { // #e : variable name is Eve (let ((Eve a) s1))
          dputs("variable name is Eve (let ((Eve a) s1))");
          // ones loads the arrow directly into environment. It will be considered as a var->value binding
          M = a(ss, a(a(w, ee), kk)); // unstack continuation, postponed let solved
       } else {
          M = a(ss, a(a(a(x, w), ee), kk)); // unstack continuation, postponed let solved
       }
       return M;
     }
  } else if (tail(p) == load) { //load expression #e#
     // p == (load (s0 s1))
     Arrow s = head(p);
     Arrow s0 = tail(s);
     Arrow s1 = head(s);

     // (load (s0 s1)) <==> (let ((Eve s0) s1))) : direct environment loading
     // rewriting program as pp = (let ((Eve s0) s1))
     Arrow pp = a(let, a(a(Eve(), s0), s1));
     M = a(pp, a(e, k));
     return M;
 
  } else if (tail(p) == evalOp) { //eval expression
     // p == (eval s)
     Arrow s = head(p);

     M = a(s, a(e, a(evalOp, k))); // stack an eval continuation
     return M;

  } else if (tail(p) == let) { //let expression family
     // p == (let ((x v) s))
     dputs("p == (let ((x v) s))");
     Arrow v = head(tail(head(p)));
     Arrow x = tail(tail(head(p)));
     Arrow s = head(head(p));


     // FIXME x == "@M" case

     if (isTrivial(v)) { // Trivial let rule
       // p == (let ((x t) s))
       dputs("p == (let ((x t) s))");
       Arrow t = v;
       Arrow w = resolve(t, e, M);
       if (isEve(x)) { // #e# direct environment load
          M = a(s, a(a(w, e), k));
       } else {
          M = a(s, a(a(a(x, w), e), k));
       }
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
         //What it is an error? I don't remember anymore
         // if (vv0 == let) {
            // // error
            // M=a(a(escape, a(tag("illegal"), p)), Eve());
            // return M;
         // }
         Arrow vv1 = head(v0);
         // rewriting program to stage vv0, vv1, and (vv0 vv1) evaluation
         // pp = (let (((p Eve) vv0) (let (((Eve p) vv1) (let ((p ((var (p Eve)) (var (Eve p)))) (let ((x ((var p) v1)))))))))))
         Arrow pp = a(let, a(a(a(p, Eve()), vv0), a(let, a(a(a(Eve(), p), vv1), a(let, a(a(p, a(a(var, a(p, Eve())), a(var, a(Eve(), p)))), a(let, a(a(x, a(a(var, p), v1)), s))))))));
         M = a(pp, a(e, k));
         return M;
         
       } else {
         // p == (let ((x (t0 arg)) s)) where t0 should return a closure or such
         Arrow t0 = v0;
         Arrow arg = v1;
         Arrow C = resolve(t0, e, M);
         
       if (tail(C) != lambdax && !isTrivial(arg)) { // "head nested application in let" rule #e#
         // p == (let ((x (t0 s1)) s))
         dputs("p == (let ((x (t0 s1)) s))");
         Arrow s1 = arg;
         
         // rewriting program to stage s1 and (t0 s1) evaluation
         // pp = (let ((p s1) (let ((x (t0 (var p))) s))))
         Arrow pp = a(let, a(a(p, s1), a(let, a(a(x, a(t0, a(var, p))), s))));
         M = a(pp, a(e, k));
         return M;
         
       } else /* isTrivial(v1)) */ { // Serious let rule
         // p == (let ((x t) s)) where t = (t0 t1)
         dputs("p == (let ((x t) s)) where t = (t0 t1)");
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
           Arrow ee;
           if (tail(C) == lambdax) { // #e# special closure
             // C == (lambdax ((y ss) ee))
             dputs("C == (lambdax ((y ss) ee))");
             
             // special closure : C == ("lambdax" CC) w/ CC being a regular closure
             // - applied arrow is escaped by default (like in let construct)
             // - "self" --> CC added to closure environment (allows recursion)
             C = head(C);
             ee = head(C);
             ee = a(a(tag("self"), a(lambdax, C)), ee);
             w = t1; // t1 is NOT resolved

           } else {
             // C == ((y ss) ee)
             ee = head(C);
             dputs("C == ((y ss) ee)");
             w = resolve(t1, e, M);
           }
           Arrow y = tail(tail(C));
           Arrow ss = head(tail(C));
           M = a(ss, a(a(a(y, w), ee), a(a(x, a(s, e)), k))); // stacks up a continuation
           return M;
           
         }
       }
     }
     }
  } else if (!isTrivial(tail(p))) { // Not trivial function in application #e#
     // p == (s v)
     dputs("p == (s v)");
     Arrow s = tail(p);
     Arrow v = head(p);
     // rewriting program with a let
     // pp = (let ((p s) ((var p) v)))
     Arrow pp = a(let, a(a(p, s), a(a(var, p), v)));
     M = a(pp, a(e, k));
     return M;

  }

  // p == (t0 arg) where t0 should return a closure or such
  Arrow t0 = tail(p);
  Arrow arg = head(p);
  Arrow C = resolve(t0, e, M);
  if (tail(C) != lambdax && !isTrivial(arg)) { // Not trivial parameter in application #e#
     // p == (t0 s)
     dputs("p == (t0 s)");
     Arrow s = arg;
     
     // rewriting program with a let
     // pp = (let ((p s) (t0 (var p))))
     Arrow pp = a(let, a(a(p, s), a(t0, a(var, p))));
     M = a(pp, a(e, k));
     return M;

  } else { // Trivial application rule
     // p == (t0 t1)
     dputs("p == (t0 t1)");
     Arrow t1 = head(p);

     // Continuation stacking not needed!
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
       Arrow ee;
       if (tail(C) == lambdax) { // #e# special closure where applied arrow is not evaluated (like let)
         // C == (lambdax ((y ss) ee))
         dputs("C == (lambdax ((y ss) ee))");
         
         // special closure : C == ("lambdax" CC) w/ CC being a regular closure
         // - applied arrow is escaped by default (like in let construct)
         // - "self" --> CC added to closure environment (allows recursion)
         C = head(C);
         ee = head(C);
         ee = a(a(tag("self"), a(lambdax, C)), ee);
         w = t1; // t1 is NOT resolved

       } else {
         // C == ((x ss) ee)
         dputs("C == ((x ss) ee)");
         ee = head(C);
         w = resolve(t1, e, M);
       }
       Arrow x = tail(tail(C));
       Arrow ss = head(tail(C));
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
  load = tag("load");
  var = tag("var");
  escape = tag("escape");
  evalOp = tag("eval");
  lambda = tag("lambda");
  lambdax = tag("lambdax");
  operator = tag("operator");
  continuation = tag("continuation");
  selfM = tag("@M");
  arrowOp = tag("arrow");
  
  Arrow reserved = tag("reserved");
  root(a(reserved, let));
  root(a(reserved, load));
  root(a(reserved, var));
  root(a(reserved, escape));
  root(a(reserved, evalOp));
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
