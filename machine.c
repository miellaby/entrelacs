#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "space.h"
#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "session.h"

#if !(defined PRODUCTION) || defined DEBUG_MACHINE
#define ONDEBUG(w) (fprintf(stderr, "%s:%d ", __FILE__, __LINE__), w)
#else
#define ONDEBUG(w)
#endif
#define dputs(format, args...) ONDEBUG(fprintf(stderr, format "\n", ## args))

static Arrow let = 0, load = 0, environment = 0, escape = 0, var = 0,
   evalOp = 0, lambda = 0, lambdax = 0, operator = 0,
   continuation = 0, selfM = 0, arrowOp = 0, systemEnvironment = 0;

static void machine_init();

Arrow xl_operator(XLCallBack hookp, Arrow context) {
  char hooks[64];
  snprintf(hooks, 64, "%p", hookp);
  Arrow hook = tag(hooks);
  return a(operator, a(hook, context));
}

Arrow xl_continuation(XLCallBack hookp, Arrow context) {
  char hooks[64];
  snprintf(hooks, 64, "%p", hookp);
  Arrow hook = tag(hooks);
  return a(continuation, a(hook, context));
}

/** Load a binding into an environment arrow (list arrow), trying to remove a previous binding for this variable to limit its size */
static Arrow _load_binding(Arrow x, Arrow w, Arrow e) {
    // typically a(a(a(x, w), e)
    int i;
    Arrow et = e;
    Arrow temp = Eve();
#define MAX_SEARCH_PREVIOUS_BINDING 10

    for (i = 0; i < MAX_SEARCH_PREVIOUS_BINDING; i++) {
       if (isEve(et)) break;
       Arrow c = tailOf(et);
       if (tailOf(c) == x) break;
       temp = a(c, temp);
       et = headOf(et);
    }
    if (isEve(et) || i == MAX_SEARCH_PREVIOUS_BINDING) // no x binding found
        return a(a(x, w), e);
    
    Arrow ne = a(a(x, w), headOf(et));
    while (!isEve(temp)) {
       ne = a(tailOf(temp), ne);
       temp = headOf(temp);
    }
    return ne;
}


static Arrow _resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
    ONDEBUG((fprintf(stderr, "   _resolve a = %O\n", a)));
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
            return a(_resolve(t1, e, C, M), _resolve(t2, e, C, M));
        } else if (t == escape) {
            return headOf(a);
        } else if (t == lambda) {
            Arrow xs = headOf(a);
            return arrow(xs, e);
        } else if (t == lambdax) {
            Arrow xs = headOf(a);
            return arrow(lambdax, arrow(xs, e));
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

    // global matching loop
    Arrow slot = C ? a(C, x) : x;
    XLEnum enu = childrenOf(slot);
    Arrow found = EVE;
    while (xl_enumNext(enu)) {
        Arrow child = xl_enumGet(enu);
        if (isRooted(child) && tailOf(child) == slot) {
            found = child;
            break;
        }
    }
    xl_freeEnum(enu);
    if (found != EVE)
        return headOf(found);

    return a;
}

static Arrow resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
  ONDEBUG((fprintf(stderr, "resolve a = %O in e = %O\n", a, e)));
  Arrow w = _resolve(a, e, C, M);
  ONDEBUG((fprintf(stderr, "resolved in w = %O\n", w)));
  return w;
}


static int isTrivial(Arrow s) {
  int type = typeOf(s);
  if (type != XL_ARROW) return 1; // true
  Arrow t = tailOf(s);
  if (t == lambda || t == lambdax || t == arrowOp || t == escape || t == var) return 1;
  return 0;
}


