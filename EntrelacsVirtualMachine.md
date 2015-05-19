This page is a work in progress. See EntrelacsAbstractMachine as well.

## Introduction ##
The Entrelacs Machine (EM) is a computing environment which executes programs stored as arrows graphs in the ArrowsSpace.

The Entrelacs Machine recognizes a function oriented minimalist  language described hereafter.

The Entrelacs machine design is somewhat inspired from the CaEK Machine described in flanagan93essance paper. But, EM actually encompasses traditional functional paradigm by reorganizing function related concepts (environment, Lambda term, closure...)in a new, simpler and more general reading.

The major improvement resides in a wider homoiconism, bluring distinction between values and non-values, free-variables and constants.

A running environment is not a symbol lookup table anymore. It's rather a function which compute an arrow from another.  It's defined as a composition (chain) of elementary functions such as classical variable-to-value bindings and arrow rewriting rules.

So variable-to-value bindings form a particular use case of a much more powerful scheme. For example, one may either rewrite some call schemes (like sort of macros). One may even define an environment which inverts (ie. swap tail and head) every evaluated arrow.

When applying an arrow to an environment, we refer to meta environments which may bind and rewrite things among the application. Such a meta environment may even refer to a a upper-level environment and so one.

## recognized constructs ##

```
C#1  "="->(A->B)
C#2 "^"->(pattern->replace)
C#3  ":"->A 
C#4   A->":"
C#5 "if"->(cond->(A->B))
```

  * C#1 construct "binds" some arrow (a free variable) to some other (variable value).
  * C#2 construct adds a macro-like arrow into the environment.
  * C#3 adds A to the environment, then the applied arrow is evaluated
  * C#4 adds first the applied arrow to the environment then evaluates A.
  * C#5 is a classical control structure. This construct evals 'A' if 'cond' is evaluated to 0, otherwise it evals 'B'.


Note C#3 corresponds to the functional construct (let variable value body) and C#4 to (lambda variable body value). Or rather: (let environment-add-in body) and (lambda body environment-add-in).

In addition, some primitive operations are stored in the global environment. Side effects occures when they are bound.

## machine state ##

The machine state S is an arrow in the form: R->(A->K).

### context R ###
R is an arrow forming a chain of match&replace rules or match&execute rules.
  * R takes the place of the Environment (E) in a CEK Machine.
  * However R may substitute any arrow, not only "atomic" ones. That's Homoiconism.
  * R is in the form: (M0->(M1->(...->(Mn->0))) where Mi are match&replace rules.
  * Each Mi rule is in an arrow in the form: (pattern->replace) or in the form: (pattern->pluginCallback).
  * Arrow matching pattern (Mi tail) can embed "jokers" in order to define an extensive set of matchable arrows.
  * Arrow replacement pattern (Mi head) can be either:
    * a remplacement arrow which may reuse the matched arrow components. We might speak about arrows "macro".
    * a plugin callback which will be called with the matched arrow as parameter. It corresponds to "primitive operation" in CEK Machine. The callback returns an arrow which takes place of the matching arrow.

### controlling arrow A ###
A is an arrow controlling the next transition state of the machine.
  * A takes place of the Control String (C) in a CEK Machine
  * A corresponds either to a part of the source tree, or to a "closure". A closure is an arrow in the form ("closure"->(x->(R->A))) created when storing a "lambda" expression in R


### continuation chain K ###
K is a "0" terminated chain of  continuations. K is an arrow in the form:  ` (x->(R->(A->(x->(R->(A->(x->...->(R->(A->0))...))))))) `
  * this chain of continuations takes the place of the continuation (K) in the CEK machine.
    * There is only one kind of continuation in the form x->(A->(R->K)). It actually corresponds to an "ar" continuation in a CaEK machine.
    * the tail of the continuation (named x here) corresponds to the "x" variable of an "ar" continuation.
    * If A is in its normal form (T#1, T#2, T#3, T#4 not applicable), the current continuation is proceed (transition T#0 herefater).

## machine transitions ##
```
T#0: R->(A->(x->(R'->(A'->K'))) => ((x->matching[A,R])->R')->(A'->K')

T#1: R->((("^"->(pattern->replace))->A)->K) => ((matching[pattern,R]->matching[replace,R])->R)->(A->K)

T#2: R->((("if"->x)->(A1->A2))->K) => R->(A1->K) if matching[x,R] != 0 or R->(A2-K) otherwise

T#3: R->(V->A)->K) => ((x->matching[A,R])->R')->(A'->K) if matching[V,R] = ("cloture"->(x->(R'->A')))

T#4: R->((("^"->(pattern->(V->B))->A)->K) => ((x->matching[B,R])->R')->(A'->(pattern->(R->(A->K))))) if matching[V,R] = ("cloture"->(x->(R'->A')))
```
... where ...
```
matching#0: matching[(pattern->program)->'"^", R] = ("closure"->(pattern->(R->program)))

matching#1: matching[X] = Y obtained by computation every matching rules in R matching chain + global matching rules
```

## state initialisation ##
```
Eval(program) => S = (0->(program->0))
```

In other mean: R: =0; A := program; K: = 0

## system iteration ##
The system runtime cycle is a "_system iteration_". It consists in a interaction loop receiving arrows one by one and applying the system behavior function to them.

_interaction loop_:
```
   repeat forever:
     read one input arrow
     compute {behavior := behavior(input)} ; with side effects
```

### micro transaction ###

Each system iteration is confined into a "arrows space transaction" so that Entrelacs iterates between stable persistent states.

## rooted arrows ##

This system behavior function actually correspond to the _root context_ of the system. This context is formed by flagged arrows among the arrows space, namely the "_rooted arrows_".

## arrow pattern matching and rewriting rules ##

Rooted arrows may be seen as pattern based rewriting rules. These rules defines how some arrows lead to (generate) some other arrows.

## arrows combinatory logic ##

Actually, Entrelacs rewriting rules are recombination rules. It means they are build with **combinators**, that is operators which don't relay on "_variables_".

As is, "Lambda barre" language is nearest from "combinatory logic" theoretical language  than "lambda calculus"
  * See http://en.wikipedia.org/wiki/Combinatory_logic.
  * See EntrelacsLanguage for details.

## Universal cache ##
Entrelacs Virtual Machine operates the ArrowsSpace as an universal cache. In other means, incoming or computed arrows may be cached in the ArrowSpace. Such a cache dramatically improves communication and computing. It works both as a data cache (see http://en.wikipedia.org/wiki/Cache) and a computation cache (see http://en.wikipedia.org/wiki/Memoization). Work in progress.