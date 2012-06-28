#ifndef _ENTRELACS_H
#define _ENTRELACS_H
/** @file
 * Entrelacs system public API.
 *
 * Everything but the Arrow type is XL prefixed to prevent name conflicts.
 * Consider including entrelacsm.h to get handy macros.
 */


#include <stdint.h>

/** Eve. */
#define EVE (0)
#define NIL (0xFFFFFF)

/** Arrow ID. */
typedef uint32_t Arrow; ///< ArrowId = SpatialRef (may be enforced by a transaction ID in a near future)

extern const Arrow Eve; // equals EVE

/** initialize the Entrelacs system. */
void xl_init();

/* Assimilation */
Arrow xl_Eve(); ///< returns Eve.
Arrow xl_arrow(Arrow, Arrow); ///< assimilate a pair of arrows, returning the corresponding arrow.
Arrow xl_tag(char*); ///< assimilate a C string, returning the corresponding tag arrow.
Arrow xl_btag(uint32_t size, char*); ///< assimilate a piece of data, returning the corresponding tag arrow.
Arrow xl_blob(uint32_t size, char*); ///< assimilate a piece of data, returning a blob arrow.
Arrow xl_uri(char*); ///< assimilate an URI, returning an arrow (NIL if URI is wrong)

/* Probe arrows without creating them if not known by the system.
 */
Arrow xl_arrowMaybe(Arrow, Arrow); ///< return the arrow corresponding to a previously assimilated pair of arrows, Eve otherwise.
Arrow xl_tagMaybe(char*); ///< return the arrow corresponding to a previously assimilated C string, Eve otherwise.
Arrow xl_btagMaybe(uint32_t size, char*); ///< return the arrow corresponding to a previously assimilated piece of data, Eve otherwise.
Arrow xl_blobMaybe(uint32_t size, char*); ///< return the arrow corresponding to a previously assimilated piece of data, Eve otherwise.
Arrow xl_uriMaybe(char*); ///< return the previously assimilated arrow corresponding to an URI, NIL if wrong URI, EVE if arrow not assimilated.

/* Unbuilding */
typedef enum e_xlType {
    XL_UNDEF=-1,
    XL_EVE=0,
    XL_ARROW=1,
    XL_TUPLE=2,
    XL_SMALL=3,
    XL_TAG=4,
    XL_BLOB=5
} XLType;

XLType xl_typeOf(Arrow); ///< return arrow type. // TODO: could it be a SMALL arrow ?
Arrow xl_headOf(Arrow); ///< return arrow head.
Arrow xl_tailOf(Arrow); ///< return arrow tail.
char* xl_tagOf(Arrow);  ///< retrieve a tag arrow as a C string. @return pointer to freed.
char* xl_btagOf(Arrow, uint32_t*); /// retrieve a tag arrow as a piece of binary data. @return pointer to freed.
char* xl_blobOf(Arrow, uint32_t*); /// retrieve a blob arrow as a piece of binary data. @return pointer to freed.
char* xl_uriOf(Arrow); ///< Retrieve an arrow definition in URI notation. @return pointer to freed.



/* Rooting */
Arrow xl_root(Arrow); ///< root an arrow.
Arrow xl_unroot(Arrow); ///< unroot an arrow.

/* Transaction */
void xl_commit(); /// commit. Loose arrows are recycled here. Don't use them anymore.

/* Testing */
int xl_isEve(Arrow); ///< returns !0 if equals Eve.
int xl_isRooted(Arrow); ///< returns !0 if rooted.
int xl_equal(Arrow, Arrow); /// returns !0 if arrows are equal.

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
 * $M is a machine state. $contextPath is a work context path (/C0.C1...Cn).
 * If the context path is not Eve, $M is "virtualized" (not yet implemented).
 */
Arrow xl_run(Arrow contextPath, Arrow M); ///< M == (<program> (<environment> <continuation-stack>))

/** Eval a program by building and run a machine */
Arrow xl_eval(Arrow contextPath, Arrow program);

#endif // entrelacs.h
