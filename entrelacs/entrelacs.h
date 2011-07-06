#ifndef _ENTRELACS_H
#define _ENTRELACS_H
#include <stdint.h>

/* ________________________________________
 * The Entrelacs C API*
 * ________________________________________
 * Note: Everything but the Arrow type is XL prefixed to prevent name conflicts with existing C libraries.
 *       Consider define macros in your project to unprefix things when possible.
 */

/* Arrow */
typedef uint32_t Arrow; // Arrow = <---TransactionId---><---MemoryRef--->


/** initialize the Entrelacs system */
void xl_init();

/* Assimilation */
Arrow xl_Eve();
Arrow xl_arrow(Arrow, Arrow);
Arrow xl_tag(char*);
Arrow xl_btag(uint32_t size, char*);
Arrow xl_blob(uint32_t size, char*);

/* Retrieving arrows if already handled by the system only
 * to query the system without storing probing arrows
 */
Arrow xl_arrowMaybe(Arrow, Arrow);
Arrow xl_tagMaybe(char*);
Arrow xl_btagMaybe(uint32_t size, char*);
Arrow xl_blobMaybe(uint32_t size, char*);

/* Unbuilding */
enum e_xlType { XL_UNDEF=-1, XL_EVE=0, XL_ARROW=1, XL_TUPLE=2, XL_SMALL=3, XL_TAG=4, XL_BLOB=5 } xl_typeOf(Arrow);
Arrow xl_headOf(Arrow);
Arrow xl_tailOf(Arrow);
char* xl_tagOf(Arrow); // to freed
char* xl_btagOf(Arrow, uint32_t*);
char* xl_blobOf(Arrow, uint32_t*); // to freed

/* Rooting */
Arrow xl_root(Arrow);
void  xl_unroot(Arrow);

/* Transaction */
void xl_commit(); //  Loose arrows are recycled here. Don't use them anymore.

/* Testing */
int xl_isEve(Arrow); // returns !0 if equals Eve
int xl_isRooted(Arrow); // returns !0 if rooted.
int xl_equal(Arrow, Arrow); // returns !0 if arrows are equal

/* Browsing */
//typedef uint32_t XLEnum;
//XLEnum xl_childrenOf(Arrow);
//int  xl_next(XLEnum*, Arrow*);

typedef int (*XLCallBack)(Arrow arrow, void* context);
void xl_childrenOf(Arrow, XLCallBack, void*);

/* Program assimilation */
Arrow xl_program(char*); // EL string
char* xl_programOf(Arrow); // to be freed

/* C hook assimilation */
Arrow xl_operator(XLCallBack hook, char*); // operator arrow with hook
Arrow xl_continuation(XLCallBack hook, char*); // continuation arrow with hook

/* Run an Entrelacs Machine
 * a vStack is a virtualization stack: vStack == (En (.. (E3 (E2 (E1 Eve)))))
 * A machine handles information inside a construct of virtual spaces
 * v(k, a) = // where k a stack and a an arrow
 *   isEve(k) ? a :
 *       isEntrelacs(a) ? arrow(tail(k), v(head(k), a)) :
 *           arrow(v(k, head(a)), v(k, tail(a)))
 */
Arrow xl_run(Arrow rootStack, Arrow M); // M == (<program> (<environment> <continuation-stack>))

/* Eval by building and run a machine */
Arrow xl_eval(Arrow rootStack, Arrow program);

#endif // entrelacs.h
