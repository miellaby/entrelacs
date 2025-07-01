#pragma once
#include "entrelacs/entrelacs.h"
#include "space/cell.h"

/** assimilate tail->head pair
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_pair(Arrow tail, Arrow head, int ifExist);

/** retrieve arrow by digest */
Address probe_digest(char *digest);

/** assimilate the arrow corresponding to data at $str with size $length and precomputed $hash.
 * Will create the singleton if missing except if $ifExist is set.
 * $str might be a blob signature or a tag content.
 */
Arrow assimilate_string(int cellType, int length, char* str, uint64_t payloadHash, int ifExist);

/** assimilate a tag arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_tag(uint32_t size, char* data, int ifExist, uint64_t hash);


/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_blob(uint32_t size, char* data, int ifExist);

/** assimilate small
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_small(int length, char* str, int ifExist);
