/**
 * @file machine.h
 * @brief Entrelacs Machine API
 * @details
 *
 * The Entrelacs machine is a state machine inspired by the CaEK abstract machine of Flanagan et al.
 * Read [the essence of compiling with continuations](https://users.soe.ucsc.edu/~cormac/papers/pldi93.pdf)
 * for an overview of the maching functioning.
 *
 * The Entrelacs machine processes a given program $p in a context $C. Its state is $M
 *
 * $M is a machine state; $M =/$p/$e+$k
 * - $p = [A-normal form](https://en.wikipedia.org/wiki/A-normal_form) of a program in λ-calculus language
 * - $e = environment
 *   <environment> ::= Eve | ((<variable> <value>) <environment>)
 *   $e = //$xn+$wn//...//$x2+$w2//$x1+$w1//$x0+$w0+Eve
 * - $k = continuation chain
 *   $k = either Eve or /$f+$kk where:
 *   - $f is a frame in the form /$x/$ss+$ee
 *     - $x is the variable name to bind the result of the evaluation of $p
 *     - $ee is the environment where the bound varaible $x is added
 *     - $ss is the next program to evaluate once $ee is completed
 *   - $kk is the next continuation
 *
 * The machine runs until it reaches a terminal state, which is either:
 * - a trivial program (a value) and the Eve (stop) continuation (returning the value)
 * - a swear (error)
 *
 * Language basis: "A(CS)"
 *   - A(CS): “Core Scheme in A-normal form” language as introduced in pldi93.pdf paper.
 *   - λ-calculus language
 *   - Only trivial expressions (variables, constants, and λ-terms) may serve as argument of applications
 *   - A non-trivial expression (eg. application) is always captured by a let-bound variable.
 *
 * A(CS) adaptation to arrows:
 *   - Any atom arrow is proceed as a variable name by default unless escaped
 *   - Unbound variables are left as is. Evaluating “hello” returns “hello” if not a bound variable.
 *   - Any pair arrow will be proceed as an application unless escaped.
 *   - An application on a non-closure arrow is left as is. (“hello” “world”) returns (“hello” “world”)
 *     if “hello” is not bound to a closure.
 *   - /escape+$arrow prevents $arrow evaluation and turns $arrow into a term (a litteral arrow)
 *   - /var+$arrow prevents $arrow evaluation and turns $arrow into a substitutable variable name
 *   - /arrow+$template prevents evaluation of $template but substitute descendants explicitly casted
 *      as variables with /var+$arrow constructs
 *
 * Other Language extensions:
 * - Non A-normal form expressions are compiled into their ANF counterpart on the fly, such
 *   the application (d a) where d not trivial, is transformed into (let (tmp d) (tmp a))
 *   the application (d a) where a not trivial, is transformed into (let (tmp a) (d tmp))
 *   the application (d a) where both d and a not trivial, is transformed into (let ((tmp d) d) (let ((tmp a) a)) ((var (tmp d)) (var (tmp a))))
 *      tmp is actually the machine state casted as a variable name.
 *   (let ((x (let ((y w) ss)) s)) is transformed into (let ((y w) (let ((x ss) s)))
 *   (let ((v (d a)) s) where d or a not trivial is transformed into ANF as well
 * - @M is a pseudo variable returning the current machine state as a whole
 * - (operator (hook context) stores C-coded closure
 * - (continuation (hook context)) stores C-coded system continuation
 * - (macro ($x $expression)) performs macro substitution.
 *   It turns into a "paddock" in the same way as a lambda expression turns into a closure.
 *   During macro application, the argument is escaped, then bound to $x, then the paddock $expression
 *   is evaluated, then the result is evaluated a second time as a program.
 * - rlambda are recursive lambda ("it" is bound to the rlambda while evaluating its expression)
 * - (eval $expression) forces a second evaluation of $expression
 * - many system functions for context handling and arrow management
 *   * get, set, reset (setVar unsetVar getVar operators)
 *   * link, unlink, browse
 *   * root, unroot, isRooted, list
 *   * isPair, tailOf, headOf, childrenOf, childrenReviewOf
 *   * enter sub-context, commit, exit secret, land
 *   * run $exp
 *   * if construct based on the branch operator
 *   * equal construct basedd on the isClone operator
 *   * digest $hash
 *   * (load ((x v) s)) loads a variable x with value v and evaluate s
 */
#include "machine/transient.h"

/**
 * @brief assimilate a C implemented operator.
 */
Arrow xs_operator(XSCallBack hook, Arrow);

/**
 * @brief assimilate a C implemented continuation.
 */
Arrow xs_continuation(XSCallBack hook, Arrow);

/**
 * @brief run an Entrelacs Machine.
 */
Arrow xs_run(Arrow C, Arrow M, Arrow session);

/**
 * @brief eval a program by building and running a machine
 * @param C the context where the program is evaluated.
 * If NULL, the default context [Eve, Session] is used, but it
 * might be changed with land operator.
 */
Arrow xs_eval(Arrow C, Arrow program, Arrow session);
