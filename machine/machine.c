#include "machine/machine.h"
#define LOG_CURRENT LOG_MACHINE
#include "log/log.h"
#include "space/space.h"
#include "machine/context.h"
#include "machine/session.h"
#include "sha1/sha1.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static struct s_machine_stats {
    int transition;
    int i1, i2, i3, i4, i5, i6, i7, i8, i9;
} machine_stats_zero = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}, machine_stats = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static Arrow tailOfOperator = 0, headOfOperator = 0;

static Arrow eve = 0, let = 0, load = 0, escape = 0, var = 0, comma = 0, it = 0, evalOp = 0, lambda = 0, macro = 0, closure = 0, paddock = 0, rlambda = 0, operator = 0, continuation = 0,
             enterM = 0, exitM = 0, selfM = 0, arrowWord = 0, swearWord = 0, brokenEnvironment = 0, land = 0, tempVar = 0;

/// initialize key arrows
static void update_keywords() {
    // environment = xs_const("environment");
    eve = xs_eve();
    let = xs_const("let");
    load = xs_const("load");
    var = xs_const("var");
    escape = xs_const("escape");
    evalOp = xs_const("eval");
    lambda = xs_const("lambda");
    macro = xs_const("macro");
    closure = xs_const("closure");
    paddock = xs_const("paddock");  // closure for macro
    rlambda = xs_const("rlambda");
    operator = xs_const("operator");
    continuation = xs_const("continuation");
    selfM = xs_const("@M");
    arrowWord = xs_const("arrow");
    enterM = xs_const("enter");
    exitM = xs_const("exit");
    land = xs_const("land");
    comma = xs_const(",");
    it = xs_const("it");
    swearWord = xs_const("&!#");
    brokenEnvironment = xs_pair(swearWord, xs_const("broken environment"));
    tempVar = xs_const("XLR3SuLT");
}

static void machine_init(Arrow);

Arrow xs_operator(XSCallBack hookp, Arrow context) {
    return xs_pair(operator, xs_pair(xs_hook(hookp), context));
}

Arrow xs_continuation(XSCallBack hookp, Arrow context) {
    return xs_pair(continuation, xs_pair(xs_hook(hookp), context));
}

Arrow _machine_commit(Arrow C, Arrow preserved) {
    // Arrow M = xs_getHead(CM);
    xs_context_root(C, preserved);  // TODO/FIXME fix this
    Address preserved_id = xs_getId(xs_resolve(preserved));
    C = xs_commit(C);
    update_keywords();
    preserved = xs_arrow(preserved_id);
    xs_context_unroot(C, preserved);  // TODO/FIXME fix this
    return preserved;
}

/** Load a binding into an environment arrow (list arrow), trying to remove a previous binding for this variable to limit its size */
static Arrow _load_binding(Arrow x, Arrow w, Arrow e) {
    // typically xs_pair(xs_pair(xs_pair(x, w), e)
    int i;
    Arrow et = e;
    Arrow temp = eve;
#define MAX_SEARCH_PREVIOUS_BINDING 10
    return xs_pair(xs_pair(x, w), e);

    //
    // BAD IDEA ! :O !!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    for (i = 0; i < MAX_SEARCH_PREVIOUS_BINDING; i++) {
        if (xs_isEve(et))
            break;
        Arrow c = xs_getTail(et);
        if (xs_equal(xs_getTail(c), x))
            break;
        temp = xs_pair(c, temp);
        et = xs_getHead(et);
    }
    if (xs_isEve(et) || i == MAX_SEARCH_PREVIOUS_BINDING)  // no x binding found
        return xs_pair(xs_pair(x, w), e);

    Arrow ne = xs_pair(xs_pair(x, w), xs_getHead(et));
    while (!xs_isEve(temp)) {
        ne = xs_pair(xs_getTail(temp), ne);
        temp = xs_getHead(temp);
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
    if (xs_isPair(a)) {
        Arrow t = xs_getTail(a);
        Arrow h = xs_getHead(a);
        if (xs_equal(t, escape))
            return h;
        else if (xs_equal(t, var)) {
            Arrow w = _resolve(a, e, C, M);
            if (w == NULL)
                w = eve;
            return w;
        } else {
            Arrow rt = _resolve_deeply(t, e, C, M);
            Arrow rh = _resolve_deeply(h, e, C, M);
            return xs_pair(rt, rh);
        }
    } else {
        return a;
    }
}

