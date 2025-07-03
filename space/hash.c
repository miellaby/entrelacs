#include "space/hash.h"
#include "log/log.h"
#include "sha1/sha1.h"
#include <stdio.h>

uint32_t left_rotate(uint32_t value, int shift) {
    return (value << shift) | (value >> (32 - shift));
}

uint32_t right_rotate(uint32_t value, int shift) {
    return (value >> shift) | (value << (32 - shift));
}

/* hash a regular arrow (based on its both ends hash codes) */
uint64_t hash_pair(uint64_t h_tail, uint64_t h_head) {
    return left_rotate(h_tail, 19) ^ right_rotate(h_head, 5);
}

/* hash a null-terminated string such as a tag atom content. Also return its size */
uint64_t hash_string(uint8_t *str, uint32_t *length) {  // simple string hash
    uint64_t hash = 5381;
    uint8_t *s = str;
    uint8_t c;
    uint32_t l = 0;
    while ((c = *s++)) {  // for all bytes up to the null terminator
        // 5 bits shift only because ASCII varies within 5 bits
        hash = ((hash << 5) + hash) + c;
        l++;
    }
    //  // The null-terminator is also counted and hashed for consistancy with hash_raw
    //  hash = ((hash << 5) + hash) + c;
    //  l++;

    TRACEPRINTF("hash_string(\"%s\") = [ %016llx ; %d] ", str, hash, l);

    *length = l;
    return hash;
}

/* hash function to get H1 from a binary tag arrow definition */
uint64_t hash_raw(uint8_t *buffer, uint32_t length) {  // simple string hash
    uint64_t hash = 5381;
    int c;
    uint8_t *p = buffer;
    uint32_t l = length;
    while (l--) {  // for all bytes (even last one)
        c = *p++;
        // 5 bits shift only because ASCII generally works within 5 bits
        hash = ((hash << 5) + hash) + c;
    }
    hash = (hash ^ (hash >> 32) ^ (hash << 32));
    TRACEPRINTF("hash_raw(%d, \"%.*s\") = %016llx", length, length, buffer, hash);

    return hash;
}

/* hash function to get hashChain from a tag or blob containing cell */
uint32_t hash_chain(Cell *cell) {
    // This hash mixes the cell content
    uint32_t inverted = cell->arrow.hash >> 16 & cell->arrow.hash << 16;
    if (cell->full.type == CELLTYPE_BLOB || cell->full.type == CELLTYPE_TAG)
        inverted = inverted ^ cell->uint.data[1] ^ cell->uint.data[2];
    return inverted;
}

/* hash function to get hash_children from a cell caracteristics */
uint32_t hash_children(Cell *cell) {
    return hash_chain(cell) ^ 0xFFFFFFFFu;
}

/* ________________________________________
  * The blob cryptographic hash computing fonction.
  * This function returns a string used to define a tag arrow.
  *
  */

char *hash_crypto(uint32_t size, uint8_t *data, char output[CRYPTO_SIZE + 1]) {
    uint8_t h[20];
    sha1(data, size, h);
    sprintf(output, "%08x%08x%08x%08x%08x", *(uint32_t *)h, *(uint32_t *)(h + 4), *(uint32_t *)(h + 8), *(uint32_t *)(h + 12), *(uint32_t *)(h + 16));
    return output;
}
