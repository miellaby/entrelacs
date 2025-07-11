/** @file
 * Entrelacs system Arrow Space
 *
 * Everything but the Arrow type is XL prefixed to prevent name conflicts.
 */
 #pragma once

 #include <stdint.h>

/// @brief  Arrow Reference
typedef uint32_t Address;

/// @brief  Arrow Type
typedef enum e_xlType {
    XL_UNDEF=-1,
    XL_EVE=0,
    XL_PAIR=1,
    XL_ATOM=2
} XLType;

/// Generic callback for arrow fetching
typedef Address (*XLCallBack)(Address arrow, Address context); 

/// Enumerator type, as returned by xl_childrenOf.
typedef void* XLEnum; 

/// Failure return code 
#define XL_NIL (0xFFFFFFFFU)

/// Eve
#define XL_EVE (0)

/// Eve
extern const Address Eve; // Eve = XL_EVE

// Life Cycle
// ----------
int  xl_init(); ///< system initialization
void xl_destroy(); ///< release system resources and locks
 
// Assimilation
// ------------
Address xl_Eve(); ///< returns Eve.
Address xl_pair(Address tail, Address head); ///< assimilate a pair of arrows
Address xl_atom(char* str); ///< assimilate a C string
Address xl_atomn(uint32_t size, uint8_t* raw); ///< assimilate raw data
Address xl_uri(char* uri); ///< assimilate an URI. @return an arrow or NIL if bad URI
Address xl_urin(uint32_t size, char* uri_part); ///< assimilate a part of URI. @return an arrow or NIL if bad URI
Address xl_anonymous();   ///< assimilate a randomized value so to get an unique "anonymous" arrow.
                          ///< This is NOT the way to knowledge representation within Entrelacs.

// Hook FIXME à déplacer dans transient ou session
// ---------------------------
Address xl_hook(void* hook); ///< assimilate a C pointer and return an arrow.
                        ///< unbuild with xl_pointerOf.
                        ///< Bottom-rooted to distinguish from evil attack attempt.
                        ///< Unroot to neutralize (xp_pointerOf returning NULL).
                        ///< Doesn't survive to reboot
void* xl_pointerOf(Address); //< get the C pointer of a "hook" arrow. @return pointer.

// Arrow testing without assimilation
// -----------
Address xl_pairMaybe(Address tail, Address head); ///< return a pair of arrows if system-known, Eve otherwise.
Address xl_atomMaybe(char*); ///< return the already assimilated arrow corresponding to a C string, Eve otherwise.
Address xl_atomnMaybe(uint32_t size, uint8_t* raw); ///< return the already assimilated arrow corresponding to a raw piece of data, Eve otherwise.
Address xl_uriMaybe(char* uri); ///< return the previously assimilated arrow corresponding to an URI, NIL if wrong URI, EVE if arrow not assimilated.
Address xl_urinMaybe(uint32_t size, char* uri_part); ///< return the previously assimilated arrow corresponding to a part of URI, NIL if wrong URI, EVE if arrow not assimilated.
Address xl_digestMaybe(char* digest); ///< return a stored arrow corresponding to a digest, NIL if no match.

// Arrow deconstruction
// --------------------
XLType xl_typeOf(Address); ///< get arrow type. // TODO: could it be a SMALL arrow ?
Address xl_headOf(Address);  ///< get arrow head.
Address xl_tailOf(Address);  ///< get arrow tail.
char* xl_strOf(Address);   ///< get atomic arrow as a C string. Null terminator always added. @return pointer to freed.
uint8_t* xl_memOf(Address, uint32_t* size_p); /// get atomic arrow as binary data. No extra null terminator added. @return pointer to freed. 
char* xl_uriOf(Address, uint32_t* size_p); ///< get arrow definition in URI notation. @return pointer to freed.
uint32_t xl_hashOf(Address); ///< get arrow checksum.
char* xl_digestOf(Address, uint32_t* size_p); ///< get arrow digest. @return pointer to freed.
int   xl_read(Address a, XLType *type_p, uint32_t* hash_p, Address *tail_p, Address *head_p, uint8_t** raw_p, uint32_t *size_p); /// get infos about Arrow 

// Rooting
// -------
Address xl_root(Address); ///< root arrow.
Address xl_unroot(Address); ///< unroot arrow.

// Transaction FIXME à déplacer dans session.h
// -----------
void xl_begin();  ///< increment the global transaction counter. Other transactions will be synced with the one of this calling thread (or xl_over)
void xl_yield(Address); ///< perform GC, only preserving one "state" arrow. wait for all threads being ready.
void xl_over();   ///< decrement the global transaction counter. For example, before thread termination. Any previously assimilated arrow should be assimilated again.
void xl_commit(); ///< commit. wait for all transactions being over.
                  ///< Previously assimilated arrow must be assimilated again as they are forgotten if loose.

// Testing
// -------
int xl_isEve(Address); ///< returns !0 if equals Eve.
int xl_isRooted(Address); ///< returns !0 if rooted.
int xl_equal(Address, Address); ///< returns !0 if arrows are equal.
Address xl_isAtom(Address); ///< returns given arrow if an atom, else Eve.
Address xl_isPair(Address); ///< returns given arrow if a pair, else Eve.

// Browsing
// --------
XLEnum xl_childrenOf(Address); ///< return children of an arrow as an enumerator.
                             ///< enumerator must be freed by xl_enumFree
int    xl_enumNext(XLEnum); ///< iterate enumerator. Return 0 if over or broken. !0 otherwise.
Address  xl_enumGet(XLEnum); ///< get current arrow from enumerator.
void   xl_enumFree(XLEnum); ///< free enumerator.
void   xl_childrenOfCB(Address, XLCallBack, Address context); ///< apply a given function to each children of an arrow
