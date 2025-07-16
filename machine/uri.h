#pragma once
#include "transient.h"
#include "space/hash.h"
#include <stdint.h>

#define NAN (uint32_t)(-1)
#define DIGEST_HASH_SIZE 8
#define DIGEST_SIZE (2 + CRYPTO_SIZE + DIGEST_HASH_SIZE)

/// @brief get the URI path corresponding to an arrow
/// @param a arrow
/// @param l_p updated with URI size when non-NULL
/// @return
char* xs_getURI(Arrow a, uint32_t *l_p);

/// @brief parse URI within provided buffer and get a transient arrow
/// @param size input buffer size
/// @param uri input buffer
/// @param uri_size_p updated with actual read size when non-NULL
/// @return arrow
Arrow xs_parseURI(uint32_t size, char *uri, uint32_t *uri_size_p);

/// @brief parse white-char separated URIs within provided buffer and get a compound transient arrow
/// @param size input buffer size
/// @param uri input buffer
/// @param uri_size_p updated with actual read size when non-NULL
/// @return arrow-based list of URI
Arrow xs_parseURIs(uint32_t size, char *uri, uint32_t *uri_size_p);
