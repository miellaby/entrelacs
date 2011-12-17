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

static Arrow let = 0, load = 0, environment = 0, escape = 0, var = 0, evalOp = 0, lambda = 0, lambdax = 0, operator = 0, continuation = 0, selfM = 0, arrowOp = 0, systemEnvironment = 0;

static Arrow printArrow(Arrow arrow) {
  char* program = programOf(arrow);
  fprintf(stderr, "%s\n", program);
  free(program);
  return arrow;
}

static Arrow printArrowC(Arrow arrow) {
  char* program = programOf(arrow);
  fprintf(stderr, "%s", program);
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
     n = btag(sx->val_used - 1, sx->val); // One don't keep 0 terminator byte
  }
  
  if (sx->next) {
     a = arrowFromSexp(sx->next);
     return arrow(n, a);
  } else
     return n;
}

static sexp_t *sexpFromArrow(Arrow a) {
   int t = typeOf(a);
   
   switch (t) {
    case XL_EVE :
       return new_sexp_atom("Eve", 3, SEXP_BASIC);

    case XL_ARROW : {
       sexp_t *sexpHead = sexpFromArrow(headOf(a));
       sexp_t *sexpTail = sexpFromArrow(tailOf(a));
       sexpTail->next = sexpHead;
       return new_sexp_list(sexpTail);
    }
    case XL_BLOB : {
       char* fingerprint = toFingerprints(a);
       sexp_t* sexp = new_sexp_atom(fingerprint, strlen(fingerprint), SEXP_DQUOTE);
       free(fingerprint);
       return sexp;
    }
    case XL_TAG : {
       uint32_t tagSize;
       char* tag = btagOf(a, &tagSize);
       sexp_t* sexp = new_sexp_atom(tag, tagSize, SEXP_DQUOTE);
       free(tag);
       return sexp;
    }
    case XL_TUPLE :
    case XL_SMALL :
       assert(0);
       return NULL; // Not yet supported TODO
       
    default:
       return NULL;
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


char* xl_programOf(Arrow p) { // returned value to be freed by the user
  sexp_t *sx = sexpFromArrow(p);
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


static Arrow _resolve(Arrow a, Arrow e, Arrow M) {
  ONDEBUG((fprintf(stderr, "   _resolve a="), printArrow(a)));
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
   XLEnum enu = childrenOf(x);
   Arrow found = Eve();
   while (xl_enumNext(enu) && isEve(found)) {
      Arrow child = xl_enumGet(enu);
      if (isRooted(child))
         found = child;
   }
   xl_freeEnum(enu);
   if (!isEve(found))
      return headOf(found);
  
  return a;
}

static Arrow resolve(Arrow a, Arrow e, Arrow M) {
  ONDEBUG((fprintf(stderr, "resolve a="), printArrowC(a), fprintf(stderr, " in e="), printArrow(e)));
  Arrow w = _resolve(a, e, M);
  ONDEBUG((fprintf(stderr, "resolved in w="), printArrow(w)));
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
  ONDEBUG((fprintf(stderr, "transition M =="), printArrow(M)));

  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  Arrow e = tailOf(ek);
  Arrow k = headOf(ek);
  ONDEBUG((fprintf(stderr, "   p =="), printArrow(p), fprintf(stderr, "   e =="), printArrow(e), fprintf(stderr, "   k =="), printArrow(k)));
  
  if (isTrivial(p)) { // Trivial expression
     // p == v
     dputs("p == v");

     if (isEve(k)) return M; // stop, program result = resolve(v, e, M)
     
     if (tail(k) == continuation) { //special system continuation #e#
        // k == (continuation (<hook> <context>))
        dputs("    k == (continuation (<hook> <context>))");
        Arrow context = head(head(k));
        XLCallBack hook;
        char *hooks = str(tail(head(k)));
        int n = sscanf(hooks, "%p", &hook);
        assert(n == 1);
        free(hooks);
        M = hook(M, context);
        return M;
        
     } else if (tail(k) == evalOp) { // #e# special "eval" continuation
       // k == (eval kk)
       dputs("    k == (eval kk)");
       Arrow kk = head(k);
       Arrow w = resolve(p, e, M);
       M = a(w, a(e, kk)); // unstack continuation and reinject evaluation result as input expression
       return M;
       
     } else {
       // k == ((x (ss ee)) kk)
       dputs("    k == ((x (ss ee)) kk)");
       Arrow w = resolve(p, e, M);
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
       dputs("     v == t trivial");
       Arrow t = v;
       Arrow w = resolve(t, e, M);
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
         // rewriting program with an eval
         // pp = (let ((x (eval (escape v))) s))
         // shortcut: it leads to the following machine state
         // M = (v (e (eval ((x (s e)) k))))
         M = a(v, a(e, a(evalOp, a(a(x, a(s, e)), k))));
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

     int lambdaxSpecial = (isTrivial(v0) && tail(resolve(v0, e, M)) == lambdax);

     if (!lambdaxSpecial && !isTrivial(v1)) { // non trivial argument in application in let expression #e#
       // p == (let ((x v:(t0 v1)) s)) where v1 not trivial
       dputs("p == (let ((x v:(v0 v1)) s)) where v1 not trivial");
       
       // rewriting program to stage v1 and (v0 v1) evaluation
       // pp = (let ((p v1) (let ((x (v0 (var p))) s))))
       // TODO use (headOf v) instead of p as variable name
       Arrow pp = a(let, a(a(p, v1), a(let, a(a(x, a(v0, a(var, p))), s))));
       M = a(pp, ek);
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
      
     // p == (let ((x (t0 t1)) s)) where t0 is a closure or equivalent
     dputs("p == (let ((x v:(v0:t0 v1:t1) s))  # a really trivial application in let expression");
     Arrow t0 = v0;
     Arrow t1 = v1;
     Arrow C = resolve(t0, e, M);
     if (tail(C) == C) {
           dputs("wrong C ==> not reduced");
           M = a(s, a(_load_binding(x, v, e), k)); // short-cut
           return M;
     }
     
     if (tail(C) == operator) { // System call special case
           // C = (operator (hook context))
           dputs("C = (operator (hook context))");
           Arrow context = head(head(C));
           XLCallBack hook;
           char* hooks = str(tail(head(C)));
           int n = sscanf(hooks, "%p", &hook);
           assert(n == 1);
           free(hooks);
           M = hook(M, context);
           return M;
           
     }

     // closure case
     Arrow w;
     Arrow ee;
     if (tail(C) == lambdax) { // #e# special closure
             // C == (lambdax ((y ss) ee))
             dputs("C == (lambdax ((y ss) ee))");
             
             // special closure : C == ("lambdax" CC) w/ CC being a regular closure
             // - applied arrow is escaped by default (like in let construct)
             C = head(C);
             ee = head(C);
             w = t1; // t1 is NOT resolved

     } else {
             // C == ((y ss) ee)
             ee = head(C);
             dputs("C == ((y ss) ee)");
             w = resolve(t1, e, M);
     }
     Arrow y = tail(tail(C));
     Arrow ss = head(tail(C));
     M = a(ss, a(_load_binding(y, w, ee), a(a(x, a(s, e)), k))); // stacks up a continuation
     return M;
  }

  // application cases
  dputs("p == (s v)");
  Arrow s = tail(p);
  Arrow v = head(p);
   
  if (tail(v) == let) { // let expression as application argument #e#
    dputs("    p == (s v:(let ((x vv) ss))");
    // rewriting program with an eval
    // pp = (s (eval (escape v)))
    Arrow pp = a(s, a(evalOp, a(escape, v)));
    // TODO shortcut
    M = a(pp, ek);
    return M;
  }

  if (tail(v) == evalOp) { //eval expression as application argument #e#
     // p == (s (eval ss))
     dputs("     v == (eval ss) # an eval expression");
     Arrow ss = head(v);
     M = a(ss, a(e, a(evalOp, a(a(p, a(a(s, a(var, p)), e)), k)))); // stack an eval continuation
     return M;
  }

  int lambdaxSpecial = (isTrivial(s) && tail(resolve(s, e, M)) == lambdax);
  
  if (!lambdaxSpecial && !isTrivial(v)) { // Not trivial argument in application #e#
    dputs("    v == something not trivial");
    // simply rewriting program with a let
    // pp = (let ((p v) (s (var p))))
    Arrow pp = a(let, a(a(p, v), a(s, a(var, p))));
    M = a(pp, ek);
    return M;
  }

  dputs("    v == t trivial");
  Arrow t = v; // v == t trivial
  
  if (!isTrivial(s)) { // Not trivial closure in application #e#
    // simply rewriting program with a let
    // pp = (let ((p s) ((var p) t)))
    if (tail(s) == let) { // let expression as closure argument #e#
       dputs("p == (s:(let ((x v) ss)) t)");
       // shortcut: the rewriting rule leads to the following machine state
       M = a(s, a(e, a(evalOp, a(a(p, a(a(a(var, p), t), e)), k)))); // stack an eval continuation
       return M; 
    }
    
    dputs("p == (s v) where s is an application or such");
    Arrow pp = a(let, a(a(p, s), a(a(var, p), v)));
    M = a(pp, ek);
    return M;
  }

  // Really trivial application
  
  // p == (t0 t1) where t0 should return a closure or such
  dputs("    p == (t0:v t1:s) # a really trivial application");
  Arrow t0 = s;
  Arrow C = resolve(t0, e, M);
  Arrow t1 = head(p);
  
  if (tail(C) == C) {
    dputs("        wrong C ==> not reduced");
    M = a(a(escape, p), ek);
    return M;
  }

  // Continuation stacking not needed!

  if (tail(C) == operator) { // System call case
       // C == (operator (hook context))
       dputs("       C == (operator (hook context))");
       Arrow context = head(head(C));
       XLCallBack hook;
       char* hooks = str(tail(head(C)));
       int n = sscanf(hooks, "%p", &hook);
       assert(n == 1);
       free(hooks);
       M = hook(M, context);
       return M;
       
  } else { // closure case
       Arrow w;
       Arrow ee;
       if (tail(C) == lambdax) { // #e# special closure where applied arrow is not evaluated (like let)
         // C == (lambdax ((y ss) ee))
         dputs("        C == (lambdax ((y ss) ee))");
         
         // special closure : C == ("lambdax" CC) w/ CC being a regular closure
         // - applied arrow is escaped by default (like in let construct)
         C = head(C);
         ee = head(C);
         w = t1; // t1 is NOT resolved

       } else {
         // C == ((x ss) ee)
         dputs("        C == ((x ss) ee)");
         ee = head(C);
         w = resolve(t1, e, M);
       }
       Arrow x = tail(tail(C));
       Arrow ss = head(tail(C));
       M = a(ss, a(_load_binding(x, w, ee), k));
       return M;   
  }
}

Arrow xl_run(Arrow rootStack, Arrow M) {
  machine_init();
  M = transition(M);
  while (!isEve(head(head(M))) || !isTrivial(tail(M))) {
    M = transition(M);
  }
  ONDEBUG((fprintf(stderr, "finished with M="), printArrow(M)));
  Arrow p = tail(M);
  Arrow e = tail(head(M));
  return resolve(p, e, M);
}

Arrow xl_eval(Arrow rootStack, Arrow program) {
  machine_init();
  Arrow M = a(program, a(systemEnvironment, Eve()));
  return run(rootStack, M);
}

Arrow xl_argInMachine(Arrow M) {
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
  ONDEBUG((fprintf(stderr, "   argument is "), printArrow(arg)));
  assert(isTrivial(arg));
  Arrow w = resolve(arg, e, M);
  return w;
}

Arrow xl_reduceMachine(Arrow M, Arrow r) {
  Arrow p = tailOf(M);
  Arrow ek = headOf(M);
  if (tail(p) == let) {
    // p == (let ((x (<operator> arg)) s))
    ONDEBUG((fprintf(stderr, "   let-application reduced to r="), printArrow(r)));
    Arrow x = tail(tail(head(p)));
    Arrow s = head(head(p));
    ONDEBUG((fprintf(stderr, "     s = "), printArrow(s)));
    Arrow e = tail(ek);
    Arrow k = head(ek);
    ONDEBUG((fprintf(stderr, "     k = "), printArrow(k)));
    Arrow ne = _load_binding(x, r, e);
    M = a(s, a(ne, k));                                      
  } else {
    ONDEBUG((fprintf(stderr, "   application reduced to r="), printArrow(r)));
    M = a(a(escape, r), ek);
  }
  return M;
}

Arrow tailOfHook(Arrow M, Arrow context) {
   Arrow arrow = xl_argInMachine(M);
   Arrow r = tail(arrow);
   return xl_reduceMachine(M, r);
}

Arrow headOfHook(Arrow M, Arrow context) {
   Arrow arrow = xl_argInMachine(M);
   Arrow r = head(arrow);
   return xl_reduceMachine(M, r);
}

Arrow childrenOfHook(Arrow M, Arrow context) {
   // usage: (childrenOf parent)
   Arrow parent = xl_argInMachine(M);
   XLEnum e = childrenOf(parent);
   Arrow list = Eve();
   while (xl_enumNext(e)) {
      Arrow child = xl_enumGet(e);
      list = a(child, list);
   }
   xl_freeEnum(e);
   return xl_reduceMachine(M, list);
}

Arrow rootHook(Arrow M, Arrow context) {
   Arrow arrow = xl_argInMachine(M);
   Arrow r = root(arrow);
   return xl_reduceMachine(M, r);
}

Arrow unrootHook(Arrow M, Arrow context) {
   Arrow arrow = xl_argInMachine(M);
   Arrow r = unroot(arrow);
   return xl_reduceMachine(M, r);
}

Arrow isRootedHook(Arrow M, Arrow context) {
   Arrow arrow = xl_argInMachine(M);
   Arrow r = isRooted(arrow);
   return xl_reduceMachine(M, r);
}

Arrow ifHook(Arrow M, Arrow context) {
  // the ifHook performs some magic
  Arrow body = xl_argInMachine(M);
  Arrow condition = tail(body);
  Arrow alternative = head(body);
  Arrow branch;
  if (isEve(condition))
     branch = head(alternative);
  else
     branch = tail(alternative);
  Arrow p = tail(M);
  Arrow ek = head(M);
  
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

Arrow commitHook(Arrow M, Arrow context) {
   root(a(selfM, M));
   commit();
   unroot(a(selfM, M));
   return xl_reduceMachine(M, Eve());
}

static struct fnMap_s {char *s; XLCallBack fn;} systemFns[] = {
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
    Arrow op = a(tag(systemFns[i].s), operator(systemFns[i].fn, Eve()));
    systemEnvironment= a(op, systemEnvironment);
  }
  root(a(reserved, systemEnvironment));
}

void xl_init() {
  int rc = space_init();
  assert(rc >= 0);
}
