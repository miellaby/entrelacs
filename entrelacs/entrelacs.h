#ifndef _ENTRELACS_H
#define _ENTRELACS_H
/** @file
 * Entrelacs system public API.
 *
 * Everything but the Arrow type is XL prefixed to prevent name conflicts.
 * Consider including entrelacsm.h to get handy macros.
 *
 * TODO: remove tag/blob distinction. Blob storage is triggered by data size and is transparent.
 * TODO: Add a way to get the universal signature of an arrow.
 *   BLOB: sha1
 *   anything else: [ open address ; definition digest ]
 * -->  TODO preamble: open address must be now context-free.
 * ------>  TODO preamble to the preamble: ArrowId to contain the whole open address.
 */


#include <stdint.h>

/** Eve. */
#define EVE (0)
#define NIL (0xFFFFFF)

/** Arrow ID. */
typedef uint32_t Arrow; ///< ArrowId = SpatialRef (may be enforced by a transaction ID in a near future)

extern const Arrow Eve; // equals EVE

/** initialize the system. */
void xl_init();

/* Assimilation */
Arrow xl_Eve(); ///< returns Eve.
Arrow xl_pair(Arrow, Arrow); ///< assimilate a pair of arrows.
Arrow xl_atom(char*); ///< assimilate a C string
Arrow xl_natom(uint32_t size, char*); ///< assimilate raw data
Arrow xl_uri(char*); ///< assimilate an URI, returning an arrow (NIL if URI is wrong)

/* Probe arrows without creating them if missing
 */
Arrow xl_pairMaybe(Arrow, Arrow); ///< return a pair of arrows if system-known, Eve otherwise.
Arrow xl_atomMaybe(char*); ///< return the already assimilated arrow corresponding to a C string, Eve otherwise.
Arrow xl_natomMaybe(uint32_t size, char*); ///< return the already assimilated arrow corresponding to a raw piece of data, Eve otherwise.
Arrow xl_uriMaybe(char*); ///< return the previously assimilated arrow corresponding to an URI, NIL if wrong URI, EVE if arrow not assimilated.
Arrow xl_digestMaybe(char*); ///< return a stored arrow corresponding to a digest, NIL if no match.

/* Unbuilding */
typedef enum e_xlType {
    XL_UNDEF=-1,
    XL_EVE=0,
    XL_PAIR=1,
    XL_ATOM=2,
    XL_TUPLE=3 // Not implemented
} XLType;

XLType xl_typeOf(Arrow); ///< return arrow type. // TODO: could it be a SMALL arrow ?
Arrow xl_headOf(Arrow); ///< return arrow head.
Arrow xl_tailOf(Arrow); ///< return arrow tail.
char* xl_strOf(Arrow);  ///< retrieve an atomic arrow as a C string. @return pointer to freed.
char* xl_memOf(Arrow, uint32_t*); /// retrieve an atomic arrow as a piece of binary data. @return pointer to freed.
char* xl_uriOf(Arrow, uint32_t*); ///< Retrieve an arrow definition in URI notation. @return pointer to freed.
uint64_t xl_checksumOf(Arrow); ///< return an arrow checksum.
char* xl_digestOf(Arrow, uint32_t*); ///< Retrieve an arrow digest. @return pointer to freed.

/* Rooting */
Arrow xl_root(Arrow); ///< root an arrow.
Arrow xl_unroot(Arrow); ///< unroot an arrow.

/* Transaction */
void xl_commit(); /// commit. Loose arrows are recycled here. Don't use them anymore.

/* Testing */
int xl_isEve(Arrow); ///< returns !0 if equals Eve.
int xl_isRooted(Arrow); ///< returns !0 if rooted.
int xl_equal(Arrow, Arrow); /// returns !0 if arrows are equal.
Arrow xl_isAtom(Arrow); /// returns given arrow if an atom, else Eve.
Arrow xl_isPair(Arrow); /// returns given arrow if a pair, else Eve.

/* Browsing */
#define EOE ((XLEnum)0xFFFFFFF)
typedef void* XLEnum; ///< enumerator type, as returned by xl_childrenOf.
int    xl_enumNext(XLEnum); /// iterate enumerator. Return 0 if exhausted. !0 otherwise.
Arrow  xl_enumGet(XLEnum); /// get current arrow from enumerator.
void   xl_freeEnum(XLEnum); /// free enumerator.

XLEnum xl_childrenOf(Arrow); ///< return all known children of an arrow as an enumerator.
                             ///< enumerator must be freed by xl_freeEnum
Arrow  xl_childOf(Arrow); ///< return only one child or Eve if there's none. If several children, result is stochastisc( TBC?).

typedef Arrow (*XLCallBack)(Arrow arrow, Arrow context); ///< generic callback type for arrow fetching
void xl_childrenOfCB(Arrow, XLCallBack, Arrow context); ///< apply a given function to each children of an arrow

/* C hook assimilation */
Arrow xl_operator(XLCallBack hook, Arrow); ///< assimilate a C implemented operator.
Arrow xl_continuation(XLCallBack hook, Arrow); ///< assimilate a C implemented continuation.

/** run an Entrelacs Machine.
 * $M is a machine state.
 * $contextPath is the path representating a nested hierarchy of contexts (/C0.C1...Cn).
 */
Arrow xl_run(Arrow contextPath, Arrow M); ///< M == (<program> (<environment> <continuation-stack>))

/** Eval a program by building and running a machine */
Arrow xl_eval(Arrow contextPath, Arrow program);

#endif // entrelacs.h
