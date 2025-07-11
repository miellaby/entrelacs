#pragma once
#include <stdint.h>

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

/// EVE
Arrow xs_eve();

/// retrieve arrow by its splace location (might be NULL if invalid)
Arrow xs_arrow(Address id);

/// pair
Arrow xs_pair(Arrow tail, Arrow head);

/// @brief atom from buffer
/// @param size 
/// @param s 
/// @return 
Arrow xs_atomn(size_t size, uint8_t* s);

/// @brief atom from string
/// @param s 
/// @return arrow
Arrow xs_atom(char* s);

/// @brief parse a canonical URI of an arrow
/// @param size 
/// @param uri 
/// @param uri_size_p updated with actual parsed URI length 
/// @return  arrow
Arrow xs_parseURI(uint32_t size, char *uri, uint32_t *uri_size_p);

/// @brief parse a canonical URI of an arrow
/// @param uri 
/// @return arrow
Arrow xs_fromURI(char* uri);

/// @brief get arrow type
/// @param a arrow
/// @return type
ArrowType xs_getType(Arrow a);

/// @brief get arrow hash
/// @param a arrow
/// @return hashcode
uint32_t xs_getHash(Arrow a);

/// @brief get arrow tail
/// @param a arrow
/// @return tail
Arrow xs_getTail(Arrow a);

/// @brief get arrow head
/// @param a arrow
/// @return head
Arrow xs_getHead(Arrow a);

/// @brief get arrow location in arrow space
/// @details call xs_resolve first then id is setted if the arrow is assimilated
/// @param a arrow
/// @return address
Address xs_getId(Arrow a);

/// @brief get atom as string
/// @param a 
/// @return string
char *xs_getStr(Arrow a);

/// @brief get atom raw buffer
/// @param a atom
/// @param size updated with buffer size
/// @return  buffer
char *xs_getMem(Arrow a, size_t *size);

/// @brief get canonical URI representation of the arrow
/// @param a arrow
/// @return URI (heap allocated)
char* xs_getUri(Arrow a);

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
int xs_isEve(Arrow* a);

/// @brief is atom?
/// @param a 
/// @return true if atom
int xs_isAtom(Arrow* a);

/// @brief is pair?
/// @param a 
/// @return true if pair
int xs_isPair(Arrow* a);

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
Arrow xs_equal(Arrow a, Arrow b);

/// @brief root arrow
/// @param a arrow
/// @return the arrow
Arrow xs_root(Arrow a);

/// @brief unroot arrow
/// @param a arrow
/// @return the arrow
Arrow xs_unroot(Arrow a);