#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "space.h"
#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "session.h"
#define LOG_CURRENT LOG_MACHINE
#include "log.h"

static Arrow let = 0, load = 0, environment = 0, escape = 0, var = 0,
   evalOp = 0, lambda = 0, macro = 0, closure = 0, paddock = 0, operator = 0,
   continuation = 0, escalate = 0, selfM = 0, arrowOp = 0;

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
    Arrow temp = EVE;
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
    DEBUGPRINTF("   _resolve a = %O", a);
    int type = typeOf(a);
    if (type == XL_EVE) return EVE;
    if (a == selfM) return M;

    Arrow x = a;
    if (type == XL_ARROW) {
        Arrow t = tailOf(a);
        if (t == closure || t == paddock || t == operator) {
            return a; // naturally escaped / typed litteral
        } else if (t == arrowOp) {
            Arrow t = headOf(a);
            Arrow t1 = tailOf(t);
            Arrow t2 = headOf(t);
            return a(_resolve(t1, e, C, M), _resolve(t2, e, C, M));
        } else if (t == escape) {
            return headOf(a);
        } else if (t == lambda) {
            Arrow xs = headOf(a);
            return arrow(closure, arrow(xs, e));
        } else if (t == macro) {
            Arrow xs = headOf(a);
            return arrow(paddock, arrow(xs, e));
        } else if (t == var) {
            x = headOf(a);
        } else {
            return a;
        }
    }
    // environment matching loop
    Arrow se = e;
    while (se != EVE) {
        Arrow b = tailOf(se);
        Arrow bx = tailOf(b);
        if (bx == x)
            return headOf(b);
        se = headOf(se);
    }

    Arrow value = xls_get(C, x);
    if (value != NIL)
        return value;

    return x; // an unbound variable is kept as is
}

static Arrow resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
  DEBUGPRINTF("resolve a = %O in e = %O C = %O", a, e, C);
  Arrow w = _resolve(a, e, C, M);
  DEBUGPRINTF("resolved in w = %O", w);
  return w;
}


static int isTrivial(Arrow s) {
  int type = typeOf(s);
  if (type != XL_ARROW) return 1; // true
  Arrow t = tailOf(s);
  // TODO what if I used arrows for casting these keywords?
  if (t == lambda || t == closure || t == macro || t == paddock || t == operator \
          || t == arrowOp || t == escape || t == var) return 1; // TODO closure,paddock,escape,arrowOp,var: as hooks
  return 0;
}