static Arrow _resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
    //DEBUGPRINTF("   _resolve a = %O", a);
    if (xs_isEve(a))
        return a;
    if (xs_equal(a, selfM))
        return M;

    Arrow x = a;
    if (xs_isPair(a)) {
        Arrow t = xs_getTail(a);
        if (xs_equal(t, swearWord) ||
            xs_equal(t, closure) ||
            xs_equal(t, paddock) ||
            xs_equal(t, operator)) {
            return a;  // naturally escaped / typed litteral
        } else if (xs_equal(t, arrowWord)) {
            Arrow h = xs_getHead(a);
            return _resolve_deeply(h, e, C, M);
        } else if (xs_equal(t, escape)) {
            return xs_getHead(a);
        } else if (xs_equal(t, lambda)) {
            Arrow xs = xs_getHead(a);
            return xs_pair(closure, xs_pair(xs, e));
        } else if (xs_equal(t, rlambda)) {
            Arrow xs = xs_getHead(a);
            return xs_pair(closure, xs_pair(eve, xs_pair(xs, e)));  // recursive closure
        } else if (xs_equal(t, macro)) {
            Arrow xs = xs_getHead(a);
            return xs_pair(paddock, xs_pair(xs, e));
        }

        if (xs_equal(t, var)) {
            x = xs_getHead(a);
        }
    }
    // environment matching loop
    Arrow se = e;
    while (xs_isPair(se)) {
        Arrow b = xs_getTail(se);
        Arrow bx = xs_getTail(b);
        if (xs_equal(bx, x))
            return xs_getHead(b);
        se = xs_getHead(se);
    }
    if (se != eve) {
        // Environnement is broken
        return brokenEnvironment;
    }

    return xs_context_get(C, x);  // may be NULL
}

static Arrow resolve(Arrow a, Arrow e, Arrow C, Arrow M) {
    TRACEPRINTF("resolve a = %O in e = %O C = %O", a, e, C);
    Arrow w = _resolve(a, e, C, M);

    if (w == NULL) {              // Not bound
        if (xs_equal(xs_getTail(a), var)) {  // var+x
            w = eve;             // unbound var+x resolved to eve
        } else {
            w = a;  // unbound atom (not variable casted) is let as is
        }
    }

    TRACEPRINTF("resolved in w = %O", w);
    return w;
}

static int isTrivial(Arrow s) {
    if (xs_isAtom(s))
        return 1;  // an atomic arrow is always trivial
    Arrow t = xs_getTail(s);
    // TODO what if I used arrows for casting these keywords?
    if (
        xs_equal(t, swearWord) ||
            xs_equal(t, closure) ||
            xs_equal(t, paddock) ||
            xs_equal(t, operator) ||
            xs_equal(t, arrowWord) ||
            xs_equal(t, escape) ||
            xs_equal(t, lambda) ||
            xs_equal(t, rlambda) ||
            xs_equal(t, macro) ||
            xs_equal(t, var)
        )
        return 1;  // TODO closure,paddock,escape,arrowWord,var: as hooks
    return 0;
}

static int isTrivialOrBound(Arrow a, Arrow e, Arrow C, Arrow M, Arrow *w) {
    if (isTrivial(a)) {
        *w = resolve(a, e, C, M);
        TRACEPRINTF("isTrivialOrBound(%O) : is trivial and resolved to %O !!", a, *w);
        return (*w != NULL);
    } else {
        *w = _resolve(a, e, C, M);
        if (*w != NULL) {
            TRACEPRINTF("Arrow %O is bound to %O !!", a, *w);
            return !0;
        }
        return 0;
    }
}

static int chainSize = 0;  // TODO thread safe