static Arrow transition(Arrow C, Arrow M) { // M = (p, (e, k))
  ONDEBUG((fprintf(stderr, "transition M = %O\n", M)));

  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  Arrow e = tailOf(ek);
  Arrow k = headOf(ek);
  ONDEBUG((fprintf(stderr, "   p = %O\n   e = %O\n   k = %O\n", p, e, k)));
  
  if (isTrivial(p)) { // Trivial expression
     // p == v
     dputs("p == v trivial");

     if (isEve(k)) return M; // stop, program result = resolve(v, e, M)
     
     if (tail(k) == continuation) { //special system continuation #e#
        // k == (continuation (<hook> <context>))
        dputs("    k == (continuation (<hook> <context>))");
        Arrow continuationParameter = head(head(k));
        XLCallBack hook;
        char *hooks = str(tail(head(k)));
        int n = sscanf(hooks, "%p", &hook);
        assert(n == 1);
        free(hooks);
        M = hook(a(C, M), continuationParameter);
        return M;
        
     } else if (tail(k) == evalOp) { // #e# special "eval" continuation
       // k == (eval kk)
       dputs("    k == (eval kk)");
       Arrow kk = head(k);
       Arrow w = resolve(p, e, C, M);
       M = a(w, a(e, kk)); // unstack continuation and reinject evaluation result as input expression
       return M;
       
     } else {
       // k == ((x (ss ee)) kk)
       dputs("    k == ((x (ss ee)) kk)");
       Arrow w = resolve(p, e, C, M);
       Arrow kk = head(k);
       Arrow f = tail(k);
       Arrow x = tail(f);
       Arrow ss = tail(head(f));
       Arrow ee = head(head(f));
       if (isEve(x)) { // #e : variable name is Eve (let ((Eve a) s1))
          dputs("variable name is Eve (let ((Eve a) s1))");
          // ones loads the arrow directly into environment. It will be considered as a var->value binding
          M = a(ss, a(_load_binding(tailOf(w), headOf(w), ee), kk)); // unstack continuation, postponed let solved
       } else {
          M = a(ss, a(_load_binding(x, w, ee), kk)); // unstack continuation, postponed let solved
       }
       return M;
     }
  }
  
  if (tail(p) == load) { //load expression #e#
     // p == (load (s0 s1))
     dputs("p == (load (s0 s1))");
     Arrow s = head(p);
     Arrow s0 = tail(s);
     Arrow s1 = head(s);

     // (load (s0 s1)) <==> (let ((Eve s0) s1))) : direct environment loading
     // rewriting program as pp = (let ((Eve s0) s1))
     Arrow pp = a(let, a(a(Eve(), s0), s1));
     M = a(pp, ek);
     return M;
  }
  
  if (tail(p) == evalOp) { //eval expression
     // p == (eval s)
     dputs("p == (eval s)");
     Arrow s = head(p);

     M = a(s, a(e, a(evalOp, k))); // stack an eval continuation
     return M;
  }
  
  if (tail(p) == let) { //let expression family
     // p == (let ((x v) s))
     dputs("p == (let ((x v) s)) # let expression");
     Arrow v = head(tail(head(p)));
     Arrow x = tail(tail(head(p)));
     Arrow s = head(head(p));


     // FIXME x == "@M" case

     if (isTrivial(v)) { // Trivial let expression
       // p == (let ((x v:t) s))
       dputs("     v (%O) == t trivial", v);
       Arrow t = v;
       Arrow w = resolve(t, e, C, M);
       if (isEve(x)) { // #e# direct environment load
         dputs("          direct environment load");
          M = a(s, a(_load_binding(tail(w), head(w), e), k));
       } else {
          M = a(s, a(_load_binding(x, w, e), k));
       }
       return M;
     }
     
     // Non trivial let expressions ...
     
     Arrow v0 = tail(v);
     if (v0 == let) { // let expression as application closure in containing let #e#
         dputs("     v == let expression");
         // stack up a continuation
         // M = (v (e ((x (s e)) k)))
         M = a(v, a(e, a(a(x, a(s, e)), k)));
         return M;
     }
       
     Arrow v1 = head(v);
     
     if (v0 == evalOp) { // eval expression in containing let #e#
         // p == (let ((x (eval ss) s))
         dputs("     v == (eval ss) # an eval expression");
         Arrow ss = v1;
         M = a(ss, a(e, a(evalOp, a(a(x, a(s, e)), k)))); // stack an eval continuation
         return M;
     }


     if (!isTrivial(v0)) { // non trivial closure in application in let expression #e#
         // p == (let ((x v:(v0 v1)) s)) where v0 not trivial
         dputs("p == (let ((x v:(v0 v1)) s)) where v0 not trivial");
         // rewriting program to stage s0 evaluation
         // TODO use (tailOf v) instead of p as variable name
         // pp = (let ((p v0) (let ((x ((var p) v1)) s))))
         Arrow pp = a(let, a(a(p, v0), a(let, a(a(x, a(a(var, p), v1)), s))));
         M = a(pp, ek);
         return M;
     }

     int resolvedTail = tail(resolve(v0, e, C, M));

     if (resolvedTail != lambdax && !isTrivial(v1)) { // non trivial argument in application in let expression #e#
       // p == (let ((x v:(t0 v1)) s)) where v1 not trivial
       dputs("p == (let ((x v:(t0 v1)) s)) where v1 not trivial");
       
       // rewriting program to stage v1 and (v0 v1) evaluation
       // pp = (let ((p v1) (let ((x (v0 (var p))) s))))
       // TODO use (headOf v) instead of p as variable name
       Arrow pp = a(let, a(a(p, v1), a(let, a(a(x, a(v0, a(var, p))), s))));
       M = a(pp, ek);
       return M;
     }
      
     // p == (let ((x (t0 t1)) s)) where t0 is a closure or equivalent
     dputs("p == (let ((x v:(v0:t0 v1:t1) s))  # a really trivial application in let expression");
     Arrow t0 = v0;
     Arrow t1 = v1;
     Arrow closure = resolve(t0, e, closure, M);
     if (tail(closure) == closure) {
           ONDEBUG(fprintf(stderr, "info: %O is not a closure\n", closure));
           Arrow w = resolve(t1, e, C, M);
           M = a(s, a(_load_binding(x, a(closure, w), e), k));
           return M;
     }
     
     if (tail(closure) == operator) { // System call special case
           // C = (operator (hook context))
           dputs("C = (operator (hook context))");
           Arrow operatorParameter = head(head(closure));
           XLCallBack hook;
           char* hooks = str(tail(head(closure)));
           int n = sscanf(hooks, "%p", &hook);
           assert(n == 1);
           free(hooks);
           M = hook(a(closure, M), operatorParameter);
           return M;
           
     }

     // closure case
     Arrow w;
     Arrow ee;
     if (tail(closure) == lambdax) { // #e# special closure
             // C == (lambdax ((y ss) ee))
             dputs("C == (lambdax ((y ss) ee))");
             
             // special closure : C == ("lambdax" CC) w/ CC being a regular closure
             // - applied arrow is escaped by default (like in let construct)
             closure = head(closure);
             ee = head(closure);
             w = t1; // t1 is NOT resolved

     } else {
             // C == ((y ss) ee)
             ee = head(closure);
             dputs("C == ((y ss) ee)");
             w = resolve(t1, e, closure, M);
     }
     Arrow y = tail(tail(closure));
     Arrow ss = head(tail(closure));
     M = a(ss, a(_load_binding(y, w, ee), a(a(x, a(s, e)), k))); // stacks up a continuation
     return M;
  }

  // application cases
  dputs("p == (s v)");
  Arrow s = tail(p);
  Arrow v = head(p);

  if (!isTrivial(s)) { // Not trivial closure in application #e#
    dputs("p == (s v) where s is an application or such");
    // rewriting rule: pp = (let ((p s) ((var p) v)))
    // ==> M = (s (e ((p (((var p) v) e)) k)))
    M = a(s, a(e, a(a(p, a(a(a(var, p), v), e)), k)));
    return M;
  }

  if (tail(v) == let) { // let expression as application argument #e#
      dputs("    p == (s v:(let ((x vv) ss))");
    // rewriting rule: pp = (let ((p v) (s (var p))))
    // ==> M = (v (e ((p ((s (var p)) e)) k))))
    M = a(v, a(e, a(a(p, a(a(s, a(var, p)), e)), k)));
    return M;
  }

  if (tail(v) == evalOp) { //eval expression as application argument #e#
     // p == (s (eval ss))
     dputs("     v == (eval ss) # an eval expression");
     Arrow ss = head(v);
     M = a(ss, a(e, a(evalOp, a(a(p, a(a(s, a(var, p)), e)), k)))); // stack an eval continuation
     return M;
  }
  Arrow resolvedTail = tail(resolve(s, e, C, M));
  if (resolvedTail != lambdax && !isTrivial(v)) { // Not trivial argument in application #e#
    dputs("    v (%O) == something not trivial", v);
    // rewriting rule: pp = (let ((p v) (s (var p))))
    // ==> M = (v (e ((p ((s (var p)) e)) k))))
    M = a(v, a(e, a(a(p, a(a(s, a(var, p)), e)), k)));
    return M;
  }

  dputs("    v (%O) == t trivial", v);
  Arrow t = v; // v == t trivial

    

  // Really trivial application
  
  // p == (t0 t1) where t0 should return a closure or such
  dputs("    p == (t0:v t1:s) # a really trivial application");
  Arrow t0 = s;
  Arrow Closure = resolve(t0, e, C, M);
  Arrow t1 = head(p);
  
  if (tail(Closure) == Closure) {
    ONDEBUG(fprintf(stderr, "info: %O is not a closure\n", Closure));
    // not a closure, one replaces it by a fake closure
    // /lambda/x/arrow/C.x == ((p (arrow (t0 p)) Eve)
    Closure = a(a(p,a(arrowOp,a(t0,p))),EVE);
    M = a(a(arrowOp, p), ek);
    return M;
  }

  // Continuation stacking not needed!

  if (tail(Closure) == operator) { // System call case
       // C == (operator (hook context))
       dputs("       C == (operator (hook context))");
       Arrow operatorParameter = head(head(Closure));
       XLCallBack hook;
       char* hooks = str(tail(head(Closure)));
       int n = sscanf(hooks, "%p", &hook);
       assert(n == 1);
       free(hooks);
       M = hook(a(Closure, M), operatorParameter);
       return M;
       
  } else { // closure case
       Arrow w;
       Arrow ee;
       if (tail(Closure) == lambdax) { // #e# special closure where applied arrow is not evaluated (like let)
         // C == (lambdax ((y ss) ee))
         dputs("        C == (lambdax ((y ss) ee))");
         
         // special closure : C == ("lambdax" CC) w/ CC being a regular closure
         // - applied arrow is escaped by default (like in let construct)
         Closure = head(Closure);
         ee = head(Closure);
         w = t1; // t1 is NOT resolved

       } else {
         // C == ((x ss) ee)
         dputs("        C == ((x ss) ee)");
         ee = head(Closure);
         w = resolve(t1, e, C, M);
       }
       Arrow x = tail(tail(Closure));
       Arrow ss = head(tail(Closure));
       M = a(ss, a(_load_binding(x, w, ee), k));
       return M;   
  }
}

