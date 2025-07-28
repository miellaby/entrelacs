#pragma once
#include "entrelacs/entrelacs.h"
#include "space/cell.h"

/** assimilate tail->head pair
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_pair(Address tail, Address head, int ifExist);

/** retrieve arrow by digest */
Address probe_digest(char *digest);

/** assimilate the arrow corresponding to binary string $str with size $length and precomputed $hash.
 * Will create the singleton if missing except if $ifExist is set.
 * $str might be a blob signature or a tag content.
 */
Address assimilate_string(int cellType, const int length, const uint8_t *payload, int ifExist);

/** assimilate a tag binary string defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_tag(const uint32_t size, const uint8_t* data, int ifExist);


/** retrieve a blob binary string defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_blob(const uint32_t size, const uint8_t* data, int ifExist);

/** assimilate small binary string
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_small(const int length, const uint8_t* str, int ifExist);