static Arrow transition(Arrow C, Arrow M) {  // M = (p, (e, k))
    assert(M);
    Arrow p = xs_getTail(M);    // program
    Arrow ins = xs_getTail(p);  // let,load,eval,lambda,macro,... instruction
    Arrow param = xs_getHead(p);
    Arrow ek = xs_getHead(M);
    Arrow e = xs_getTail(ek);
    Arrow k = xs_getHead(ek);
    Arrow w;

    TRACEPRINTF("\ntransition p = %O\n   e = %O\n   k = %O", p, e, k);
    machine_stats.transition++;

    if (xs_equal(ins, load)) {  //load expression #e#
        // p == (load (s0 s1))
        dputs("p == (load (s0 s1))");
        Arrow s0 = xs_getTail(param);
        Arrow s1 = xs_getHead(param);

        // (load (s0 s1)) <==> (let ((Eve s0) s1))) : direct environment loading
        // rewriting program as pp = (let ((Eve s0) s1))
        Arrow pp = xs_pair(let, xs_pair(xs_pair(eve, s0), s1));
        M = xs_pair(pp, ek);
        return M;
    }

    if (xs_equal(ins, evalOp)) {  //eval expression
        // p == (eval s)
        dputs("p == (eval s)");
        Arrow s = xs_getHead(p);
        chainSize++;
        M = xs_pair(s, xs_pair(e, xs_pair(evalOp, xs_pair(e, k))));  // stack an eval continuation
        return M;
    }

    if (xs_equal(ins, let)) {  //let expression family
        // p == (let ((x v) s))
        dputs("p == (let ((x v) s)) # let expression");
        Arrow v = xs_getHead(xs_getTail(xs_getHead(p)));
        Arrow x = xs_getTail(xs_getTail(xs_getHead(p)));
        Arrow s = xs_getHead(xs_getHead(p));
        Arrow w;
        dputs("   x = %O\n   v = %O\n   s= %O", x, v, s);

        // FIXME x == "@M" case

        if (isTrivialOrBound(v, e, C, M, &w)) {  // Trivial let expression
            // p == (let ((x v:t) s))
            dputs("     v (%O) == t trivial", v);

            // FIXME: if v == /p+v where p resolve to a paddock, one should eval the bound value
            if (xs_equal(w, brokenEnvironment))
                return xs_pair(swearWord, xs_pair(brokenEnvironment, M));

            if (xs_isEve(x)) {  // #e# direct environment load
                dputs("          direct environment load");
                M = xs_pair(s, xs_pair(_load_binding(xs_getTail(w), xs_getHead(w), e), k));
            } else {
                M = xs_pair(s, xs_pair(_load_binding(x, w, e), k));
            }
            return M;
        }

        // Non trivial let expressions ...

        Arrow v0 = xs_getTail(v);
        if (xs_equal(v0, let)) {  // let expression as application closure in containing let #e#
            dputs("     v == let expression");
            // stack up a continuation
            // M = (v (e ((x (s e)) k)))
            chainSize++;
            M = xs_pair(v, xs_pair(e, xs_pair(xs_pair(x, xs_pair(s, e)), k)));
            return M;
        }

        Arrow v1 = xs_getHead(v);

        if (xs_equal(v0, evalOp)) {  // eval expression in containing let #e#
            // p == (let ((x (eval ss) s))
            dputs("     v == (eval ss) # an eval expression");
            Arrow ss = v1;
            chainSize++;
            M = xs_pair(ss, xs_pair(e, xs_pair(evalOp, xs_pair(e, xs_pair(xs_pair(x, xs_pair(s, e)), k)))));  // stack an eval continuation
            return M;
        }

        Arrow w0 = NULL;
        Arrow w1 = NULL;

        if (!isTrivialOrBound(v0, e, C, M, &w0)) {  // non trivial expression in application in let expression #e#
            // p == (let ((x v:(v0 v1)) s)) where v0 not trivial
            dputs("p == (let ((x v:(v0 v1)) s)) where v0 not trivial");
            // rewriting program to stage s0 evaluation
            // TODO use (tail v) instead of p as variable name
            // pp = (let ((tmp v0) (let ((x ((var tmp) v1)) s))))
            Arrow pp = xs_pair(let, xs_pair(xs_pair(M, v0), xs_pair(let, xs_pair(xs_pair(x, xs_pair(xs_pair(var, M), v1)), s))));
            M = xs_pair(pp, ek);
            return M;
        }

        if (xs_equal(w0, brokenEnvironment))
            return xs_pair(swearWord, xs_pair(brokenEnvironment, M));

        Arrow w0_type = xs_getTail(w0);

        if (!isTrivialOrBound(v1, e, C, M, &w1) && !xs_equal(w0_type, paddock)) {
            // non trivial argument in application in let expression #e#
            // p == (let ((x v:(t0 v1)) s)) where v1 not trivial
            dputs("p == (let ((x v:(t0 v1)) s)) where v1 not trivial");

            // rewriting program to stage v1 and (v0 v1) evaluation
            // pp = (let ((tmp v1) (let ((x (v0 (var tmp))) s))))
            // TODO use (head v) instead of p as variable name
            Arrow pp = xs_pair(let, xs_pair(xs_pair(M, v1), xs_pair(let, xs_pair(xs_pair(x, xs_pair(v0, xs_pair(var, M))), s))));
            M = xs_pair(pp, ek);
            return M;
        }

        // p == (let ((x (t0 t1)) s)) where t0 is a closure or equivalent
        dputs("p == (let ((x v:(v0:t0 v1:t1) s))  # a really trivial application in let expression");
        Arrow t0 = v0;
        Arrow t1 = v1;

        if (xs_equal(w0_type, paddock) && isTrivialOrBound(xs_pair(t0, t1), e, C, M, &w)) {
            dputs("   t0 is bound to a paddock w0 and /w0+t1 is bound to %O, one evals this expression", w);
            // FIXME : xs_pair(t0, t1) is v+ See FIXME above
            M = xs_pair(w, xs_pair(evalOp, xs_pair(e, xs_pair(xs_pair(x, xs_pair(s, e)), k))));
            return M;
        } else if (!xs_equal(w0_type, paddock) && isTrivialOrBound(xs_pair(t0, w1), e, C, M, &w)) {
            dputs("   /t0 is not bound to a paddock and /t0+resolve(t1) is bound to %O", w);

            if (xs_isEve(x)) {  // #e# direct environment load
                dputs("          direct environment load");
                M = xs_pair(s, xs_pair(_load_binding(xs_getTail(w), xs_getHead(w), e), k));
            } else {  // as simple as trivial application
                M = xs_pair(s, xs_pair(_load_binding(x, w, e), k));
            }
            return M;
        }

        if (xs_equal(w0_type, operator)) {  // System call special case
            // r(t0) = (operator (hook context))
            dputs("  resolve(t0) = (operator (hook context))");
            Arrow operatorParameter = xs_getHead(xs_getHead(w0));
            Arrow operatorHook = xs_getTail(xs_getHead(w0));
            XSCallBack cb = xs_getPointer(operatorHook);
            assert(cb);
            M = cb(xs_pair(C, M), operatorParameter);
            return M;

        } else if (xs_equal(w0_type, paddock) || xs_equal(w0_type, closure)) {
            dputs("    w0_type = %O", w0_type);
            Arrow yse = xs_getHead(w0);
            Arrow ee = xs_getHead(yse);
            Arrow ys = xs_getTail(yse);
            int recursive = xs_isEve(ys);
            if (recursive) {
                yse = xs_getHead(yse);
                ys = xs_getTail(yse);
            }
            Arrow y = xs_getTail(xs_getTail(yse));
            Arrow ss = xs_getHead(xs_getTail(yse));
            if (xs_equal(w0_type, paddock)) {  // #e# paddock special closure
                // r(t0) == (paddock ((y ss) ee))
                dputs("  resolve(t0) == %O", w0);
                // applied arrow is not resolved

                // stacks up two continuation
                // the first is a normal continuation to eval paddock expression
                // second one is to eval expression result in caller closure
                // that is the evaluation of the expression after macro-substitution
                chainSize += 2;
                ee = _load_binding(y, t1, ee);
                if (recursive)
                    ee = _load_binding(it, w0, ee);
                M = xs_pair(ss, xs_pair(ee, xs_pair(evalOp, xs_pair(e, xs_pair(xs_pair(x, xs_pair(s, e)), k)))));
            } else {
                // r(t0) == (closure ((y ss) ee))
                dputs("  resolve(t0) == %O", w0);
                if (xs_equal(w1, brokenEnvironment))
                    return xs_pair(swearWord, xs_pair(brokenEnvironment, M));
                chainSize++;
                ee = _load_binding(y, w1, ee);
                if (recursive)
                    ee = _load_binding(it, w0, ee);
                M = xs_pair(ss, xs_pair(ee, xs_pair(xs_pair(x, xs_pair(s, e)), k)));  // stacks up a continuation
            }

            return M;

        } else {  // not a closure thing
            TRACEPRINTF("resolve(t0)=%O is not closure-like\n", w0);
            if (xs_equal(w, brokenEnvironment))
                return xs_pair(swearWord, xs_pair(brokenEnvironment, M));

            M = xs_pair(s, xs_pair(_load_binding(x, xs_pair(w0, w1), e), k));
            return M;
        }
    }  // let

    if (isTrivialOrBound(p, e, C, M, &w)) {  // Trivial expression (including lambda expression)
        // p == v
        dputs("p == v trivial");

        // Let's unstack a continuation

        if (xs_isEve(k))
            return M;  // stop, program result = resolve(v, e, M)

        if (xs_equal(xs_getTail(k), continuation)) {  //special system continuation #e
            // TODO : could be operator or hook
            // k == (continuation (<hook> <context>))
            dputs("    k == (continuation (<hook> <context>))");
            Arrow continuationParameter = xs_getHead(xs_getHead(k));
            XSCallBack cb = xs_getPointer(xs_getTail(xs_getHead(k)));
            assert(cb);
            M = cb(xs_pair(C, M), continuationParameter);
            return M;

        } else if (xs_equal(xs_getTail(k), evalOp)) {  // #e# special "eval" continuation
            // k == (eval (ee kk))
            dputs("    k == (eval (ee kk))");
            Arrow eekk = xs_getHead(k);
            Arrow ee = xs_getTail(eekk);
            Arrow kk = xs_getHead(eekk);
            if (xs_equal(w, brokenEnvironment))
                return xs_pair(swearWord, xs_pair(brokenEnvironment, M));
            chainSize--;
            M = xs_pair(w, xs_pair(ee, kk));  // unstack continuation and reinject evaluation result as input expression
            return M;

        } else {
            // k == ((x (ss ee)) kk)
            dputs("    k == ((x (ss ee)) kk)");
            if (xs_equal(w, brokenEnvironment))
                return xs_pair(swearWord, xs_pair(brokenEnvironment, M));

            Arrow kk = xs_getHead(k);
            Arrow f = xs_getTail(k);
            Arrow x = xs_getTail(f);
            Arrow ss = xs_getTail(xs_getHead(f));
            Arrow ee = xs_getHead(xs_getHead(f));
            if (xs_isEve(x)) {  // #e : variable name is Eve (let ((Eve a) s1))
                dputs("variable name is Eve (let ((Eve a) s1))");
                // ones loads the arrow directly into environment. It will be considered as a var->value binding
                chainSize--;
                M = xs_pair(ss, xs_pair(_load_binding(xs_getTail(w), xs_getHead(w), ee), kk));  // unstack continuation, postponed let solved
            } else {
                chainSize--;
                M = xs_pair(ss, xs_pair(_load_binding(x, w, ee), kk));  // unstack continuation, postponed let solved
            }
            return M;
        }
    }

    // application cases
    dputs("p == (s v)");
    Arrow s = xs_getTail(p);
    Arrow v = xs_getHead(p);
    Arrow ws;

    if (xs_isPair(v) && xs_equal(xs_getTail(v), comma)) {
        dputs("p == (s (, next))");
        // TODO: right-paddock to emulate this
        //  <==> (let (it s) next)
        Arrow next = xs_getHead(v);
        if (next == comma)
            return xs_pair(s, ek);
        // k = ((it (next e)) k)
        chainSize++;
        return xs_pair(s, xs_pair(e, xs_pair(xs_pair(it, xs_pair(next, e)), k)));
    }

    if (!isTrivialOrBound(s, e, C, M, &ws)) {  // Not trivial closure in application #e#
        dputs("p == (s v) where s is not trivial");
        // rewriting rule: pp = (let ((tmp s) ((var tmp) v)))
        // ==> M = (s (e ((tmp (((var tmp) v) e)) k)))
        M = xs_pair(s, xs_pair(e, xs_pair(xs_pair(M, xs_pair(xs_pair(xs_pair(var, M), v), e)), k)));
        return M;
    }
    if (xs_equal(ws, brokenEnvironment))
        return xs_pair(swearWord, xs_pair(brokenEnvironment, M));

    if (xs_equal(xs_getTail(v), let)) {  // let expression as application argument #e#
        dputs("    p == (s v:(let ((x vv) ss))");
        // rewriting rule: pp = (let ((tmp v) (s (var tmp))))
        // ==> M = (v (e ((tmp ((s (var tmp)) e)) k))))
        M = xs_pair(v, xs_pair(e, xs_pair(xs_pair(M, xs_pair(xs_pair(xs_pair(escape, ws), xs_pair(var, M)), e)), k)));
        return M;
    }

    if (xs_equal(xs_getTail(v), evalOp)) {  //eval expression as application argument #e#
        // p == (s (eval ss))
        dputs("     v == (eval ss) # an eval expression");
        Arrow ss = xs_getHead(v);
        chainSize++;
        M = xs_pair(ss, xs_pair(e, xs_pair(evalOp, xs_pair(e, xs_pair(xs_pair(M, xs_pair(xs_pair(xs_pair(escape, ws), xs_pair(var, M)), e)), k)))));  // stack an eval continuation
        return M;
    }

    Arrow ws_type = xs_getTail(ws);
    Arrow wv = NULL;
    if (!isTrivialOrBound(v, e, C, M, &wv) && !xs_equal(ws_type, paddock)) {  // Not trivial argument in application #e#
        dputs("    v (%O) == something not trivial", v);
        // rewriting rule: pp = (let ((tmp v) (s (var tmp))))
        // ==> M = (v (e ((tmp ((s (var tmp)) e)) k))))
        chainSize++;
        M = xs_pair(v, xs_pair(e, xs_pair(xs_pair(M, xs_pair(xs_pair(s, xs_pair(var, M)), e)), k)));
        return M;
    }

    // v == t trivial
    // Really trivial application
    // Continuation stacking not needed!

    // p == (t0 t1) where t0 should return a closure or such
    dputs("    p == (t0:s t1:v) # a really trivial application");
    // Arrow t0 = s;
    Arrow t1 = v;
    if (xs_equal(ws_type, operator)) {  // System call case
        // resolve(t0) == (operator (hook context))
        dputs("       resolve(t0) == /operator/hook+context (%O)", ws);
        Arrow operatorParameter = xs_getHead(xs_getHead(ws));
        XSCallBack cb = xs_getPointer(xs_getTail(xs_getHead(ws)));
        assert(cb);
        M = cb(xs_pair(C, M), operatorParameter);
        return M;

    } else if (xs_equal(ws_type, paddock) || xs_equal(ws_type, closure)) {
        // closure/paddock case
        Arrow yse = xs_getHead(ws);
        Arrow ys = xs_getTail(yse);
        int recursive = xs_isEve(ys);
        if (recursive) {
            yse = xs_getHead(yse);
            ys = xs_getTail(yse);
        }
        Arrow ee = xs_getHead(yse);
        Arrow x = xs_getTail(xs_getTail(yse));
        Arrow ss = xs_getHead(xs_getTail(yse));

        if (xs_equal(ws_type, paddock)) {  // #e# paddock special closure
            // r(t0) == (paddock ((x ss) ee))
            dputs("        resolve(t0) == (paddock ((x ss) ee))");
            wv = t1;  // applied arrow is not evaluated (like in let construct)

            // stacks up one continuation to eval the expression after macro-substitution
            chainSize++;
            ee = _load_binding(x, wv, ee);
            if (recursive)
                ee = _load_binding(it, ws, ee);
            M = xs_pair(ss, xs_pair(ee, xs_pair(evalOp, ek)));
        } else {
            // r(t0) == (closure ((x ss) ee))
            dputs("        resolve(t0) == (closure ((x ss) ee))");
            if (xs_equal(wv, brokenEnvironment))
                return xs_pair(swearWord, xs_pair(brokenEnvironment, M));
            //chainSize--;
            ee = _load_binding(x, wv, ee);
            if (recursive)
                ee = _load_binding(it, ws, ee);
            M = xs_pair(ss, xs_pair(ee, k));
        }

        return M;

    } else {
        TRACEPRINTF("info: resolve(t0)=%O is not closure-like\n", ws);
        // not a closure, one let's the expression almost as if it was escaped
        if (xs_equal(w, brokenEnvironment))
            return xs_pair(swearWord, xs_pair(brokenEnvironment, M));
        else if (wv == NULL)
            wv = t1;
        M = xs_pair(xs_pair(escape, xs_pair(ws, wv)), ek);
        return M;
    }
}

