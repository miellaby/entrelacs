/** @file
 * Entrelacs system Arrow Space
 *
 * Everything but the Arrow type is XL prefixed to prevent name conflicts.
 */



 #ifndef _ARROW_SPACE_H
 #define _ARROW_SPACE_H
 #ifdef	__cplusplus
 extern "C" {
 #endif

#include <stdint.h>

/// @brief  Arrow Reference
typedef uint32_t Arrow;

/// @brief  Arrow Type
typedef enum e_xlType {
    XL_UNDEF=-1,
    XL_EVE=0,
    XL_PAIR=1,
    XL_ATOM=2
} XLType;

/// Generic callback for arrow fetching
typedef Arrow (*XLCallBack)(Arrow arrow, Arrow context); 

/// Enumerator type, as returned by xl_childrenOf.
typedef void* XLEnum; 

/// Failure return code 
#define XL_NIL (0xFFFFFFFFU)

/// Eve
#define XL_EVE (0)

/// Eve
extern const Arrow Eve; // Eve = XL_EVE

// Life Cycle
// ----------
int  xl_init(); ///< system initialization
void xl_destroy(); ///< release system resources and locks
 
// Assimilation
// ------------
Arrow xl_Eve(); ///< returns Eve.
Arrow xl_pair(Arrow tail, Arrow head); ///< assimilate a pair of arrows
Arrow xl_atom(char* str); ///< assimilate a C string
Arrow xl_atomn(uint32_t size, uint8_t* raw); ///< assimilate raw data
Arrow xl_uri(char* uri); ///< assimilate an URI. @return an arrow or NIL if bad URI
Arrow xl_urin(uint32_t size, char* uri_part); ///< assimilate a part of URI. @return an arrow or NIL if bad URI

// Compound arrow assimilation
// ---------------------------
Arrow xl_anonymous();   ///< assimilate a randomized value so to get an unique "anonymous" arrow.
                        ///< This is NOT the way to knowledge representation within Entrelacs.
Arrow xl_hook(void* hook); ///< assimilate a C pointer and return an arrow.
                        ///< unbuild with xl_pointerOf.
                        ///< Bottom-rooted to distinguish from evil attack attempt.
                        ///< Unroot to neutralize (xp_pointerOf returning NULL).
                        ///< Doesn't survive to reboot

// Arrow testing without assimilation
// -----------
Arrow xl_pairMaybe(Arrow tail, Arrow head); ///< return a pair of arrows if system-known, Eve otherwise.
Arrow xl_atomMaybe(char*); ///< return the already assimilated arrow corresponding to a C string, Eve otherwise.
Arrow xl_atomnMaybe(uint32_t size, uint8_t* raw); ///< return the already assimilated arrow corresponding to a raw piece of data, Eve otherwise.
Arrow xl_uriMaybe(char* uri); ///< return the previously assimilated arrow corresponding to an URI, NIL if wrong URI, EVE if arrow not assimilated.
Arrow xl_urinMaybe(uint32_t size, char* uri_part); ///< return the previously assimilated arrow corresponding to a part of URI, NIL if wrong URI, EVE if arrow not assimilated.
Arrow xl_digestMaybe(char* digest); ///< return a stored arrow corresponding to a digest, NIL if no match.

// Arrow deconstruction
// --------------------
XLType xl_typeOf(Arrow); ///< get arrow type. // TODO: could it be a SMALL arrow ?
Arrow xl_headOf(Arrow);  ///< get arrow head.
Arrow xl_tailOf(Arrow);  ///< get arrow tail.
char* xl_strOf(Arrow);   ///< get atomic arrow as a C string. Null terminator always added. @return pointer to freed.
uint8_t* xl_memOf(Arrow, uint32_t* size_p); /// get atomic arrow as binary data. No extra null terminator added. @return pointer to freed. 
char* xl_uriOf(Arrow, uint32_t* size_p); ///< get arrow definition in URI notation. @return pointer to freed.
uint32_t xl_checksumOf(Arrow); ///< get arrow checksum.
char* xl_digestOf(Arrow, uint32_t* size_p); ///< get arrow digest. @return pointer to freed.
void* xl_pointerOf(Arrow); //< get the C pointer of a "hook" arrow. @return pointer.

// Rooting
// -------
Arrow xl_root(Arrow); ///< root arrow.
Arrow xl_unroot(Arrow); ///< unroot arrow.

// Transaction
// -----------
void xl_begin();  ///< increment the global transaction counter. Other transactions will be synced with the one of this calling thread (or xl_over)
void xl_yield(Arrow); ///< perform GC, only preserving one "state" arrow. wait for all threads being ready.
void xl_over();   ///< decrement the global transaction counter. For example, before thread termination. Any previously assimilated arrow should be assimilated again.
void xl_commit(); ///< commit. wait for all transactions being over.
                  ///< Previously assimilated arrow must be assimilated again as they are forgotten if loose.

// Testing
// -------
int xl_isEve(Arrow); ///< returns !0 if equals Eve.
int xl_isRooted(Arrow); ///< returns !0 if rooted.
int xl_equal(Arrow, Arrow); ///< returns !0 if arrows are equal.
Arrow xl_isAtom(Arrow); ///< returns given arrow if an atom, else Eve.
Arrow xl_isPair(Arrow); ///< returns given arrow if a pair, else Eve.

// Browsing
// --------
XLEnum xl_childrenOf(Arrow); ///< return children of an arrow as an enumerator.
                             ///< enumerator must be freed by xl_enumFree
int    xl_enumNext(XLEnum); ///< iterate enumerator. Return 0 if over or broken. !0 otherwise.
Arrow  xl_enumGet(XLEnum); ///< get current arrow from enumerator.
void   xl_enumFree(XLEnum); ///< free enumerator.
void   xl_childrenOfCB(Arrow, XLCallBack, Arrow context); ///< apply a given function to each children of an arrow

#ifdef	__cplusplus
}
#endif
#endif // arrow_space.h

 