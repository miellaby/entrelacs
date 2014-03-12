/* File:   entrelacs.h
 * Entrelacs core API
 */

#ifndef _ENTRELACS_H
#define _ENTRELACS_H

#ifdef	__cplusplus
extern "C" {
#endif


/** @file
 * Entrelacs system public API.
 *
 * Everything but the Arrow type is XL prefixed to prevent name conflicts.
 * Consider including entrelacsm.h to get handy macros.
 */


#include <stdint.h>

/** Eve. */
#define EVE (0)
#define NIL (0xFFFFFFFFU)

/** Arrow ID. */
typedef uint32_t Arrow; ///< ArrowId = SpatialRef (may be enforced by a transaction ID in a near future)

extern const Arrow Eve; // equals EVE

/** initialize the system. */
int  xl_init();
void xl_destroy(); ///< release hardware locks and such.

/* Assimilation */
Arrow xl_Eve(); ///< returns Eve.
Arrow xl_pair(Arrow, Arrow); ///< assimilate a pair of arrows.
Arrow xl_atom(char*); ///< assimilate a C string
Arrow xl_atomn(uint32_t size, char*); ///< assimilate raw data
Arrow xl_uri(char*); ///< assimilate an URI, returning an arrow (NIL if URI is wrong)
Arrow xl_urin(uint32_t size, char*); ///< assimilate a piece of URI, returning an arrow (NIL if URI is wrong)

/* Compound arrow assimilation */
Arrow xl_anonymous();      ///< assimilate a randomized value so to get an unique "anonymous" arrow.
                           ///< This is NOT the way to knowledge representation within Entrelacs.
Arrow xl_hook(void* hook); ///< assimilate a C pointer and return an arrow.
                           ///< unbuild with xl_pointerOf.
                           ///< Bottom-rooted to distinguish from evil attack attempt.
                           ///< Unroot to neutralize (xp_pointerOf returning NULL).
                           ///< Auto-neutralized after reboot.

/* Probe arrows without creating them if missing
 */
Arrow xl_pairMaybe(Arrow, Arrow); ///< return a pair of arrows if system-known, Eve otherwise.
Arrow xl_atomMaybe(char*); ///< return the already assimilated arrow corresponding to a C string, Eve otherwise.
Arrow xl_atomnMaybe(uint32_t size, char*); ///< return the already assimilated arrow corresponding to a raw piece of data, Eve otherwise.
Arrow xl_uriMaybe(char*); ///< return the previously assimilated arrow corresponding to an URI, NIL if wrong URI, EVE if arrow not assimilated.
Arrow xl_urinMaybe(uint32_t size, char*); ///< return the previously assimilated arrow corresponding to a piece of URI, NIL if wrong URI, EVE if arrow not assimilated.
Arrow xl_digestMaybe(char*); ///< return a stored arrow corresponding to a digest, NIL if no match.

/* Unbuilding */
typedef enum e_xlType {
    XL_UNDEF=-1,
    XL_EVE=0,
    XL_PAIR=1,
    XL_ATOM=2
} XLType;

XLType xl_typeOf(Arrow); ///< return arrow type. // TODO: could it be a SMALL arrow ?
Arrow xl_headOf(Arrow); ///< return arrow head.
Arrow xl_tailOf(Arrow); ///< return arrow tail.
char* xl_strOf(Arrow);  ///< retrieve an atomic arrow as a C string. @return pointer to freed.
char* xl_memOf(Arrow, uint32_t* size_p); /// retrieve an atomic arrow as a piece of binary data. @return pointer to freed.
char* xl_uriOf(Arrow, uint32_t* size_p); ///< Retrieve an arrow definition in URI notation. @return pointer to freed.
uint32_t xl_hashOf(Arrow); ///< return the arrow hash.
char* xl_digestOf(Arrow, uint32_t* size_p); ///< Retrieve an arrow digest. @return pointer to freed.
void* xl_pointerOf(Arrow); //< Retrieve the C pointer corresponding to a "hook" arrow. @return pointer.

/* Rooting */
Arrow xl_root(Arrow); ///< root an arrow.
Arrow xl_unroot(Arrow); ///< unroot an arrow.

/* Transaction */
void xl_begin();  ///< increment the global transaction counter. Other transactions will be synced with the one of this calling thread (or xl_over)
void xl_yield(Arrow); ///< perform GC, only preserving one "state" arrow. wait for all threads being ready.
void xl_over();   ///< decrement the global transaction counter. For example, before thread termination. Any previously assimilated arrow should be assimilated again.
void xl_commit(); ///< commit. wait for all transactions being over.
                  ///< Previously assimilated arrow must be assimilated again as they may have been forgotten if loose.

/* Testing */
int xl_isEve(Arrow); ///< returns !0 if equals Eve.
int xl_isRooted(Arrow); ///< returns !0 if rooted.
int xl_equal(Arrow, Arrow); ///< returns !0 if arrows are equal.
Arrow xl_isAtom(Arrow); ///< returns given arrow if an atom, else Eve.
Arrow xl_isPair(Arrow); ///< returns given arrow if a pair, else Eve.

/* Browsing */
#define EOE ((XLEnum)0xFFFFFFF)
typedef void* XLEnum; ///< enumerator type, as returned by xl_childrenOf.
int    xl_enumNext(XLEnum); ///< iterate enumerator. Return 0 if exhausted. !0 otherwise.
Arrow  xl_enumGet(XLEnum); ///< get current arrow from enumerator.
void   xl_freeEnum(XLEnum); ///< free enumerator.

XLEnum xl_childrenOf(Arrow); ///< return all known children of an arrow as an enumerator.
                             ///< enumerator must be freed by xl_freeEnum
Arrow  xl_childOf(Arrow); ///< return only one child or Eve if there's none. If several children, result is stochastisc( TBC?).

typedef Arrow (*XLCallBack)(Arrow arrow, Arrow context); ///< generic callback type for arrow fetching
void xl_childrenOfCB(Arrow, XLCallBack, Arrow context); ///< apply a given function to each children of an arrow

/* Assimilate binding */
Arrow xl_operator(XLCallBack hook, Arrow); ///< assimilate a C implemented operator.
Arrow xl_continuation(XLCallBack hook, Arrow); ///< assimilate a C implemented continuation.

/** run an Entrelacs Machine.
 * $M is a machine state.
 * $contextPath is the path representating a nested hierarchy of contexts (/C0.C1...Cn).
 */
Arrow xl_run(Arrow contextPath, Arrow M, Arrow session); ///< M == (<program> (<environment> <continuation-stack>))

/** Eval a program by building and running a machine */
Arrow xl_eval(Arrow contextPath, Arrow program, Arrow session);


#ifdef	__cplusplus
}
#endif


#endif // entrelacs.h