Arrow xs_argInMachine(Arrow CM) {
    Arrow C = xs_getTail(CM);
    Arrow M = xs_getHead(CM);
    Arrow p = xs_getTail(M);
    Arrow ek = xs_getHead(M);
    Arrow e = xs_getTail(ek);
    Arrow arg;
    if (xs_equal(xs_getTail(p), let)) {
        // p == (let ((x (<operator> arg)) s))
        arg = xs_getHead(xs_getHead(xs_getTail(xs_getHead(p))));
    } else {
        // p == (<operator> arg)
        arg = xs_getHead(p);
    }
    Arrow w;
    int bound = isTrivialOrBound(arg, e, C, M, &w);
    assert(bound);
    resolve(arg, e, C, M);
    TRACEPRINTF("   argument is %O resolved in %O", arg, w);
    return w;
}

Arrow xs_reduceMachine(Arrow CM, Arrow r) {
    Arrow M = xs_getHead(CM);
    Arrow p = xs_getTail(M);
    Arrow ek = xs_getHead(M);
    if (xs_equal(xs_getTail(p), let)) {
        // p == (let ((x (<operator> arg)) s))
        TRACEPRINTF("   let-application reduced to r = %O", r);
        Arrow x = xs_getTail(xs_getTail(xs_getHead(p)));
        Arrow s = xs_getHead(xs_getHead(p));
        TRACEPRINTF("     s = %O", s);
        Arrow e = xs_getTail(ek);
        Arrow k = xs_getHead(ek);
        TRACEPRINTF("     k = %O", k);
        Arrow ne = _load_binding(x, r, e);
        M = xs_pair(s, xs_pair(ne, k));
    } else {
        TRACEPRINTF("   application reduced to r = %O", r);
        M = xs_pair(xs_pair(escape, r), ek);
    }
    return M;
}