static Arrow transition(Arrow C, Arrow M) { // M = (p, (e, k))
  DEBUGPRINTF("transition M = %O", M);

  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  Arrow e = tailOf(ek);
  Arrow k = headOf(ek);
  DEBUGPRINTF("   p = %O\n   e = %O\n   k = %O", p, e, k);
  
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
     Arrow pp = a(let, a(a(EVE, s0), s1));
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


      if (!isTrivial(v0)) { // non trivial expression in application in let expression #e#
          // p == (let ((x v:(v0 v1)) s)) where v0 not trivial
          dputs("p == (let ((x v:(v0 v1)) s)) where v0 not trivial");
          // rewriting program to stage s0 evaluation
          // TODO use (tailOf v) instead of p as variable name
          // pp = (let ((p v0) (let ((x ((var p) v1)) s))))
          Arrow pp = a(let, a(a(p, v0), a(let, a(a(x, a(a(var, p), v1)), s))));
          M = a(pp, ek);
          return M;
      }

      Arrow resolved_s = resolve(v0, e, C, M);
      Arrow resolved_s_type = tail(resolved_s);

      if (resolved_s != paddock && !isTrivial(v1)) { // non trivial argument in application in let expression #e#
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


      if (resolved_s_type == operator) { // System call special case
          // r(t0) = (operator (hook context))
          dputs("  r(t0) = (operator (hook context))");
          Arrow operatorParameter = head(head(resolved_s));
          XLCallBack hook;
          char* hooks = str(tail(head(resolved_s)));
          int n = sscanf(hooks, "%p", &hook);
          assert(n == 1);
          free(hooks);
          M = hook(a(C, M), operatorParameter);
          return M;

      } else if (resolved_s_type == paddock || resolved_s_type == closure) {
          // closure/paddock case
          Arrow yse = head(resolved_s);
          Arrow ee = head(yse);
          Arrow y = tail(tail(yse));
          Arrow ss = head(tail(yse));

          Arrow w;
          if (resolved_s_type == paddock) { // #e# paddock special closure
              // r(t0) == (paddock ((y ss) ee))
              dputs("  r(t0) == (paddock ((y ss) ee))");
              w = t1; // applied arrow is not resolved
          } else {
              // r(t0) == (closure ((y ss) ee))
              dputs("  r(t0) == (closure ((y ss) ee))");
              w = resolve(t1, e, C, M);
          }

          M = a(ss, a(_load_binding(y, w, ee), a(a(x, a(s, e)), k))); // stacks up a continuation
          return M;

      } else { // not a closure thing
          ONDEBUG(fprintf(stderr, "info: r(t0)=%O is not closure-like\n", resolved_s));
          Arrow w = resolve(t1, e, C, M);
          M = a(s, a(_load_binding(x, a(resolved_s, w), e), k));
          return M;
      }
  } // let

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

  Arrow resolved_s = resolve(s, e, C, M);
  Arrow resolved_s_type = tail(resolved_s);
  if (resolved_s_type != paddock && !isTrivial(v)) { // Not trivial argument in application #e#
    dputs("    v (%O) == something not trivial", v);
    // rewriting rule: pp = (let ((p v) (s (var p))))
    // ==> M = (v (e ((p ((s (var p)) e)) k))))
    M = a(v, a(e, a(a(p, a(a(s, a(var, p)), e)), k)));
    return M;
  }

  // v == t trivial
  // Really trivial application
  // Continuation stacking not needed!

  // p == (t0 t1) where t0 should return a closure or such
  dputs("    p == (t0:s t1:v) # a really trivial application");
  Arrow t0 = s;
  Arrow t1 = v;

  if (resolved_s_type == operator) { // System call case
      // r(t0) == (operator (hook context))
      dputs("       r(t0) == /operator/hook.context (%O)", resolved_s);
      Arrow operatorParameter = head(head(resolved_s));
      XLCallBack hook;
      char* hooks = str(tail(head(resolved_s)));
      int n = sscanf(hooks, "%p", &hook);
      assert(n == 1);
      free(hooks);
      M = hook(a(C, M), operatorParameter);
      return M;

  } else if (resolved_s_type == paddock || resolved_s_type == closure) {
      // closure/paddock case
      Arrow yse = head(resolved_s);
      Arrow ee = head(yse);
      Arrow x = tail(tail(yse));
      Arrow ss = head(tail(yse));

      Arrow w;
      if (resolved_s_type == paddock) { // #e# paddock special closure
          // r(t0) == (paddock ((x ss) ee))
          dputs("        r(t0) == (paddock ((x ss) ee))");
          w = t1; // applied arrow is not evaluated (like in let construct)
      } else {
          // r(t0) == (closure ((x ss) ee))
          dputs("        r(t0) == (closure ((x ss) ee))");
          w = resolve(t1, e, C, M);
      }

      M = a(ss, a(_load_binding(x, w, ee), k));
      return M;

  } else {
      ONDEBUG(fprintf(stderr, "info: r(t0)=%O is not closure-like\n", resolved_s));
      // not a closure, one replaces it by a fake closure
      // /lambda/x/arrow/C.x == (closure ((p (arrow (s p)) Eve))
      resolved_s = a(closure,a(a(p,a(arrowOp,a(s,p))),EVE));
      M = a(a(arrowOp, p), ek);
      return M;
  }

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
  assert(isTrivial(arg));
  Arrow w = resolve(arg, e, C, M);
  DEBUGPRINTF("   argument is %O resolved in %O", arg, w);
  return w;
}

Arrow xl_reduceMachine(Arrow CM, Arrow r) {
  Arrow M = headOf(CM);
  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  if (tail(p) == let) {
    // p == (let ((x (<operator> arg)) s))
    DEBUGPRINTF("   let-application reduced to r = %O", r);
    Arrow x = tail(tail(head(p)));
    Arrow s = head(head(p));
    DEBUGPRINTF("     s = %O", s);
    Arrow e = tail(ek);
    Arrow k = head(ek);
    DEBUGPRINTF("     k = %O", k);
    Arrow ne = _load_binding(x, r, e);
    M = a(s, a(ne, k));                                      
  } else {
    DEBUGPRINTF("   application reduced to r = %O", r);
    M = a(a(escape, r), ek);
  }
  return M;
}

Arrow runHook(Arrow CM, Arrow hookParameter) {
   Arrow M = xl_argInMachine(CM);
   if (headOf(M) == escalate) // come on
       return Eve;

   return M;
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
   if (!e) return xl_reduceMachine(CM, EVE);

   Arrow list = EVE;
   while (xl_enumNext(e)) {
      Arrow child = xl_enumGet(e);
      list = a(list, child);
   }
   xl_freeEnum(e);
   return xl_reduceMachine(CM, list);
}

Arrow rootHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_root(C, arrow);
   return xl_reduceMachine(CM, r);
}

Arrow unrootHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_unroot(contextPath, arrow);
   return xl_reduceMachine(CM, r);
}

Arrow setHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_set(C, xl_tailOf(arrow), xl_headOf(arrow));
   return xl_reduceMachine(CM, r);
}

Arrow unsetHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   xls_unset(contextPath, arrow);
   return xl_reduceMachine(CM, arrow);
}

Arrow getHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_get(C, arrow);
   return xl_reduceMachine(CM, (r == NIL ? EVE : r)); // TODO: "throwing" an error?
}

Arrow isRootedHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_isRooted(contextPath, arrow);
   return xl_reduceMachine(CM, r);
}

