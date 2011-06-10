#ifndef _ENTRELACS_H
#define _ENTRELACS_H
#include <stdint.h>

/* ________________________________________
 * The Entrelacs C API*
 * ________________________________________
 * Note: Everything but the Arrow type is XL prefixed to prevent name conflicts with existing C libraries. Consider define macros in your project to unprefix things when possible.
 */

/* Arrow */
typedef uint32_t Arrow; // Arrow = <---TransactionId---><---MemoryRef--->


/** initialize the Entrelacs system */
void xl_init();

/* Assimilation */
Arrow xl_Eve();
Arrow xl_arrow(Arrow, Arrow);
Arrow xl_tag(char*);
Arrow xl_blob(uint32_t size, char*);

/* Retrieving arrows if already handled by the system only
 * to query the system without storing probing arrows
 */
Arrow xl_arrowMaybe(Arrow, Arrow);
Arrow xl_tagMaybe(char*);
Arrow xl_blobMaybe(uint32_t size, char*);

/* Unbuilding */
enum e_xlType { XL_UNDEF=-1, XL_EVE=0, XL_ARROW=1, XL_TAG=2, XL_BLOB=3, XL_SMALL=4, XL_TUPLE=5 } xl_typeOf(Arrow);
Arrow xl_headOf(Arrow);
Arrow xl_tailOf(Arrow);
char* xl_tagOf(Arrow); // to freed
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

typedef int (*XLCallBack)(Arrow arrow, char* context);
void xl_childrenOf(Arrow, XLCallBack, char*);

/* Program assimilation */
Arrow xl_program(char*); // EL string

/* C hook assimilation */
Arrow xl_operator(XLCallBack hook, char*); // operator arrow with hook
Arrow xl_continuation(XLCallBack hook, char*); // continuation arrow with hook

/* Build an Entrelacs Machine */
Arrow xl_machine(Arrow program, Arrow cc, Arrow current, Arrow stack);

/* Run an Entrelacs Machine */
int   xl_run(Arrow M);

/* Eval, ie. the program, continuation, machine and run sequence */
int   xl_eval(char* programString, XLCallBack cb);

#endif // entrelacs.h
