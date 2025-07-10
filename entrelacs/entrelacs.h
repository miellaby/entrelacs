/** @file
 * Entrelacs system public API.
 *
 * Everything but the Arrow type is XL prefixed to prevent name conflicts.
 * Consider including entrelacsm.h to get simpler macros.
 */

#ifndef _ENTRELACS_H
#define _ENTRELACS_H

#include "entrelacs/arrow_space.h"

#ifdef	__cplusplus
extern "C" {
#endif
/// @brief  Transient Arrow
typedef struct xs_arrow_s* Arrow;

/* Assimilate binding */
Arrow xs_operator(XLCallBack hook, Arrow); ///< assimilate a C implemented operator.
Arrow xs_continuation(XLCallBack hook, Arrow); ///< assimilate a C implemented continuation.

/** run an Entrelacs Machine.
 * $M is a machine state.
 * $contextPath is the path representating a nested hierarchy of contexts (/C0.C1...Cn).
 */
Arrow xs_run(Arrow contextPath, Arrow M, Arrow session); ///< M == (<program> (<environment> <continuation-stack>))

/** Eval a program by building and running a machine */
Arrow xs_eval(Arrow contextPath, Arrow program, Arrow session);


#ifdef	__cplusplus
}
#endif


#endif // entrelacs.h
