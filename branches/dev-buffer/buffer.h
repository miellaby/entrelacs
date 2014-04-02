/* File:   buffer.h
 * Buffered Entrelacs API
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#ifdef	__cplusplus
extern "C" {
#endif


/** @file
 * Buffered Entrelacs API.
 */


#include <stdint.h>

/** Eve. */
#define EVE (0)
#define NIL (0xFFFFFFFFU)

/** Buffered Arrow ID. */
typedef uint32_t BArrow; ///< Buffered ArrowId
extern const BArrow Eve; // equals EVE

/** initialize the system. */
int  bxl_init();
int  bxl_reset();
void bxl_destroy();

/* Assimilation */
BArrow bxl_Eve(); ///< returns Eve.
BArrow bxl_pair(BArrow, BArrow); ///< assimilate a pair of arrows.
BArrow bxl_atom(char*); ///< assimilate a C string
BArrow bxl_atomn(uint32_t size, char*); ///< assimilate raw data
BArrow bxl_uri(char*); ///< assimilate an URI, returning an arrow (NIL if URI is wrong)
BArrow bxl_urin(uint32_t size, char*); ///< assimilate a piece of URI, returning an arrow (NIL if URI is wrong)

/* Compound arrow assimilation */
BArrow bxl_anonymous();      ///< assimilate a randomized value so to get an unique "anonymous" arrow.
BArrow bxl_hook(void* hook); ///< assimilate a C pointer and return an arrow.
                           ///< unbuild with bxl_pointerOf.
                           ///< Bottom-rooted to distinguish from evil attack attempt.
                           ///< Unroot to neutralize (bxl_pointerOf returning NULL).
                           ///< Auto-neutralized after reboot.

/* Probe arrows without creating them if missing
 */
BArrow bxl_pairIfAny(BArrow, BArrow); ///< return a pair of arrows if system-known, Eve otherwise.
BArrow bxl_atomIfAny(char*); ///< return the already assimilated arrow corresponding to a C string, Eve otherwise.
BArrow bxl_atomnIfAny(uint32_t size, char*); ///< return the already assimilated arrow corresponding to a raw piece of data, Eve otherwise.
BArrow bxl_uriIfAny(char*); ///< return the previously assimilated arrow corresponding to an URI, NIL if wrong URI, EVE if arrow not assimilated.
BArrow bxl_urinIfAny(uint32_t size, char*); ///< return the previously assimilated arrow corresponding to a piece of URI, NIL if wrong URI, EVE if arrow not assimilated.
BArrow bxl_digestIfAny(char*); ///< return a stored arrow corresponding to a digest, NIL if no match.

/* Unbuilding */
typedef enum e_bxlType {
    BXL_UNDEF=-1,
    BXL_EVE=0,
    BXL_PAIR=1,
    BXL_ATOM=2
} BXLType;

BXLType bxl_typeOf(BArrow); ///< return arrow type. // TODO: could it be a SMALL arrow ?
BArrow bxl_headOf(BArrow); ///< return arrow head.
BArrow bxl_tailOf(BArrow); ///< return arrow tail.
char* bxl_strOf(BArrow);  ///< retrieve an atomic arrow as a C string. @return pointer to freed.
char* bxl_memOf(BArrow, uint32_t* size_p); /// retrieve an atomic arrow as a piece of binary data. @return pointer to freed.
char* bxl_uriOf(BArrow, uint32_t* size_p); ///< Retrieve an arrow definition in URI notation. @return pointer to freed.
uint32_t bxl_hashOf(BArrow); ///< return the arrow hash.
char* bxl_digestOf(BArrow, uint32_t* size_p); ///< Retrieve an arrow digest. @return pointer to freed.
void* bxl_pointerOf(BArrow); //< Retrieve the C pointer corresponding to a "hook" arrow. @return pointer.

/* Rooting */
BArrow bxl_root(BArrow); ///< root an arrow.
BArrow bxl_unroot(BArrow); ///< unroot an arrow.

/* Transaction */
void bxl_begin();  ///< increment the global transaction counter. Other transactions will be synced with the one of this calling thread (or bxl_over)
void bxl_yield(BArrow); ///< perform GC, only preserving one "state" arrow. wait for all threads being ready.
void bxl_over();   ///< decrement the global transaction counter. For example, before thread termination. Any previously assimilated arrow should be assimilated again.
void bxl_commit(); ///< commit. wait for all transactions being over.
                  ///< Previously assimilated arrow must be assimilated again as they may have been forgotten if loose.

/* Testing */
int bxl_isEve(BArrow); ///< returns !0 if equals Eve.
int bxl_isRooted(BArrow); ///< returns !0 if rooted.
int bxl_equal(BArrow, BArrow); ///< returns !0 if arrows are equal.
BArrow bxl_isAtom(BArrow); ///< returns given arrow if an atom, else Eve.
BArrow bxl_isPair(BArrow); ///< returns given arrow if a pair, else Eve.

/* Browsing */
typedef void* BXLConnectivity; ///< enumerator type, as returned by bxl_childrenOf.
BArrow bxl_nextChild(BXLConnectivity); ///< iterate enumerator. Return child arrow or EVE when over.
int    bxl_rootFlag(XLConnectivity); ///< return the root flag for the connectivity info
int    bxl_stamp(XLConnectivity); ///< return a modification time stamp about connectivity
void   bxl_freeEnum(BXLConnectivity); ///< free enumerator.

BXLConnectivity bxl_childrenOf(BArrow); ///< return all known children of an arrow as an enumerator.
                                        ///< enumerator must be freed by bxl_freeEnum
BArrow  bxl_childOf(BArrow); ///< return only one child or Eve if there's none. If several children, result is stochastisc( TBC?).

typedef BArrow (*BXLCallBack)(BArrow arrow, BArrow context); ///< generic callback type for arrow fetching
void bxl_childrenOfCB(BArrow, BXLCallBack, BArrow context); ///< apply a given function to each children of an arrow

/* Assimilate binding */
BArrow bxl_operator(BXLCallBack hook, BArrow); ///< assimilate a C implemented operator.
BArrow bxl_continuation(BXLCallBack hook, BArrow); ///< assimilate a C implemented continuation.

/** run an Entrelacs Machine.
 * $M is a machine state.
 * $contextPath is the path representating a nested hierarchy of contexts (/C0.C1...Cn).
 */
BArrow bxl_run(BArrow contextPath, BArrow M, BArrow session); ///< M == (<program> (<environment> <continuation-stack>))

/** Eval a program by building and running a machine */
BArrow bxl_eval(BArrow contextPath, BArrow program, BArrow session);


#ifdef	__cplusplus
}
#endif

#endif // buffer.h
