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
   continuation = 0, escalate = 0, selfM = 0, arrowWord = 0, swearWord= 0, brokenEnvironment = 0 ;

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
static Arrow _resolve(Arrow a, Arrow e, Arrow C, Arrow M);

/** unbuild an rebuild an arrow such as
  * any /var.x ancester is replaced by env(x)
  * /escape.x ancester by x
  * other atoms are leaved as is (no substitution by default)
 */
static Arrow _resolve_deeply(Arrow a, Arrow e, Arrow C, Arrow M) {
    // TODO: turn this call stack into machine states
    if (typeOf(a) == XL_ARROW) {
        Arrow t = tailOf(a);
        Arrow h = headOf(a);
        if (t == escape)
            return h;
        else if (t == var) {
            Arrow w = _resolve(a, e, C, M);
            if (w == NIL) w = EVE;
            return w;
        } else {
            Arrow rt = _resolve_deeply(t, e, C, M);
            Arrow rh = _resolve_deeply(h, e, C, M);
            return a(rt, rh);
        }
    } else {
        return a;
    }
}


static Arrow _resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
    DEBUGPRINTF("   _resolve a = %O", a);
    int type = typeOf(a);
    if (type == XL_EVE) return EVE;
    if (a == selfM) return M;

    Arrow x = a;
    if (type == XL_ARROW) {
        Arrow t = tailOf(a);
        if (t == swearWord || t == closure || t == paddock || t == operator) {
            return a; // naturally escaped / typed litteral
        } else if (t == arrowWord) {
            Arrow h = headOf(a);
            return _resolve_deeply(h, e, C, M);
        } else if (t == escape) {
            return headOf(a);
        } else if (t == lambda) {
            Arrow xs = headOf(a);
            return arrow(closure, arrow(xs, e));
        } else if (t == macro) {
            Arrow xs = headOf(a);
            return arrow(paddock, arrow(xs, e));
        }

        if (t == var) {
            x = headOf(a);
        }
    }
    // environment matching loop
    Arrow se = e;
    while (typeOf(se) == XL_ARROW) {
        Arrow b = tailOf(se);
        Arrow bx = tailOf(b);
        if (bx == x)
            return headOf(b);
        se = headOf(se);
    }
    if (se != EVE) {
         // Environnement is broken
        return brokenEnvironment;
    }

    return xls_get(C, x); // may be NIL
}

static Arrow resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
  DEBUGPRINTF("resolve a = %O in e = %O C = %O", a, e, C);
  Arrow w = _resolve(a, e, C, M);

  if (w == NIL) { // Not bound
      if (tailOf(a) == var) { // var.a
          w = EVE; // unbound var.x resolved to EVE
      } else {
          w = a; // unbound atom (not variable casted) is let as is
      }
  }

  DEBUGPRINTF("resolved in w = %O", w);
  return w;
}


static int isTrivial(Arrow s) {
  int type = typeOf(s);
  if (type != XL_ARROW) return 1; // an atomic arrow is always trivial
  Arrow t = tailOf(s);
  // TODO what if I used arrows for casting these keywords?
  if (t == swearWord || t == lambda || t == closure || t == macro || t == paddock || t == operator \
          || t == arrowWord || t == escape || t == var) return 1; // TODO closure,paddock,escape,arrowWord,var: as hooks
  return 0;
}

static int isTrivialOrBound(Arrow a, Arrow e, Arrow C, Arrow M, Arrow* w) {
    if (isTrivial(a)) {
        *w = resolve(a, e, C, M);
        return !0;
    } else {
        *w = _resolve(a, e, C, M);
        if (*w != NIL) {
            DEBUGPRINTF("Arrow %O is bound to %O !!", a, *w);
            return !0;
        }
        return 0;
    }
}

static int chainSize = 0; // TODO thread safe

