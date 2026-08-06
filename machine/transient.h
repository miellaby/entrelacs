#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * Transient Arrows
 *
 * définition et manipulation de flèches éphémères assimilées paresseusement
 * idéal pour applicatif créant beaucoup de flèches intermédiaires:
 * API xl_... : flèches projetées dans le Arrow Space (ref = adresse)
 * API xs_... : flèches de travail temporaires (ref = struct *)
 * API d'après https://github.com/miellaby/entrelacs/blob/wiki/ArrowSpaceInterface.md
*/
// #include "space/space.h"
typedef uint32_t Address;

/// @brief  Transient Arrow
typedef struct xs_arrow_s* Arrow;

typedef enum xs_type {
    XS_UNDEF = -1,
    XS_EVE = 0,
    XS_ATOM = 1,
    XS_PAIR = 2
} ArrowType;

/// Generic destructor callback for native hook pointers
typedef void (*XSDestructor)(void*);

/// Generic callback for client
typedef Arrow (*XSCallBack)(Arrow arrow, Arrow context);

/// system initialization
int  xs_init();

/// reset transient arrows
void xs_pool_reset();

/// EVE
Arrow xs_eve();

/// retrieve arrow by its splace location (might be NULL if invalid)
Arrow xs_arrow(Address id);

/// pair
Arrow xs_pair(Arrow tail, Arrow head);

/// @brief atom from buffer
/// @param size buffer size
/// @param buffer buffer
/// @return arrow
Arrow xs_atomn(size_t size, uint8_t* buffer);

/// @brief atom from string
/// @param s string
/// @return arrow
Arrow xs_atom(char* s);

/// @brief atom from borrowed const buffer
/// @param s string
/// @return arrow
Arrow xs_constn(size_t size, const uint8_t* buffer);

/// @brief atom from borrowed const string
/// @param s string
/// @return arrow
Arrow xs_const(const char* s);

/// @brief arrow from URI
/// @param null-terminated URI string
/// @return arrow
Arrow xs_uri(char *aUri);

/// @brief arrow from URI
/// @param aSize buffer size
/// @param uri buffer
/// @return arrow
Arrow xs_urin(uint32_t aSize, char *aUri);

/// @brief get an arrow by its digest
/// @param digest
/// @return the arrow or NULL if wrong digest
Arrow xs_digest(char *digest);

/// @brief get arrow type
/// @param a arrow
/// @return type
ArrowType xs_getType(Arrow a);

/// @brief get arrow hash
/// @param a arrow
/// @return hashcode
uint32_t xs_getHash(Arrow a);

/// @brief get arrow diget
/// @param a arrow
/// @return heap-allocated digest
char* xs_getDigest(Arrow a, size_t *l);

/// @brief get arrow tail
/// @param a arrow
/// @return tail
Arrow xs_getTail(Arrow a);

/// @brief get arrow head
/// @param a arrow
/// @return head
Arrow xs_getHead(Arrow a);

/// @brief get first atom in a chain of pairs
/// @param a arrow
/// @return first atom
Arrow xs_getFirstAtom(Arrow a);

/// @brief get arrow location in arrow space
/// @details call xs_resolve first then id is setted if the arrow is assimilated
/// @param a arrow
/// @return address
Address xs_getId(Arrow a);

/// @brief get canonical URI representation of the arrow
/// @param a arrow
/// @return URI (heap allocated)
char* xs_getUri(Arrow a);

/// @brief get atom as string
/// @param a
/// @return heap-allocated string
char *xs_getStr(Arrow a);

/// @brief get atom raw buffer
/// @param a atom
/// @param size updated with buffer size
/// @return  heap-allocated buffer
uint8_t *xs_getMem(Arrow a, size_t *size);

/// @brief get atom raw buffer
/// @param a atom
/// @param size updated with buffer size
/// @return  buffer in atom definition
const uint8_t *xs_borrowMem(Arrow a, size_t *size);

/// @brief getStr into size-limited buffer. Max size-1 chars. Null-terminator added.
/// @param size buffer size
/// @param buffer output buffer
/// @param a arrow
/// @param offset skipped bytes from arrow str
/// @return read byte count
int xs_readStr(size_t size, uint8_t* buffer, Arrow a, size_t offset);

/// @brief getMem into size-limited buffer
/// @param size buffer size
/// @param buffer output buffer
/// @param a arrow
/// @param offset skipped bytes from arrow str
/// @return read byte count
ssize_t xs_readMem(size_t size, uint8_t* buffer, Arrow a, size_t offset);

/// @brief resolve the arrow location in the AS
/// @param a arrow
/// @return the arrow
Arrow xs_resolve(Arrow a);

/// @brief assimilate the arrow
/// @details if the arrow doesn't exist yet in the AS, it is added
/// @param a arrow
/// @return the arrow
Arrow xs_assimilate(Arrow a);

/// @brief is eve?
/// @param a
/// @return true if eve
int xs_isEve(Arrow a);

/// @brief is atom?
/// @param a
/// @return true if atom
int xs_isAtom(Arrow a);

/// @brief is pair?
/// @param a
/// @return true if pair
int xs_isPair(Arrow a);

/// @brief is known
/// @details tell if arrow has already been assimilated
/// @param a
/// @return true is arrow has an address
int xs_isKnown(Arrow a);

/// @brief is rooted?
/// @param a
/// @return true if a is rooted
int xs_isRooted(Arrow a);

/// @brief compare 2 arrow definitions
/// @param a
/// @param b
/// @return a if equals to b, eve otherwise
int xs_equal(Arrow a, Arrow b);

/// @brief root arrow
/// @param a arrow
/// @return the arrow
Arrow xs_root(Arrow a);

/// @brief unroot arrow
/// @param a arrow
/// @return the arrow
Arrow xs_unroot(Arrow a);

/// apply a given function to each children of an arrow
void xs_childrenOfCB(Arrow, XSCallBack, void* context);

/// hook badge
#define xs_hookBadge() xs_constn(7, (uint8_t *)"XShO0K")

/// hook a pointer
Arrow xs_hook(void* p);

/// hook a pointer with an optional destructor
Arrow xs_hook_destructor(void* p, XSDestructor destructor);

/// read hooked pointer into destination pointer
ssize_t xs_readPointer(Arrow hook, void** pp);

/// get hooked pointer
void* xs_getPointer(Arrow hook);