Arrow runHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow M = xs_argInMachine(CM);
    if (xs_equal(xs_getHead(M), exitM))  // NO
        return eve;

    return M;
}

Arrow tailOfHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_getTail(arrow);
    return xs_reduceMachine(CM, r);
}

Arrow headOfHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_getHead(arrow);
    return xs_reduceMachine(CM, r);
}

Arrow childrenReviewOfHook(Arrow CM, Arrow hookParameter) {
    Arrow parent = xs_argInMachine(CM);
    if (xs_isEve(hookParameter)) {
        XLEnum e = xl_childrenOf(xs_getId(xs_assimilate(parent)));
        return xs_reduceMachine(CM, xs_operator(childrenReviewOfHook, xs_hook(e)));
    }
    XLEnum e = xs_getPointer(hookParameter);
    Arrow child;
    if (!e) {
        child = eve;
    } else if (xl_enumNext(e)) {
        child = xs_arrow(xl_enumGet(e));
    } else {
        xl_enumFree(e); // FIXME : only on forget
        child = eve;
    }
    return xs_reduceMachine(CM, child);
}

Arrow childrenOfHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow parent = xs_argInMachine(CM);
    XLEnum e = xl_childrenOf(xs_getId(xs_assimilate(parent)));
    if (!e)
        return xs_reduceMachine(CM, eve);

    Arrow list = eve;
    while (xl_enumNext(e)) {
        Arrow child = xs_arrow(xl_enumGet(e));
        list = xs_pair(child, list);
    }
    xl_enumFree(e);
    return xs_reduceMachine(CM, list);
}