Arrow xl_run(Arrow C, Arrow M) {
  machine_init();
  M = transition(C, M);
  while (!isEve(head(head(M))) || !isTrivial(tail(M))) {
    M = transition(C, M);
  }
  ONDEBUG((fprintf(stderr, "run finished with M = %O\n",M)));
  Arrow p = tail(M);
  Arrow e = tail(head(M));
  return resolve(p, e, C, M);
}

Arrow xl_eval(Arrow C /* ContextPath */, Arrow p /* program */) {
  machine_init();
  Arrow M = a(p, a(systemEnvironment, Eve()));
  return run(C /* ContextPath */, M);
}


Arrow xl_argInMachine(Arrow CM) {
  Arrow C = tailOf(CM);
  Arrow M = headOf(CM);
  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  Arrow e = tailOf(ek);
  Arrow arg;
  if (tail(p) == let) {
    // p == (let ((x (<operator> arg)) s))
    arg = head(head(tail(head(p))));
  } else {
    // p == (<operator> arg)
    arg = head(p);
  }
  ONDEBUG((fprintf(stderr, "   argument is %O\n", arg)));
  assert(isTrivial(arg));
  Arrow w = resolve(arg, e, C, M);
  return w;
}

Arrow xl_reduceMachine(Arrow CM, Arrow r) {
  Arrow M = headOf(CM);
  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  if (tail(p) == let) {
    // p == (let ((x (<operator> arg)) s))
    ONDEBUG((fprintf(stderr, "   let-application reduced to r = %O\n", r)));
    Arrow x = tail(tail(head(p)));
    Arrow s = head(head(p));
    ONDEBUG((fprintf(stderr, "     s = %O\n", s)));
    Arrow e = tail(ek);
    Arrow k = head(ek);
    ONDEBUG((fprintf(stderr, "     k = %O\n", k)));
    Arrow ne = _load_binding(x, r, e);
    M = a(s, a(ne, k));                                      
  } else {
    ONDEBUG((fprintf(stderr, "   application reduced to r = %O\n", r)));
    M = a(a(escape, r), ek);
  }
  return M;
}

