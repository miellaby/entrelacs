# Introduction #

The second major component of an Entrelacs System beside the ArrowsSpace is the _Entrelacs Abstract Machine_. It's a software machine which evals programs directly stored as arrows constructs.

Programs are defined according to the [Entrelacs Language](EntrelacsLanguage.md) (EL). This language is designed so that there is no difference between, source code, CST (Concrete Syntax Tree), and AST (Abstract Syntax Tree). The objective is to simplify meta-programming and persistent reflexivity.

The machine state itself consists in an arrow construct which may be read and modified by the running program. A programs may consequently handle all the underlying machine state components like:
  * environments,
  * continuations,
  * closures,
  * evaluated expression
  * etc.

It obviously implies that **the machine state itself takes part of the language definition**.

## Sources and credits ##
The _Entrelacs Machine_ described hereafter is largely inspired by the
["\_CaEK\_" abstract machine of Flanagan et al. (1993)](http://users.info.unicaen.fr/~karczma/TEACH/Doc/compi_cont.pdf).

So the EL language is roughly equivalent to the "Core Scheme in A-normal form" language introduced in this paper (aka _A(CS)_).

In short, it's a very basic (not typed) functional language. The only thing to keep in mind is that programs and machine states are directly hold by arrows structures.

Textual source code is reserved for communication purpose. The current prototype should be able to assimilate arrow in the form of s-expression or URI (see [EntrelacsServer](EntrelacsServer.md) API).

## Extensions to the very strict A(CS)_language ##_

The EL language brings few modifications to the _A(CS)_ language. Most of these experiments are not documented here. See source code.

These extensions include:
  * Arrow casting and escaping. Any arrow may be seen as a variable symbol, or an expression, or a literal.
  * ~~Experimental: The possibility to eval expressions which are not in A-normal form.~~
  * Unbound variables are left as is. Evaluating "hello" returns "hello" if not a bound variable.
  * Application on something other than a closure is left as is. ("hello" "world") returns ("hello" "world") if "hello" is not bound to a closure and "world" not bound.
  * System continuations
  * the "@M" keyword is bound to the current machine state
  * a run keyword allows to replace the current machine state. TODO.

## Restrictions ##

Functions are one-variable only.

# The Language #
```
x ::= any entrelacs arrow (Eve, tags, blobs)
a ::= x | (a a) // any arrow

// literals
v ::=  x // entrelacs are resolved if bound, otherwise left as is
       | ("escape" a) // escape any arrow (prevent expression evaluation) #e#
       | ("var" a) // escape and cast any arrow as a variable #e#

// trivial expression
t ::=  	v
 	| ("lambda" (v s)) // lambda expression

// serious expression in A-normal form
s ::=	("let" ((v t) s)) // binding
  	| ("let" ((v (t t)) s)) // application in binding
  	| (t t) // application

// not A-normal form expressions
z :== ... see source code ...

// EL expression
e ::=   s | z

// EL program
p ::= e
```

Missing: operators and system continuations.

# The Machine #

## Trivial expressions ##

```
<arrow> ::= any arrow
<entrelacs> ::= any entrelacs arrow
<escape> ::= ("escape" <arrow>)
<var> ::= ("var" <arrow>)
<lambda-expression> ::= ("lambda" (<arrow> <expression>))
<trivial> ::= <entrelacs> | <lambda-expression> | <escape> | <var>
```

## Programs ##
```
<binding> ::= ("let" ((<variable> <trivial>) <serious>))
<application-binding> ::= ("let" ((<variable> <application>) <serious>))
<application> ::= (<trivial> <trivial>)
<serious> ::= <binding> | <application-binding> | <application>
<extensions> ::= ... see code ...
<expression> ::= <trivial> | <serious> | <extensions>
<program> ::= <expression>
```

## Environment and closure ##

```
<variable> ::= <arrow>
<value> ::= <arrow>

<environment> ::= Eve | ((<variable> <value>) <environment>)
/* ((<variable> <value>) ((<variable> <value>)... ((<variable> <value>) Eve) ... )) */

<closure> ::= ((<variable> <expression>) <environment>)
```

## The Continuation chain, aka stack ##

```
<frame> ::= (<variable> (<expression> <environment>))
/* TODO: is not a frame actually a closure? */

<system-continuation> ::= <continuation (<hook> <context>) /* System continuation */

<continuation-chain> ::= /* (<frame> (<frame> ... (<frame> Eve) ... )) */
      Eve
    | <system-continuation>
    | (<frame> <continuation-chain>)
```


## The Machine State ##

```
<machine-state> := (<program> (<environment> <continuation-chain>))
```

## System functions and variables ##
Uncompleted list
  * head
  * tail
  * childrenOf
  * root
  * unroot
  * isRooted
  * run : replace the current machine state with the provided one.
  * "@M" : pseudo variable; return current machine state

## Machine dynamics ##

### _resolve_ function ###

```
  Arrow resolve(Arrow trivial, Arrow environment)
```

Pseudo pattern matching algo:
```
let resolve(t, e) =
   match t with:
     | ("lambda" (x s)) -> ((x s) e) /* closure */
     | ("var" x) -> resolveVariable(x, e)
     | ("escape" a) -> a
     | "@M" -> M (current machine state)
     | x -> resolveVariableIfBound(x, e)

let resolveVariableIfBound(x, e) =
   match e with:
     | Eve -> x /* Miss: An unbound variable is left as is*/
     | ((x value) head) -> value /* Hit */
     | (tail head) -> resolveVariable(x, head) /* recursion */
```

### _isTrivial_ function ###
Pseudo-C algo:
```
   Boolean isTrivial(Arrow expression) = {
     return
        isEntrelacs(exp)
     || tail(exp) == "lambda"
     || tail(exp) == "escape"
     || tail(exp) == "var"
   }
```

### _eval_ function ###

Pseudo-C algo:
```
Arrow eval(program) {
  M = (program (Eve Eve))
  M = transition(M);
  while (head(head(M)) != Eve) {
    M = transition(M)
  }
  p = tail(M)
  e = tail(head(M))
  return resolve(p, e)
}

```

### _transition_ function ###

Pseudo-C algo:
```
#define _(a,b) = arrow(a,b)

Arrow transition(Arrow M) {
  // match M to (p (e k))
  p = tail(M)
  e = tail(head(M))
  k = head(head(M))

  if (isTrivial(p)) { // Trivial expression
     // p = v

     // if no more continuation
     if (k == Eve) return // stop, program result = resolve(p, e)


     if (tail(k) == continuation) { //special system continuation
        // k = (continuation (<hook> <context>))
        hook = castAsPointer(tail(head(k)))
        context = head(head(k))
        M = hook(M, context)
     } else {
       // k = ((x (ss ee)) kk)
       w = resolve(p, e)
       kk = head(k)
       f = tail(k)
       x = tail(f)
       ss = tail(head(f))
       ee = head(head(f))
       M = _(ss, _(_(_(x, w), ee), kk)) // unstack continuation, postponed let solved
     }
  } else if (tail(p)) == let) { //let expression family
     // p = (let ((x v) s))
     v = head(tail(head(p))
     x = tail(tail(head(p))

     // when the var construct is used as first parameter of a let expression, it forces variable name resolution
     if (tail(x) == "var") { x = resolve(x); }

     if (isTrivial(v)) { // Trivial let rule
       // p = (let ((x t) s))
       t = v
       w = resolve(t, e)
       s = head(head(p))
       M = _(s, _(_(_(x, w), e), k))
     } else /* !isTrivial(v) */ { // Non trivial let rules
       // p = (let ((x (t0 s1) s))
       t0 = tail(v)
       v1 = head(v)
       if (!isTrivial(v1)) { // Not A-normal form
         ... see code ...
       } else /* isTrivial(v1)) */ { // Serious let rule
         // p = (let ((x t) s)) where t = (t0 t1)
         t1 = v1
         t0 = tail(t)
         C = resolve(t0, e)
         if (tail(C) == "operator") { // Operator call
           // C = (operator (hook context))
           hook = getPointer(tail(head(C)))
           context = head(head(C))
           // M = _(t, _(e, _(_(x, _(s, e)), k))) // polish the machine state. TODO
           M = hook(M, context) // operator performs the transition by itself

         } else { // closure case
           // C = ((y ss) ee)
           t1 = head(t)
           w = resolve(t1, e)
           ee = head(C)
	   y = tail(tail(C))
           ss = head(tail(C))
 	   M = _(ss, _(_(_(y, w), ee), _(_(x, _(s, e)), k))) // stacks up a continuation
         }
       }
     }
  } else if (!isTrivial(s = head(P))) { // Not A-normal form
     ... see code ...

  } else { // application rule
     // p = (t0 t1)
     // Stacking not needed
     t0 = tail(p)
     C = resolve(t0, e)
     if (tail(C) == operator) { // Operator call
       // C = (operator (hook context))
       hook = getPointer(tail(head(C)))
       context = head(head(C))
       M = hook(M, context)  // operator performs the transition by itself
     } else { // closure case
       // C = ((x ss) ee)
       t1 = head(p)
       w = resolve(t1, e)
       x = tail(tail(C))
       ss = head(tail(C))
       ee = head(C)
       M = _(ss, _(_(_(x, w), ee), k))
     }
  }
}
```

### Differences with actual code ###

Last update: 02/2012

The actual machine also features
  * (eval p)
    * It stacks an "eval" continuation so that:
    * the machine evaluates the "eval parameter" as en EL expression.
    * the machine then evaluates the result as an EL program.
    * p=(eval s)
      * ==> M=(s (e (eval k)))
    * p=(let ((x (eval ss) s))
      * ==> M=(ss (e (eval ((x (s e)) k))))
    * p=(s (eval ss))
      * ==> M=(ss (e (eval ((p ((s (var p)) e)) k))))
  * ~~Few language extensions are rewritten with "eval"~~ bad idea
  * Direct load of binding
    * (let ((Eve binding) exp)) means that  _binding_ is directly put into environment (as a var->value arrow)
    * p=(load (s0 s1)) <==> p=(let ((Eve s0) s1)))
  * ~~p == (let ((x (v0 v1)) s)) ==> pp = (let ((p v0) (let ((x ((var p) v1)) s))))~~ no
  * a lambdax construct.
    * defines special closure : C == ("lambdax" CC) with CC a regular closure
    * special closure where application argument is not evaluated.
    * Allows to make constructs like "let", "if".

## A normalization ##
An higher functional language might be interpreted by performing A-normalization and various optimization of code to obtain the corresponding optimized A-normal form to be evaluated by the Abstract Machine above.