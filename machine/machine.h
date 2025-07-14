#include "machine/transient.h"

// Assimilate bindings

Arrow xs_operator(XSCallBack hook, Arrow); ///< assimilate a C implemented operator.

Arrow xs_continuation(XSCallBack hook, Arrow); ///< assimilate a C implemented continuation.

/** run an Entrelacs Machine.
 * $M is a machine state.
 * $contextPath is the path representating a nested hierarchy of contexts (/C0.C1...Cn).
 */
Arrow xs_run(Arrow contextPath, Arrow M, Arrow session); ///< M == (<program> (<environment> <continuation-stack>))

/** Eval a program by building and running a machine */
Arrow xs_eval(Arrow contextPath, Arrow program, Arrow session);