Arrow linkHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_context_link(C, xs_getTail(arrow), xs_getHead(arrow));
    return xs_reduceMachine(CM, r);
}

Arrow unlinkHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_context_unlink(C, xs_getTail(arrow), xs_getHead(arrow));
    return xs_reduceMachine(CM, r);
}

Arrow browseHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow list = xs_context_browse(C, arrow);
    return xs_reduceMachine(CM, list);
}

Arrow rootHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_context_root(C, arrow);
    return xs_reduceMachine(CM, r);
}

Arrow unrootHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow contextPath = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_context_unroot(contextPath, arrow);
    return xs_reduceMachine(CM, r);
}

Arrow setVarHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_context_set(C, xs_getTail(arrow), xs_getHead(arrow));
    return xs_reduceMachine(CM, r);
}

Arrow unsetVarHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow contextPath = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    xs_context_unset(contextPath, arrow);
    return xs_reduceMachine(CM, arrow);
}

Arrow getVarHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_context_get(C, arrow);
    return xs_reduceMachine(CM, (r == NULL ? eve : r));  // TODO: "throwing" an error?
}

Arrow isRootedHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow contextPath = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    int r = xs_context_isRooted(contextPath, arrow);
    return xs_reduceMachine(CM, r ? arrow : xs_eve());  // no context
}