static Arrow transition(Arrow C, Arrow M) { // M = (p, (e, k))
  DEBUGPRINTF("transition M = %O", M);

  Arrow p = tailOf(M); // program
  Arrow ins = tailOf(p); // let,load,eval,lambda,macro,... instruction
  Arrow ek = headOf(M);
  Arrow e = tailOf(ek);
  Arrow k = headOf(ek);
  Arrow w;
  DEBUGPRINTF("   p = %O\n   e = %O\n   k = %O", p, e, k);
  
  if (ins == load) { //load expression #e#
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
  
  if (ins == evalOp) { //eval expression
     // p == (eval s)
     dputs("p == (eval s)");
     Arrow s = head(p);

     chainSize++;
     M = a(s, a(e, a(evalOp, a(e, k)))); // stack an eval continuation
     return M;
  }
  
  if (ins == let) { //let expression family
      // p == (let ((x v) s))
      dputs("p == (let ((x v) s)) # let expression");
      Arrow v = head(tail(head(p)));
      Arrow x = tail(tail(head(p)));
      Arrow s = head(head(p));
      Arrow w;

      // FIXME x == "@M" case

      if (isTrivialOrBound(v, e, C, M, &w)) { // Trivial let expression
          // p == (let ((x v:t) s))
          dputs("     v (%O) == t trivial", v);
          if (w == brokenEnvironment)
              return a(swearWord,a(brokenEnvironment, M));

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
          chainSize++;
          M = a(v, a(e, a(a(x, a(s, e)), k)));
          return M;
      }

      Arrow v1 = head(v);


      if (v0 == evalOp) { // eval expression in containing let #e#
          // p == (let ((x (eval ss) s))
          dputs("     v == (eval ss) # an eval expression");
          Arrow ss = v1;
          chainSize++;
          M = a(ss, a(e, a(evalOp, a(e, a(a(x, a(s, e)), k))))); // stack an eval continuation
          return M;
      }

      Arrow w0 = NIL;
      Arrow w1 = NIL;

      if (!isTrivialOrBound(v0, e, C,  M, &w0)) { // non trivial expression in application in let expression #e#
          // p == (let ((x v:(v0 v1)) s)) where v0 not trivial
          dputs("p == (let ((x v:(v0 v1)) s)) where v0 not trivial");
          // rewriting program to stage s0 evaluation
          // TODO use (tailOf v) instead of p as variable name
          // pp = (let ((p v0) (let ((x ((var p) v1)) s))))
          Arrow pp = a(let, a(a(p, v0), a(let, a(a(x, a(a(var, p), v1)), s))));
          M = a(pp, ek);
          return M;
      }

      if (w0 == brokenEnvironment)
          return a(swearWord,a(brokenEnvironment, M));

      Arrow w0_type = tail(w0);

      if (!isTrivialOrBound(v1, e, C, M, &w1) && w0 != paddock) { // non trivial argument in application in let expression #e#
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

      if (w0_type == paddock && isTrivialOrBound(arrow(w0, t1), e, C, M, &w)) {
          M = a(w, a(evalOp, a(e, a(a(x, a(s, e)), k))));
      } else if (isTrivialOrBound(arrow(w0, w1), e, C, M, &w)) {
          if (isEve(x)) { // #e# direct environment load
              dputs("          direct environment load");
              M = a(s, a(_load_binding(tail(w), head(w), e), k));
          } else { // as simple as trivial application
              M = a(s, a(_load_binding(x, w, e), k));
          }
          return M;
      }

      if (w0_type == operator) { // System call special case
          // r(t0) = (operator (hook context))
          dputs("  r(t0) = (operator (hook context))");
          Arrow operatorParameter = head(head(w0));
          XLCallBack hook;
          char* hooks = str(tail(head(w0)));
          int n = sscanf(hooks, "%p", &hook);
          assert(n == 1);
          free(hooks);
          M = hook(a(C, M), operatorParameter);
          return M;

      } else if (w0_type == paddock || w0_type == closure) {
          // closure/paddock case
          Arrow yse = head(w0);
          Arrow ee = head(yse);
          Arrow y = tail(tail(yse));
          Arrow ss = head(tail(yse));
          if (w0_type == paddock) { // #e# paddock special closure
              // r(t0) == (paddock ((y ss) ee))
              dputs("  r(t0) == (paddock ((y ss) ee))");
              // applied arrow is not resolved

              // stacks up two continuation
              // the first is a normal continuation to eval paddock expression
              // second one is to eval expression result in caller closure
              // that is the evaluation of the expression after macro-substitution
              chainSize+=2;
              M = a(ss, a(_load_binding(y, t1, ee), a(evalOp, a(e, a(a(x, a(s, e)), k)))));
          } else {
              // r(t0) == (closure ((y ss) ee))
              dputs("  r(t0) == (closure ((y ss) ee))");
              if (w1 == brokenEnvironment)
                  return a(swearWord,a(brokenEnvironment, M));
              chainSize++;
              M = a(ss, a(_load_binding(y, w1, ee), a(a(x, a(s, e)), k))); // stacks up a continuation
          }

          return M;

      } else { // not a closure thing
          ONDEBUG(fprintf(stderr, "info: r(t0)=%O is not closure-like\n", w0));
          if (w == brokenEnvironment)
              return a(swearWord,a(brokenEnvironment, M));

          M = a(s, a(_load_binding(x, a(w0, w), e), k));
          return M;
      }
  } // let

  if (isTrivialOrBound(p, e, C, M, &w)) { // Trivial expression (including lambda expression)
     // p == v
     dputs("p == v trivial");

     // Let's unstack a continuation

     if (isEve(k)) return M; // stop, program result = resolve(v, e, M)

     if (tail(k) == continuation) { //special system continuation #e
        // TODO : could be operator or hook
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
       // k == (eval (ee kk))
       dputs("    k == (eval (ee kk))");
       Arrow eekk = head(k);
       Arrow ee = tail(eekk);
       Arrow kk = head(eekk);
       if (w == brokenEnvironment)
           return a(swearWord,a(brokenEnvironment, M));
       chainSize--;
       M = a(w, a(ee, kk)); // unstack continuation and reinject evaluation result as input expression
       return M;

     } else {
       // k == ((x (ss ee)) kk)
       dputs("    k == ((x (ss ee)) kk)");
       if (w == brokenEnvironment)
           return a(swearWord,a(brokenEnvironment, M));

       Arrow kk = head(k);
       Arrow f = tail(k);
       Arrow x = tail(f);
       Arrow ss = tail(head(f));
       Arrow ee = head(head(f));
       if (isEve(x)) { // #e : variable name is Eve (let ((Eve a) s1))
          dputs("variable name is Eve (let ((Eve a) s1))");
          // ones loads the arrow directly into environment. It will be considered as a var->value binding
          chainSize--;
          M = a(ss, a(_load_binding(tailOf(w), headOf(w), ee), kk)); // unstack continuation, postponed let solved
       } else {
          chainSize--;
          M = a(ss, a(_load_binding(x, w, ee), kk)); // unstack continuation, postponed let solved
       }
       return M;
     }
  }

  // application cases
  dputs("p == (s v)");
  Arrow s = tail(p);
  Arrow v = head(p);
  Arrow ws;

  if (!isTrivialOrBound(s, e, C, M, &ws)) { // Not trivial closure in application #e#
    dputs("p == (s v) where s is an application or such");
    // rewriting rule: pp = (let ((p s) ((var p) v)))
    // ==> M = (s (e ((p (((var p) v) e)) k)))
    M = a(s, a(e, a(a(p, a(a(a(var, p), v), e)), k)));
    return M;
  }

  if (ws == brokenEnvironment)
      return a(swearWord,a(brokenEnvironment, M));

  if (tail(v) == let) { // let expression as application argument #e#
      dputs("    p == (s v:(let ((x vv) ss))");
    // rewriting rule: pp = (let ((p v) (s (var p))))
    // ==> M = (v (e ((p ((s (var p)) e)) k))))
    M = a(v, a(e, a(a(p, a(a(a(escape,ws), a(var, p)), e)), k)));
    return M;
  }

  if (tail(v) == evalOp) { //eval expression as application argument #e#
     // p == (s (eval ss))
     dputs("     v == (eval ss) # an eval expression");
     Arrow ss = head(v);
     chainSize++;
     M = a(ss, a(e, a(evalOp, a(e, a(a(p, a(a(a(escape,ws), a(var, p)), e)), k))))); // stack an eval continuation
     return M;
  }

  Arrow ws_type = tail(ws);
  Arrow wv = NIL;
  if (!isTrivialOrBound(v, e, C, M, &wv) && ws_type != paddock) { // Not trivial argument in application #e#
    dputs("    v (%O) == something not trivial", v);
    // rewriting rule: pp = (let ((p v) (s (var p))))
    // ==> M = (v (e ((p ((s (var p)) e)) k))))
    chainSize++;
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

  if (ws_type == paddock && isTrivialOrBound(arrow(ws, t1), e, C, M, &w)) {
      M = a(w, a(evalOp, ek));
      return M;
  //} else if (isTrivialOrBound(arrow(ws, wv), e, C, M, &w)) {
  //    M = a(a(escape, w), ek);
  //    return M;
  }

  if (ws_type == operator) { // System call case
      // r(t0) == (operator (hook context))
      dputs("       r(t0) == /operator/hook.context (%O)", ws);
      Arrow operatorParameter = head(head(ws));
      XLCallBack hook;
      char* hooks = str(tail(head(ws)));
      int n = sscanf(hooks, "%p", &hook);
      assert(n == 1);
      free(hooks);
      M = hook(a(C, M), operatorParameter);
      return M;

  } else if (ws_type == paddock || ws_type == closure) {
      // closure/paddock case
      Arrow yse = head(ws);
      Arrow ee = head(yse);
      Arrow x = tail(tail(yse));
      Arrow ss = head(tail(yse));

      if (ws_type == paddock) { // #e# paddock special closure
          // r(t0) == (paddock ((x ss) ee))
          dputs("        r(t0) == (paddock ((x ss) ee))");
          wv = t1; // applied arrow is not evaluated (like in let construct)

          // stacks up one continuation to eval the expression after macro-substitution
          chainSize++;
          M = a(ss, a(_load_binding(x, wv, ee), a(evalOp, ek)));
      } else {
          // r(t0) == (closure ((x ss) ee))
          dputs("        r(t0) == (closure ((x ss) ee))");
          if (wv == brokenEnvironment)
              return a(swearWord,a(brokenEnvironment, M));
          //chainSize--;
          M = a(ss, a(_load_binding(x, wv, ee), k));
      }

      return M;

  } else {
      ONDEBUG(fprintf(stderr, "info: r(t0)=%O is not closure-like\n", ws));
      // not a closure, one let's the expression almost as if it was escaped
      if (w == brokenEnvironment)
          return a(swearWord,a(brokenEnvironment, M));
      else if (wv == NIL)
          wv = t1;
      M = a(a(escape, a(t0, wv)), ek);
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
  Arrow w;
  assert(isTrivialOrBound(arg, e, C, M, &w));
  resolve(arg, e, C, M);
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
      list = a(child, list);
   }
   xl_freeEnum(e);
   return xl_reduceMachine(CM, list);
}

Arrow rootHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_root(C, arrow);
   return xl_reduceMachine(CM, headOf(r)); // one doesn't show the context
}

Arrow unrootHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_unroot(contextPath, arrow);
   return xl_reduceMachine(CM, headOf(r)); // one doesn't show the context
}

Arrow setHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_set(C, xl_tailOf(arrow), xl_headOf(arrow));
   return xl_reduceMachine(CM, headOf(r)); // one doesn't show the context
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
   return xl_reduceMachine(CM, headOf(r)); // no context
}

