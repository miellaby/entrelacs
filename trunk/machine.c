#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "session.h"
#define LOG_CURRENT LOG_MACHINE
#include "log.h"
#include "sha1.h"

static struct s_machine_stats {
    int transition;
    int i1, i2, i3, i4, i5, i6, i7, i8, i9;
} machine_stats_zero = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}, machine_stats = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static Arrow let = 0, load = 0, environment = 0, escape = 0, var = 0, comma = 0, it = 0,
   evalOp = 0, lambda = 0, macro = 0, closure = 0, paddock = 0, rlambda = 0, operator = 0,
   continuation = 0, fall = 0, escalate = 0, selfM = 0, arrowWord = 0, swearWord= 0, brokenEnvironment = 0,
        tempVar, tailOfOperator = EVE, headOfOperator = EVE;

static void machine_init(Arrow);

Arrow tmp(Arrow M) {
  char memRef[64]; 
  snprintf(memRef, 64, "%d", (int)M);
  return a(tempVar, atom(memRef));
}

Arrow xl_operator(XLCallBack hookp, Arrow context) {
  return a(operator, a(xl_hook(hookp), context));
}

Arrow xl_continuation(XLCallBack hookp, Arrow context) {
  return a(continuation, a(xl_hook(hookp), context));
}

/** Load a binding into an environment arrow (list arrow), trying to remove a previous binding for this variable to limit its size */
static Arrow _load_binding(Arrow x, Arrow w, Arrow e) {
    // typically a(a(a(x, w), e)
    int i;
    Arrow et = e;
    Arrow temp = EVE;
#define MAX_SEARCH_PREVIOUS_BINDING 10
    return a(a(x, w), e);

    //
    // BAD IDEA ! :O !!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
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
  * any /var+x ancester is replaced by env(x)
  * /escape+x ancester by x
  * other atoms are leaved as is (no substitution by default)
 */
static Arrow _resolve_deeply(Arrow a, Arrow e, Arrow C, Arrow M) {
    // TODO: turn this call stack into machine states
    if (isPair(a)) {
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
    if (a == EVE) return EVE;
    if (a == selfM) return M;

    Arrow x = a;
    if (isPair(a)) {
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
            return pair(closure, pair(xs, e));
        } else if (t == rlambda) {
            Arrow xs = headOf(a);
            return pair(closure, pair(EVE, pair(xs, e))); // recursive closure
        } else if (t == macro) {
            Arrow xs = headOf(a);
            return pair(paddock, pair(xs, e));
        }

        if (t == var) {
            x = headOf(a);
        }
    }
    // environment matching loop
    Arrow se = e;
    while (isPair(se)) {
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
  TRACEPRINTF("resolve a = %O in e = %O C = %O", a, e, C);
  Arrow w = _resolve(a, e, C, M);

  if (w == NIL) { // Not bound
      if (tailOf(a) == var) { // var+a
          w = EVE; // unbound var+x resolved to EVE
      } else {
          w = a; // unbound atom (not variable casted) is let as is
      }
  }

  TRACEPRINTF("resolved in w = %O", w);
  return w;
}


static int isTrivial(Arrow s) {
  if (isAtom(s)) return 1; // an atomic arrow is always trivial
  Arrow t = tailOf(s);
  // TODO what if I used arrows for casting these keywords?
  if (t == swearWord || t == lambda || t == closure || t == macro || t == paddock || t == operator \
          || t == arrowWord || t == escape || t == var) return 1; // TODO closure,paddock,escape,arrowWord,var: as hooks
  return 0;
}

static int isTrivialOrBound(Arrow a, Arrow e, Arrow C, Arrow M, Arrow* w) {
    if (isTrivial(a)) {
        *w = resolve(a, e, C, M);
        TRACEPRINTF("isTrivialOrBound(%O) : is trivial and resolved to %O !!", a, *w);
        return (*w != NIL);
    } else {
        *w = _resolve(a, e, C, M);
        if (*w != NIL) {
            TRACEPRINTF("Arrow %O is bound to %O !!", a, *w);
            return !0;
        }
        return 0;
    }
}

static int chainSize = 0; // TODO thread safe

static Arrow transition(Arrow C, Arrow M) { // M = (p, (e, k))
  TRACEPRINTF("transition M = %O", M);

  Arrow p = tailOf(M); // program
  Arrow ins = tailOf(p); // let,load,eval,lambda,macro,... instruction
  Arrow param = headOf(p);
  Arrow ek = headOf(M);
  Arrow e = tailOf(ek);
  Arrow k = headOf(ek);
  Arrow w;

  TRACEPRINTF("=== P %O ===\n   e = %O\n   k = %O", p, e, k);
  machine_stats.transition++;

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
      dputs("   x = %O\n   v = %O\n   s= %O", x, v, s);

      // FIXME x == "@M" case

      if (isTrivialOrBound(v, e, C, M, &w)) { // Trivial let expression
          // p == (let ((x v:t) s))
          dputs("     v (%O) == t trivial", v);

          // FIXME: if v == /p+v where p resolve to a paddock, one should eval the bound value
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
          // pp = (let ((tmp v0) (let ((x ((var tmp) v1)) s))))
          Arrow pp = a(let, a(a(tmp(M), v0), a(let, a(a(x, a(a(var, tmp(M)), v1)), s))));
          M = a(pp, ek);
          return M;
      }

      if (w0 == brokenEnvironment)
          return a(swearWord,a(brokenEnvironment, M));

      Arrow w0_type = tail(w0);

      if (!isTrivialOrBound(v1, e, C, M, &w1) && w0_type != paddock) {
          // non trivial argument in application in let expression #e# 
          // p == (let ((x v:(t0 v1)) s)) where v1 not trivial
    	    dputs("p == (let ((x v:(t0 v1)) s)) where v1 not trivial");

          // rewriting program to stage v1 and (v0 v1) evaluation
          // pp = (let ((tmp v1) (let ((x (v0 (var tmp))) s))))
          // TODO use (headOf v) instead of p as variable name
          Arrow pp = a(let, a(a(tmp(M), v1), a(let, a(a(x, a(v0, a(var, tmp(M)))), s))));
          M = a(pp, ek);
          return M;
      }
 
      // p == (let ((x (t0 t1)) s)) where t0 is a closure or equivalent
      dputs("p == (let ((x v:(v0:t0 v1:t1) s))  # a really trivial application in let expression");
      Arrow t0 = v0;
      Arrow t1 = v1;

      if (w0_type == paddock && isTrivialOrBound(pair(t0, t1), e, C, M, &w)) {
	        dputs("   t0 is bound to a paddock w0 and /w0+t1 is bound to %O, one evals this expression", w); 
          // FIXME : pair(t0, t1) is v+ See FIXME above
          M = a(w, a(evalOp, a(e, a(a(x, a(s, e)), k))));
	        return M;
      } else if (w0_type != paddock && isTrivialOrBound(pair(t0, w1), e, C, M, &w)) {

          dputs("   /t0 is not bound to a paddock and /t0+resolve(t1) is bound to %O", w);

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
          dputs("  resolve(t0) = (operator (hook context))");
          Arrow operatorParameter = head(head(w0));
          XLCallBack cb  = pointer(tail(head(w0)));
          assert(cb);
          M = cb(a(C, M), operatorParameter);
          return M;

      } else if (w0_type == paddock || w0_type == closure) {
          dputs("    w0_type = %O", w0_type);
          Arrow yse = head(w0);
          Arrow ee = head(yse);
          Arrow ys = tail(yse);
          int recursive = (ys == EVE);
          if (recursive) {
              yse = head(yse);
              ys = tail(yse);
          }
          Arrow y = tail(tail(yse));
          Arrow ss = head(tail(yse));
          if (w0_type == paddock) { // #e# paddock special closure
              // r(t0) == (paddock ((y ss) ee))
              dputs("  resolve(t0) == %O", w0);
              // applied arrow is not resolved

              // stacks up two continuation
              // the first is a normal continuation to eval paddock expression
              // second one is to eval expression result in caller closure
              // that is the evaluation of the expression after macro-substitution
              chainSize+=2;
              ee = _load_binding(y, t1, ee);
              if (recursive)
                   ee = _load_binding(it, w0, ee);
              M = a(ss, a(ee, a(evalOp, a(e, a(a(x, a(s, e)), k)))));
          } else {
              // r(t0) == (closure ((y ss) ee))
              dputs("  resolve(t0) == %O", w0);
              if (w1 == brokenEnvironment)
                  return a(swearWord,a(brokenEnvironment, M));
              chainSize++;
              ee = _load_binding(y, w1, ee);
              if (recursive)
                   ee = _load_binding(it, w0, ee);
              M = a(ss, a(ee, a(a(x, a(s, e)), k))); // stacks up a continuation
          }

          return M;

      } else { // not a closure thing
          TRACEPRINTF("resolve(t0)=%O is not closure-like\n", w0);
          if (w == brokenEnvironment)
              return a(swearWord,a(brokenEnvironment, M));

          M = a(s, a(_load_binding(x, a(w0, w1), e), k));
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
        XLCallBack cb = pointer(tail(head(k)));
        assert(cb);
        M = cb(a(C, M), continuationParameter);
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


  if (xl_isPair(v) && tailOf(v) == comma) {
      dputs("p == (s (, next))");
      // TODO: right-paddock to emulate this
      //  <==> (let (it s) next)
      Arrow next = headOf(v);
      if (next == comma)
          return a(s, ek);
      // k = ((it (next e)) k)
      chainSize++;
      return a(s, a(e, a(a(it, a(next, e)), k)));
  }


  if (!isTrivialOrBound(s, e, C, M, &ws)) { // Not trivial closure in application #e#
    dputs("p == (s v) where s is not trivial");
    // rewriting rule: pp = (let ((tmp s) ((var tmp) v)))
    // ==> M = (s (e ((tmp (((var tmp) v) e)) k)))
    M = a(s, a(e, a(a(tmp(M), a(a(a(var, tmp(M)), v), e)), k)));
    return M;
  }

  if (ws == brokenEnvironment)
      return a(swearWord,a(brokenEnvironment, M));

  if (tail(v) == let) { // let expression as application argument #e#
      dputs("    p == (s v:(let ((x vv) ss))");
    // rewriting rule: pp = (let ((tmp v) (s (var tmp))))
    // ==> M = (v (e ((tmp ((s (var tmp)) e)) k))))
    M = a(v, a(e, a(a(tmp(M), a(a(a(escape, ws), a(var, tmp(M))), e)), k)));
    return M;
  }

  if (tail(v) == evalOp) { //eval expression as application argument #e#
     // p == (s (eval ss))
     dputs("     v == (eval ss) # an eval expression");
     Arrow ss = head(v);
     chainSize++;
     M = a(ss, a(e, a(evalOp, a(e, a(a(M, a(a(a(escape,ws), a(var, M)), e)), k))))); // stack an eval continuation
     return M;
  }

  Arrow ws_type = tail(ws);
  Arrow wv = NIL;
  if (!isTrivialOrBound(v, e, C, M, &wv) && ws_type != paddock) { // Not trivial argument in application #e#
    dputs("    v (%O) == something not trivial", v);
    // rewriting rule: pp = (let ((tmp v) (s (var tmp))))
    // ==> M = (v (e ((tmp ((s (var tmp)) e)) k))))
    chainSize++;
    M = a(v, a(e, a(a(tmp(M), a(a(s, a(var, tmp(M))), e)), k)));
    return M;
  }

  // v == t trivial
  // Really trivial application
  // Continuation stacking not needed!

  // p == (t0 t1) where t0 should return a closure or such
  dputs("    p == (t0:s t1:v) # a really trivial application");
  Arrow t0 = s;
  Arrow t1 = v;
  if (ws_type == paddock && isTrivialOrBound(pair(s, t1), e, C, M, &w)) {
      dputs("s is bound to a paddock and /s+t1 is bound to %O, one evals the expression", w);
      M = a(w, a(evalOp, ek));
      return M;
  } else if (ws_type != paddock && isTrivialOrBound(pair(s, wv), e, C, M, &w)) {
      dputs("/s+resolve(v) is bound to something and s is not bound to a paddock!");
      M = a(a(escape, w), ek);
      return M;
  }

  if (ws_type == operator) { // System call case
      // resolve(t0) == (operator (hook context))
      dputs("       resolve(t0) == /operator/hook+context (%O)", ws);
      Arrow operatorParameter = head(head(ws));
      XLCallBack cb = pointer(tail(head(ws)));
      assert(cb);
      M = cb(a(C, M), operatorParameter);
      return M;

  } else if (ws_type == paddock || ws_type == closure) {
      // closure/paddock case
      Arrow yse = head(ws);
      Arrow ys = tail(yse);
      int recursive = (ys == EVE);
      if (recursive) {
          yse = head(yse);
          ys = tail(yse);
      }
      Arrow ee = head(yse);
      Arrow x = tail(tail(yse));
      Arrow ss = head(tail(yse));

      if (ws_type == paddock) { // #e# paddock special closure
          // r(t0) == (paddock ((x ss) ee))
          dputs("        resolve(t0) == (paddock ((x ss) ee))");
          wv = t1; // applied arrow is not evaluated (like in let construct)

          // stacks up one continuation to eval the expression after macro-substitution
          chainSize++;
          ee = _load_binding(x, wv, ee);
          if (recursive)
               ee = _load_binding(it, ws, ee);
          M = a(ss, a(ee, a(evalOp, ek)));
      } else {
          // r(t0) == (closure ((x ss) ee))
          dputs("        resolve(t0) == (closure ((x ss) ee))");
          if (wv == brokenEnvironment)
              return a(swearWord,a(brokenEnvironment, M));
          //chainSize--;
          ee = _load_binding(x, wv, ee);
          if (recursive)
               ee = _load_binding(it, ws, ee);
          M = a(ss, a(ee, k));
      }

      return M;

  } else {
      TRACEPRINTF("info: resolve(t0)=%O is not closure-like\n", ws);
      // not a closure, one let's the expression almost as if it was escaped
      if (w == brokenEnvironment)
          return a(swearWord,a(brokenEnvironment, M));
      else if (wv == NIL)
          wv = t1;
      M = a(a(escape, a(ws, wv)), ek);
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
  int bound = isTrivialOrBound(arg, e, C, M, &w);
  assert(bound);
  resolve(arg, e, C, M);
  TRACEPRINTF("   argument is %O resolved in %O", arg, w);
  return w;
}

Arrow xl_reduceMachine(Arrow CM, Arrow r) {
  Arrow M = headOf(CM);
  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  if (tail(p) == let) {
    // p == (let ((x (<operator> arg)) s))
    TRACEPRINTF("   let-application reduced to r = %O", r);
    Arrow x = tail(tail(head(p)));
    Arrow s = head(head(p));
    TRACEPRINTF("     s = %O", s);
    Arrow e = tail(ek);
    Arrow k = head(ek);
    TRACEPRINTF("     k = %O", k);
    Arrow ne = _load_binding(x, r, e);
    M = a(s, a(ne, k));                                      
  } else {
    TRACEPRINTF("   application reduced to r = %O", r);
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

Arrow childrenReviewOfHook(Arrow CM, Arrow hookParameter) {
    Arrow parent = xl_argInMachine(CM);
    XLEnum e;
    if (hookParameter == EVE) {
        e = childrenOf(parent);
        return xl_reduceMachine(CM, operator(childrenReviewOfHook, xl_hook(e)));
    }

    e = pointer(hookParameter);
    Arrow child;
    if (!e) {
        child = EVE;
    } else if (xl_enumNext(e)) {
        child = xl_enumGet(e);
    } else {
        xl_freeEnum(e); // FIXME : only on forget
        xl_unroot(hookParameter); // should made it unreadable
        child = EVE;
    }
    return xl_reduceMachine(CM, child);
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

Arrow partnerOfHook(Arrow CM, Arrow hookParameter) {
    Arrow C = tailOf(CM);
    Arrow arrow = xl_argInMachine(CM);
    Arrow list = xls_partnerOf(C, arrow);
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

Arrow setTailWithHeadInHook(Arrow CM, Arrow hookParameter) {
   Arrow C = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xls_set(C, xl_tailOf(arrow), xl_headOf(arrow));
   return xl_reduceMachine(CM, headOf(r)); // one doesn't show the context
}

Arrow unsetVarHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   xls_unset(contextPath, arrow);
   return xl_reduceMachine(CM, arrow);
}

Arrow getVarHook(Arrow CM, Arrow hookParameter) {
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

Arrow isPairHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xl_isPair(arrow) ? arrow : EVE;
   return xl_reduceMachine(CM, r);
}

Arrow branchHook(Arrow CM, Arrow hookParameter) {
  Arrow condition = xl_argInMachine(CM);
  Arrow branch = isEve(condition) ? headOfOperator : tailOfOperator;
  return xl_reduceMachine(CM, branch);
}

Arrow isCloneHook(Arrow CM, Arrow hookParameter) {
   Arrow contextPath = tailOf(CM);
   Arrow arrow = xl_argInMachine(CM);
   Arrow r = xl_isPair(arrow) && xl_equal(tailOf(arrow), headOf(arrow)) ? arrow : EVE;
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

Arrow fallHook(Arrow CM, Arrow hookParameter) {
    Arrow C = tailOf(CM);
    Arrow M = headOf(CM);
    Arrow V = xl_argInMachine(CM);
    return a(a(V, xl_reduceMachine(CM, EVE)), fall);
}

Arrow escalateHook(Arrow CM, Arrow hookParameter) {
    Arrow C = tailOf(CM);
    Arrow M = headOf(CM);
    if (C == EVE)
        return xl_reduceMachine(CM, EVE);
    // arg = (target secret)
    Arrow target_secret_expr = xl_argInMachine(CM);
    Arrow target_secret = tailOf(target_secret_expr);
    Arrow expr = headOf(target_secret_expr);
    Arrow target = tailOf(target_secret);
    Arrow secret = headOf(target_secret);
    char* secret_s = str(secret);
    if (!secret_s)
        return xl_reduceMachine(CM, EVE);

    unsigned char h[20], secret_sha1[41];
    sha1(secret_s, strlen(secret_s), h);
    for (int i = 0; i < 20; i++) {
           sprintf(secret_sha1 + i * 2, "%02x", h[i]);
    }
    free(secret_s);

    Arrow CT = (isPair(C) ? xl_tailOf(C) : EVE); // Meta-context
    Arrow expression = xls_get(CT, a(target, xl_atom(secret_sha1)));

    if (expression == NIL) {
        LOGPRINTF(LOG_WARN, "escalate attempt %O",
                  target_secret_expr);

        return xl_reduceMachine(CM, EVE);
    }

    return a(a(a(expression, expr), EVE), escalate);
}

Arrow digestHook(Arrow CM, Arrow hookParameter) {
   Arrow arrow = xl_argInMachine(CM);
   uint32_t digestSize;
   char* digest = xl_digestOf(arrow, &digestSize);
   return xl_reduceMachine(CM, atomn(digestSize, digest));
}

static void machine_init(Arrow CM) {
    char* keyword;
    XLCallBack callBack;

    if (let) return;

    // initialize key arrows
    // environment = atom("environment");
    let = atom("let");
    load = atom("load");
    var = atom("var");
    escape = atom("escape");
    evalOp = atom("eval");
    lambda = atom("lambda");
    macro = atom("macro");
    closure = atom("closure");
    paddock = atom("paddock"); // closure for macro
    rlambda = atom("rlambda");
    operator = atom("operator");
    continuation = atom("continuation");
    selfM = atom("@M");
    arrowWord = atom("arrow");
    fall = atom("fall");
    escalate = atom("escalate");
    comma = atom(",");
    it = atom("it");
    swearWord = atom("&!#");
    brokenEnvironment = pair(swearWord, atom("broken environment"));
    tempVar = atom("XLR3SuLT");
    // preserve keyarrows from GC
    Arrow locked = atom("locked");
    root(a(locked, let));
    root(a(locked, load));
    root(a(locked, var));
    root(a(locked, escape));
    root(a(locked, evalOp));
    root(a(locked, lambda));
    root(a(locked, macro));
    root(a(locked, closure));
    root(a(locked, rlambda));
    root(a(locked, paddock));
    root(a(locked, operator));
    root(a(locked, continuation));
    root(a(locked, selfM));
    root(a(locked, arrowWord));
    root(a(locked, fall));
    root(a(locked, escalate));
    root(a(locked, comma));
    root(a(locked, it));
    root(a(locked, swearWord));
    root(a(locked, brokenEnvironment));
    root(a(locked, tempVar));

    // root basic operators into the global context

    static struct fnMap_s {
        char *s;
        XLCallBack fn;
    } systemFns[] = {
        {"run", runHook},
        {"tailOf", tailOfHook},
        {"headOf", headOfHook},
        {"childrenOf", childrenOfHook},
        {"childrenReviewOf", childrenReviewOfHook},
        {"partnerOf", partnerOfHook},
        {"root", rootHook},
        {"unroot", unrootHook},
        {"isRooted", isRootedHook},
        {"isPair", isPairHook},
        {"setTailWithHeadIn", setTailWithHeadInHook},
        {"unsetVar", unsetVarHook},
        {"getVar", getVarHook},
        {"branch", branchHook},
        {"isClone", isCloneHook},
        {"commit", commitHook},
        {"fall", fallHook},
        {"escalate", escalateHook},
        {"digest", digestHook},
        {NULL, NULL}
    };

    for (int i = 0; (callBack = systemFns[i].fn) != NULL; i++) {
        keyword = systemFns[i].s;
        Arrow operatorKeyword = atom(keyword);
        Arrow operatorArrow = operator(callBack, EVE);
        // always reset a callback at every reboot because moving pointers
        // NOT A GOOD IDEA: if (xls_get(EVE, operatorKey) != NIL) continue;
        xls_set(EVE, operatorKeyword, operatorArrow);
        if (callBack == tailOfHook)
            tailOfOperator = operatorArrow;
        if (callBack == headOfHook)
            headOfOperator = operatorArrow;
    }

    if (xls_get(EVE, atom("if")) == NIL)
        xls_set(EVE, atom("if"), xl_uri("/paddock//x/let//condition/tailOf+x/let//alternative/headOf+x/arrow/eval/let//it/branch/var+condition/it//escape+escape/var+alternative+"));
    if (xls_get(EVE, atom("equal")) == NIL)
        xls_set(EVE, atom("equal"), xl_uri("/paddock//x/let//a/tailOf+x/let//b/headOf+x/arrow/let///tailOf/var+x/var+a/let///headOf/var+x/var+b/isClone/arrow///escape+var/tailOf/var+x//escape+var/headOf/var+x+"));
    if (xls_get(EVE, atom("get")) == NIL)
        xls_set(EVE, atom("get"), xl_uri("/paddock//x/arrow/getVar//escape+escape/var+x+"));
    if (xls_get(EVE, atom("unset")) == NIL)
        xls_set(EVE, atom("unset"), xl_uri("/paddock//x/arrow/unsetVar//escape+escape/var+x+"));

    if (xls_get(EVE, atom("set")) == NIL)
        xls_set(EVE, atom("set"), xl_uri("/paddock//x/let//slot/tailOf+x/let//exp/headOf+x/arrow/let///headOf/var+x/var+exp/setTailWithHeadIn/arrow///escape+escape/var+slot//escape+var/headOf/var+x+"));

    // System Init call
    xl_eval(EVE, pair(atom("init"), pair(escape, CM))); // we pass CM at parameter to preserve it from GC
}

Arrow xl_run(Arrow C, Arrow M) {
    machine_init(pair(C,M));

    // M = //p/e+k
    chainSize = 0;
    Arrow w;
    while (chainSize < 500 && tail(M) != swearWord && (/*k*/head(head(M)) != EVE || !isTrivialOrBound(tail(M) /*p*/, tail(head(M)) /*e*/, C, M, &w))) {
        // only operators can produce fall/escalate states
        // TODO check secret here

        if (head(M) == fall) {
            Arrow VM = tail(M);
            Arrow V = tail(VM);
            C = a(C, V); // Fall into context
            M = head(VM);
            LOGPRINTF(LOG_WARN, "machine context fall to %O", V);
            continue;
        }

        // only operators can produce such a state
        if (head(M) == escalate) {
            C = tail(C); // Escape from enclosing context
            LOGPRINTF(LOG_WARN, "machine context escalate to %O", C);
            M = tail(M);
            continue;
        }

        M = transition(C, M);
        xl_yield(pair(C,M));
    }

    if (chainSize >= 500) {
        TRACEPRINTF("Continuation chain is too long (infinite loop?), p=%O", tail(M));
        return a(swearWord, atom("too long continuation chain"));
    } else if (tail(M) == swearWord) {
        TRACEPRINTF("run finished with error : %O", head(M));
        return tail(head(M));
    }

    TRACEPRINTF("run finished with M = %O", M);
    if (w == NIL) w = EVE;
    TRACEPRINTF("run result is %O", w);

    LOGPRINTF(LOG_WARN, "xl_run done, transition=%d",
            machine_stats.transition);
    machine_stats = machine_stats_zero;

    return w;
}

Arrow xl_eval(Arrow C /* ContextPath */, Arrow p /* program */) {
    TRACEPRINTF("cl_eval C=%O p=%O", C, p);
    Arrow M = a(p, a(EVE, EVE));
    return xl_run(C /* ContextPath */, M);
}