Arrow isPairHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    // Arrow contextPath = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    int r = xs_isPair(arrow);
    return xs_reduceMachine(CM, r ? arrow : xs_eve());
}

Arrow branchHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow condition = xs_argInMachine(CM);
    Arrow branch = xs_isEve(condition) ? headOfOperator : tailOfOperator;
    return xs_reduceMachine(CM, branch);
}

Arrow isCloneHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    // Arrow contextPath = xs_getTail(CM);
    Arrow arrow = xs_argInMachine(CM);
    Arrow r = xs_isPair(arrow) && xs_equal(xs_getTail(arrow), xs_getHead(arrow)) ? arrow : eve;
    return xs_reduceMachine(CM, r);
}

Arrow commitHook(Arrow CM, Arrow hookParameter) {
    (void) hookParameter; // NOT USED
    Arrow C = xs_getTail(CM);
    Arrow state = xs_pair(selfM, CM);
    state = _machine_commit(C, state);
    CM = xs_getHead(state);
    return xs_reduceMachine(CM, eve);
}

Arrow enterHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    // Arrow C = xs_getTail(CM);
    // Arrow M = xs_getHead(CM);
    Arrow V = xs_argInMachine(CM);
    return xs_pair(xs_pair(V, xs_reduceMachine(CM, eve)), enterM);
}

Arrow exitHook(Arrow CM, Arrow hookParameter) {
    (void)hookParameter;  // NOT USED
    Arrow C = xs_getTail(CM);
    // Arrow M = xs_getHead(CM);
    if (xs_isEve(C))
        return xs_reduceMachine(CM, eve);
    // arg = (target secret)
    Arrow target_secret_expr = xs_argInMachine(CM);
    Arrow target_secret = xs_getTail(target_secret_expr);
    Arrow expr = xs_getHead(target_secret_expr);
    Arrow target = xs_getTail(target_secret);
    Arrow secret = xs_getHead(target_secret);
    char *secret_s = xs_getStr(secret);
    if (!secret_s)
        return xs_reduceMachine(CM, eve);

    uint8_t h[20];
    char secret_sha1[41];
    sha1((uint8_t *)secret_s, strlen(secret_s), h);
    for (int i = 0; i < 20; i++) {
        sprintf(secret_sha1 + i * 2, "%02x", h[i]);
    }
    free(secret_s);

    Arrow CT = (xs_isPair(C) ? xs_getTail(C) : eve);  // Meta-context
    Arrow expression = xs_context_get(CT, xs_pair(target, xs_atom(secret_sha1)));

    if (expression == NULL) {
        WARNPRINTF("exit attempt %O", target_secret_expr);

        return xs_reduceMachine(CM, eve);
    }

    return xs_pair(xs_pair(xs_pair(expression, expr), eve), exitM);
}

Arrow landHook(Arrow CM, Arrow hookParameter) {
    (void) hookParameter; // NOT USED

    return xs_pair(xs_reduceMachine(CM, eve), land);
}

Arrow digestHook(Arrow CM, Arrow hookParameter) {
    (void) hookParameter; // NOT USED

    Arrow arrow = xs_argInMachine(CM);
    size_t digestSize;
    char *digest = xs_getDigest(arrow, &digestSize);
    return xs_reduceMachine(CM, xs_atomn(digestSize, (uint8_t *)digest));
}