Arrow ifHook(Arrow CM, Arrow hookParameter) {
  // the ifHook performs some magic
  Arrow condition = xl_argInMachine(CM);
  Arrow branch = xl_operator(isEve(condition) ? headOfHook: tailOfHook, EVE);

  return xl_reduceMachine(CM, branch);
}

Arrow equalHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xl_equal(tailOf(arrow), headOf(arrow)) ? tailOf(arrow) : EVE;
   return xl_reduceMachine(CM, r);
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
  arrowWord = tag("arrow");
  escalate = tag("escalate");
  swearWord = tag("&!#");
  brokenEnvironment = arrow(swearWord, tag("broken environment"));

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
  root(a(locked, arrowWord));
  root(a(locked, escalate));
  root(a(locked, swearWord));
  root(a(locked, brokenEnvironment));

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
      {"equalHook", equalHook},
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
      xls_set(EVE, tag("if"), xl_uri("/paddock//x/let//condition/tailOf.x/let//alternative/headOf.x/arrow/eval//ifHook/var.condition//escape.escape/var.alternative."));
  if (xls_get(EVE, tag("equal")) == NIL)
      xls_set(EVE, tag("equal"), xl_uri("/paddock//x/let//a/tailOf.x/let//b/headOf.x/arrow/let///tailOf/var.x/var.a/let///headOf/var.x/var.b/equalHook/arrow///escape.var/tailOf/var.x//escape.var/headOf/var.x."));
  if (xls_get(EVE, tag("get")) == NIL)
      xls_set(EVE, tag("get"), xl_uri("/paddock//x/arrow/getHook//escape.escape/var.x."));
  if (xls_get(EVE, tag("unset")) == NIL)
      xls_set(EVE, tag("unset"), xl_uri("/paddock//x/arrow/unsetHook//escape.escape/var.x."));
  if (xls_get(EVE, tag("set")) == NIL)
      xls_set(EVE, tag("set"), xl_uri("/paddock//x/let//slot/tailOf.x/let//exp/headOf.x/arrow/let///headOf/var.x/var.exp/setHook/arrow///escape.escape/var.slot//escape.var/headOf/var.x."));
}

Arrow xl_run(Arrow C, Arrow M) {
  machine_init();
  // M = //p/e.k
  chainSize = 0;
  Arrow w;
  while (tail(M) != swearWord && (!isTrivialOrBound(tail(M) /*p*/, tail(head(M)) /*e*/, C, M, &w) || /*k*/head(head(M)) != EVE)) {
    M = transition(C, M);

    if (chainSize > 500) {
        M = arrow(swearWord, arrow(arrow(swearWord, tag("too long continuation chain")), M));
        break;
    }

    // only operators can produce such a state
    while (head(M) == escalate && M != escalate) {
        C = tail(C);
        LOGPRINTF(LOG_WARN, "machine context escalate to %O", C);
        M = tail(M);
    }
  }

  if (tail(M) == swearWord) {
      DEBUGPRINTF("run finished with error : %O", head(M));
      return tail(head(M));
  }

  DEBUGPRINTF("run finished with M = %O", M);
  if (w == NIL) w = EVE;
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