Arrow runHook(Arrow CM, Arrow hookParameter) {
   return xl_argInMachine(CM);
}

Arrow tailOfHook(Arrow CM, Arrow hookParameter) {
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = tail(arrow);
   return xl_reduceMachine(CM, r);
}

Arrow headOfHook(Arrow CM, Arrow hookParameter) {
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = head(arrow);
   return xl_reduceMachine(CM, r);
}

Arrow childrenOfHook(Arrow CM, Arrow hookParameter) {
   Arrow parent = xl_argInMachine(CM);
   XLEnum e = childrenOf(parent);
   Arrow list = Eve();
   while (xl_enumNext(e)) {
      Arrow child = xl_enumGet(e);
      list = a(child, list);
   }
   xl_freeEnum(e);
   return xl_reduceMachine(CM, list);
}

Arrow rootHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_root(contextPath, arrow);
   return xl_reduceMachine(CM, r);
}

Arrow unrootHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_unroot(contextPath, arrow);
   return xl_reduceMachine(CM, r);
}

Arrow isRootedHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_isRooted(contextPath, arrow);
   return xl_reduceMachine(CM, r);
}

Arrow ifHook(Arrow CM, Arrow hookParameter) {
  Arrow M = headOf(CM);

  // the ifHook performs some magic
  Arrow body = xl_argInMachine(CM);
  Arrow condition = tail(body);
  Arrow alternative = head(body);
  Arrow branch;
  Arrow p = tail(CM);
  Arrow ek = head(CM);
  Arrow e = tail(ek);
  if (isEve(condition))
     branch = head(alternative);
  else
     branch = tail(alternative);

  if (tail(p) == let) {
    // p == (let ((x (if arg)) s))
    // rewritten in (let ((x branch) s))
    Arrow x = tail(tail(head(p)));
    Arrow s = head(head(p));
    p = a(let, a(a(x, branch), s));
    M = a(p, ek);
  } else {
    // p == (if arg)
    // rewritten in (branch)
    M = a(branch, ek);
  }
  return M;
}

Arrow commitHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow M = headOf(CM);
   xls_root(C, a(selfM, CM)); // TODO/FIXME fix this
   commit();
   xls_unroot(C, a(selfM, CM));
   return xl_reduceMachine(CM, Eve());
}

static struct fnMap_s {char *s; XLCallBack fn;} systemFns[] = {
 {"run", runHook},
 {"tailOf", tailOfHook},
 {"headOf", headOfHook},
 {"childrenOf", childrenOfHook},
 {"root", rootHook},
 {"unroot", unrootHook},
 {"isRooted", isRootedHook},
 {"if", ifHook},
 {"commit", commitHook},
 {NULL, NULL}
};

static void machine_init() {
  if (let) return;
  // environment = tag("environment");
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
    Arrow op = a(tag(systemFns[i].s), operator(systemFns[i].fn, EVE));
    systemEnvironment= a(op, systemEnvironment);
  }
  root(a(reserved, systemEnvironment));
}

void xl_init() {
  int rc = space_init();
  assert(rc >= 0);
}