static void machine_init(Arrow CM) {
    (void) CM;

    char *keyword;
    XSCallBack callBack;

    update_keywords();

    // root basic operators into the global context
    static struct fnMap_s {
        char *s;
        XSCallBack fn;
    } systemFns[] = { { "run", runHook },
                      { "tailOf", tailOfHook },
                      { "headOf", headOfHook },
                      { "childrenOf", childrenOfHook },
                      { "childrenReviewOf", childrenReviewOfHook },
                      { "root", rootHook },
                      { "unroot", unrootHook },
                      { "link", linkHook },
                      { "unlink", unlinkHook },
                      { "browse", browseHook },
                      { "isRooted", isRootedHook },
                      { "isPair", isPairHook },
                      { "setVar", setVarHook },
                      { "unsetVar", unsetVarHook },
                      { "getVar", getVarHook },
                      { "branch", branchHook },
                      { "isClone", isCloneHook },
                      { "commit", commitHook },
                      { "enter", enterHook },
                      { "exit", exitHook },
                      { "land", landHook },
                      { "digest", digestHook },
                      { NULL, NULL } };

    for (int i = 0; (callBack = systemFns[i].fn) != NULL; i++) {
        keyword = systemFns[i].s;
        Arrow operator = xs_operator(callBack, eve);
        xs_context_set(eve, xs_const(keyword), operator);
        if (callBack == tailOfHook)
            tailOfOperator = operator;
        if (callBack == headOfHook)
            headOfOperator = operator;
    }

    if (xs_context_get(eve, xs_const("if")) == NULL)
        xs_context_set(eve, xs_const("if"),
                xs_uri("/paddock//x/let//condition/tailOf+x/let//alternative/headOf+x/arrow/eval/let//it/branch/var+condition/it//escape+escape/var+alternative+"));
    if (xs_context_get(eve, xs_const("equal")) == NULL)
        xs_context_set(eve, xs_const("equal"),
                xs_uri("/paddock//x/let//a/tailOf+x/let//b/headOf+x/arrow/let///headOf/var+x/var+a/let///tailOf/var+x/var+b/isClone/arrow///escape+var/tailOf/var+x//escape+var/"
                       "headOf/var+x+"));
    if (xs_context_get(eve, xs_const("get")) == NULL)
        xs_context_set(eve, xs_const("get"), xs_uri("/paddock//x/arrow/getVar//escape+escape/var+x+"));
    if (xs_context_get(eve, xs_const("unset")) == NULL)
        xs_context_set(eve, xs_const("unset"), xs_uri("/paddock//x/arrow/unsetVar//escape+escape/var+x+"));
    if (xs_context_get(eve, xs_const("set")) == NULL)
        xs_context_set(eve, xs_const("set"),
            xs_uri("/paddock//x/let//slot/tailOf+x/let//exp/headOf+x/arrow/let///headOf/var+x/var+exp/setVar/arrow///escape+escape/var+slot//escape+var/headOf/var+x+"));
    if (xs_context_get(eve, xs_const("link")) == NULL)
        xs_context_set(eve, xs_const("link"),
                xs_uri("/paddock//x/let//slot/tailOf+x/let//exp/headOf+x/arrow/let///tailOf/var+x/var+slot/let///headOf/var+x/var+exp/link/arrow///escape+var/tailOf/"
                       "var+x//escape+var/headOf/var+x+"));
    if (xs_context_get(eve, xs_const("unlink")) == NULL)
        xs_context_set(eve, xs_const("unlink"),
                xs_uri("/paddock//x/let//slot/tailOf+x/let//exp/headOf+x/arrow/let///tailOf/var+x/var+slot/let///headOf/var+x/var+exp/unlink/arrow///escape+var/tailOf/"
                       "var+x//escape+var/headOf/var+x+"));

    // System Init call
    // xs_eval(eve, xs_pair(xs_const("init"), xs_pair(escape, CM)), xs_const("init"));  // we pass CM at parameter to preserve it from GC
}

Arrow xs_run(Arrow C, Arrow M, Arrow session) {
    TRACEPRINTF("BEGIN xs_run(%O)", M);
    TRACEPRINTF(" C = %O", C);
    TRACEPRINTF(" session = %O", session);
    machine_init(xs_pair(C, M));

    // M = //p/e+k
    chainSize = 0;
    Arrow w;
    while (chainSize < 500
        && !xs_equal(xs_getTail(M), swearWord)) {
        // M = //p/e+k
        Arrow p = xs_getTail(M);
        Arrow ek = xs_getHead(M);
        Arrow e = xs_getTail(ek);
        Arrow k = xs_getHead(ek);
        TRACEPRINTF("New state M = //p/e+k");
        TRACEPRINTF("p = %O", p);
        TRACEPRINTF("e = %O", e);
        TRACEPRINTF("k = %O", k);
        if (k == eve
             && isTrivialOrBound(p, e, C, M, &w)
            ) break;


        // only operators can produce enter/exit states
        // TODO check secret here
        Arrow MHead = xs_getHead(M);
        if (xs_equal(MHead, enterM)) {
            Arrow VM = xs_getTail(M);
            Arrow V = xs_getTail(VM);
            C = xs_pair(C, V);  // Fall into context
            M = xs_getHead(VM);
            WARNPRINTF("machine enters into context %O", V);
            continue;
        }

        // only operators can produce such a state
        if (xs_equal(MHead, exitM)) {
            WARNPRINTF("machine exits from context %O", C);
            C = xs_getTail(C);  // Escape from enclosing context
            WARNPRINTF("machine context is now %O", C);
            M = xs_getTail(M);
            continue;
        }

        if (xs_equal(MHead, land)) {
            xs_context_set(eve, session, C);
            WARNPRINTF(" landing to %O", C);
            M = xs_getTail(M);
            continue;
        }

        M = transition(C, M);
    }

    if (chainSize >= 500) {
        TRACEPRINTF("Continuation chain is too long (infinite loop?), p=%O", xs_getTail(M));
        return xs_pair(swearWord, xs_atom("too long continuation chain"));
    }

    if (xs_equal(xs_getTail(M), swearWord)) {
        TRACEPRINTF("run finished with error : %O", xs_getHead(M));
        return xs_getTail(xs_getHead(M));
    }

    //DEBUGPRINTF("run finished with M = %O", M);
    if (w == NULL)
        w = eve;

    TRACEPRINTF("END xs_run(...) = %O transition=%d", w, machine_stats.transition);

    machine_stats = machine_stats_zero;

    return w;
}

Arrow xs_eval(Arrow C /* ContextPath */, Arrow p /* program */, Arrow session) {
    TRACEPRINTF("xs_eval(%O, %O)", C, p);
    Arrow M = xs_pair(p, xs_pair(xs_eve(), xs_eve()));
    return xs_run(C /* ContextPath */, M, session);
}