Arrow ifHook(Arrow CM, Arrow hookParameter) {
  Arrow C = tailOf(CM);
  Arrow M = headOf(CM);

  // the ifHook performs some magic
  Arrow body = xl_argInMachine(CM);
  Arrow condition = tail(body);
  Arrow alternative = head(body);
  Arrow branch;
  Arrow p = tail(M);
  Arrow ek = head(M);
  Arrow e = tail(ek);
  Arrow w = resolve(condition, e, C, M);
  if (isEve(w))
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
   return xl_reduceMachine(CM, EVE);
}

Arrow escalateHook(Arrow CM, Arrow hookParameter) {
    Arrow C = tailOf(CM);
    Arrow M = headOf(CM);
    char* uri = uriOf(C);
    Arrow secret = xl_argInMachine(CM);
    char* secret_s = str(secret);
    char  try_s[256];
    snprintf(try_s, 255, "%s=%s", uri, secret_s);
    LOGPRINTF(LOG_WARN, "escalate attempt");
    char* server_secret_s = getenv("ENTRELACS_SECRET"); // TODO better solution
    if (!server_secret_s) server_secret_s = "chut";
    if (isRooted(a(tag(server_secret_s), tag(try_s))))
        // success
        M = a(xl_reduceMachine(CM, EVE), escalate);
    else
        M = xl_reduceMachine(CM, EVE);
    free(secret_s);
    free(uri);
    return M;
}


static void machine_init() {
  if (let) return;

  // initialize key arrows
  // environment = tag("environment");
  let = tag("let");
  load = tag("load");
  var = tag("var");
  escape = tag("escape");
  evalOp = tag("eval");
  lambda = tag("lambda");
  macro = tag("macro");
  closure = tag("closure");
  paddock = tag("paddock"); // closure for macro
  operator = tag("operator");
  continuation = tag("continuation");
  selfM = tag("@M");
  arrowOp = tag("arrow");
  escalate = tag("escalate");

  // preserve keyarrows from GC
  Arrow locked = tag("locked");
  root(a(locked, let));
  root(a(locked, load));
  root(a(locked, var));
  root(a(locked, escape));
  root(a(locked, evalOp));
  root(a(locked, lambda));
  root(a(locked, macro));
  root(a(locked, closure));
  root(a(locked, paddock));
  root(a(locked, operator));
  root(a(locked, continuation));
  root(a(locked, selfM));
  root(a(locked, arrowOp));
  root(a(locked, escalate));

  // root basic operators into the global context
  static struct fnMap_s {char *s; XLCallBack fn;} systemFns[] = {
      {"run", runHook},
      {"tailOf", tailOfHook},
      {"headOf", headOfHook},
      {"childrenOf", childrenOfHook},
      {"root", rootHook},
      {"unroot", unrootHook},
      {"isRooted", isRootedHook},
      {"setHook", setHook},
      {"unsetHook", unsetHook},
      {"getHook", getHook},
      {"ifHook", ifHook},
      {"commit", commitHook},
      {"escalate", escalateHook},
      {NULL, NULL}
  };

  for (int i = 0; systemFns[i].s != NULL ; i++) {
    Arrow operatorKey = tag(systemFns[i].s);
    if (xls_get(EVE, operatorKey) != NIL) continue; // already set
    xls_set(EVE,operatorKey, operator(systemFns[i].fn, EVE));
  }

  if (xls_get(EVE, tag("if")) == NIL)
      xls_set(EVE, tag("if"), xl_uri("/paddock//x/let//condition/tailOf.x/let//alternative/headOf.x/let//value/eval.condition/ifHook/arrow/value.alternative.."));
  if (xls_get(EVE, tag("get")) == NIL)
      xls_set(EVE, tag("get"), xl_uri("/paddock//x/getHook.x."));
  if (xls_get(EVE, tag("unset")) == NIL)
      xls_set(EVE, tag("unset"), xl_uri("/paddock//x/unsetHook.x."));
  if (xls_get(EVE, tag("set")) == NIL)
      xls_set(EVE, tag("set"), xl_uri("/paddock//x/let//slot/tailOf.x/let//exp/headOf.x/let//value/eval.exp/setHook/arrow/slot.value.."));
}

Arrow xl_run(Arrow C, Arrow M) {
  machine_init();
  // M = //p/e.k
  while (/*k*/head(head(M)) != EVE || !isTrivial(/*p*/tail(M))) {
    M = transition(C, M);
    // only operators can produce such a state
    while (head(M) == escalate && M != escalate) {
        C = tail(C);
        LOGPRINTF(LOG_WARN, "machine context escalate to %O", C);
        M = tail(M);
    }
  }
  DEBUGPRINTF("run finished with M = %O", M);
  Arrow p = tail(M);
  Arrow e = tail(head(M));
  Arrow w = resolve(p, e, C, M);
  DEBUGPRINTF("run result is %O", w);
  return w;
}

Arrow xl_eval(Arrow C /* ContextPath */, Arrow p /* program */) {
  DEBUGPRINTF("cl_eval C=%O p=%O", C, p);
  machine_init();
  Arrow M = a(p, a(EVE, EVE));
  return run(C /* ContextPath */, M);
}


void xl_init() {
  int rc = space_init();
  assert(rc >= 0);
}
