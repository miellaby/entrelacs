#pragma once
#include <stdint.h>
#include "space/cell.h"
#include "entrelacs/entrelacs.h"

#define DIGEST_HASH_SIZE 8
#define DIGEST_SIZE (2 + CRYPTO_SIZE + DIGEST_HASH_SIZE)
#define NAN (uint32_t)(-1)

/// @brief compute a $-digest of an arrow
/// @param a  address
/// @param[in] cellp cell@address
/// @param[out] l digestLength
/// @return digest
char* serial_digest(Address a, Cell* cellp, uint32_t *l);

char* serial_toURI(Address a, uint32_t *l);

Arrow serial_parseUri(uint32_t size, char* uri, uint32_t* uriLength_p, int ifExist);

Arrow serial_parseURIs(uint32_t size, char *uri, int ifExist);
