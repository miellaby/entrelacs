#include "serial.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "log/log.h"
#include "space/cell.h"
#include "space/hash.h"
#include "mem/geoalloc.h"
#include "mem/mem.h"
#include "space/assimilate.h"

#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

/// URL-encode input buffer into destination buffer.
/// 0-terminate the destination buffer.
static void percent_encode(uint8_t *src, uint32_t src_len, char *dst, uint32_t* dst_len_p) {
    static const char *dont_escape = "_-,;~()";
    static const char *hex = "0123456789abcdef";
    uint32_t i, j;
    for (i = j = 0; i < src_len; i++, src++, dst++, j++) {
        if (*src && (isalnum(*src) ||
                strchr(dont_escape, *src) != NULL)) {
            *dst = *src;
        } else {
            dst[0] = '%';
            dst[1] = hex[(*src) >> 4];
            dst[2] = hex[(*src) & 0xf];
            dst += 2;
            j += 2;
        }
    }

    *dst = '\0';
    *dst_len_p = j;
}


/// URL-decode input buffer into destination buffer.
/// 0-terminate the destination buffer.
static void percent_decode(const char *src, uint32_t src_len, uint8_t *dst, uint32_t* dst_len_p) {
    uint32_t i, j;
    int a, b;

    for (i = j = 0; i < src_len; i++, j++) {
        if (src[i] == '%' &&
                isxdigit(a = src[i + 1]) &&
                isxdigit(b = src[i + 2])) {
            a = tolower(a);
            b = tolower(b);
            dst[j] = (char) ((HEXTOI(a) << 4) | HEXTOI(b));
            i += 2;
        } else {
            dst[j] = src[i];
        }
    }

    dst[j] = '\0'; // Null-terminate the destination
    *dst_len_p = j;
}

static uint32_t skeepSpacesAndOnePlus(uint32_t size, char* uriEnd) {
    char c;
    uint32_t l = 0;
    while ((size == NAN || l < size)
            && (c = uriEnd[l]) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
        // white spaces are tolerated and ignored here
        l++;
    }
    if (c == '+' && (size == NAN || l < size)) l++;
    return l;
}

/** get an URI path corresponding to an arrow */
char* serial_toURI(Address a, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (a == EVE) { // Eve is identified by an empty path
        
        // allocate and return an empty string
        char *s = (char*) malloc(1);
        if (!s) { // allocation failed
            return NULL;
        }

        s[0] = '\0';
        if (l) *l = 0; // if asked, return length = 0
        return s;
    }

    if (a >= SPACE_SIZE) {
        return NULL; // address anomaly
    }

    // Get cell pointed by a
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));

    switch (cell.full.type) {
        case CELLTYPE_EMPTY:
        {
            return NULL; // address anomaly
        }
        case CELLTYPE_BLOB:
        { // return a BLOB digest
          char *digest = serial_digest(a, &cell, l);
          return digest;
        }
        case CELLTYPE_SMALL:
        case CELLTYPE_TAG:
        { // return an encoded string with atom content

            // get atom content
            uint32_t memLength, encodedDataLength;
            uint8_t* mem = cell_getPayload(a, &cell, &memLength);
            char *uri = malloc(3 * memLength + 1); // memory allocation for encoded content
            if (!uri) { // allocation failed
               free(mem);
               return NULL;
            }

            // encode content
            percent_encode(mem, memLength, uri, &encodedDataLength);
            free(mem); // original content is not returned so freed 
            
            // adjust memory allocaiton to free few bytes
            uri = realloc(uri, 1 + encodedDataLength);
            
            if (l) *l = encodedDataLength; // return length if asked
            return uri;
        }
        case CELLTYPE_PAIR:
        { // concat tail and head identifiers into /tail+head style URI
            uint32_t l1, l2;
            char *tailUri = serial_toURI(xl_tailOf(a), &l1);
            if (tailUri == NULL) return NULL;

            char *headUri = serial_toURI(xl_headOf(a), &l2);
            if (headUri == NULL) {
                free(tailUri);
                return NULL;
            }

            // allocate to save result
            char *uri = malloc(2 + l1 + l2 + 1);
            if (!uri) {
                free(tailUri);
                free(headUri);
                return NULL;
            }

            // concat identiers
            sprintf(uri, "/%s+%s", tailUri, headUri); // TODO no printf
            
            // free both end identifiers
            free(tailUri);
            free(headUri);

            if (l) *l = 2 + l1 + l2; // return length if asked
            return uri;
        }
        default:
            assert(0);
    } // switch
}

char* serial_digest(Address a, Cell* cellp, uint32_t *l) {
    char    *digest;
    uint32_t hash;

    char    *hashStr = (char *) malloc(CRYPTO_SIZE + 1);
    if (!hashStr) { // alloc failed
      return NULL;
    }

    hash = cellp->arrow.hash;
    if (a == EVE) {
      hash_crypto(0, (uint8_t *)"", hashStr);
    } else {
      switch (cellp->full.type) {
        case CELLTYPE_PAIR:
        {
            uint32_t uriLength;
            char* uri = serial_toURI(a, &uriLength);
            if (uri == NULL) {
              return NULL;
            }
            hash_crypto(uriLength, (uint8_t *)uri, hashStr);
            free(uri);
            break;
        }
        case CELLTYPE_BLOB:
        {
            uint32_t hashLength;
            free(hashStr);
            hashStr = (char *)cell_getPayload(a, cellp, &hashLength);
            if (hashStr == NULL)
              return NULL;
            break;
        }
        case CELLTYPE_SMALL:
        case CELLTYPE_TAG:
        {
            uint32_t dataSize;
            uint8_t *data = cell_getPayload(a, cellp, &dataSize);
            if (data == NULL)
              return NULL;
            hash_crypto(dataSize, data, hashStr); // FIXME why not percent-encoded string?
            free(data);
            break;
        }
        default:
            assert(0);
      }
    }

    uint32_t digestLength;
    digest = (char *) malloc(2 + DIGEST_HASH_SIZE + CRYPTO_SIZE + 1);
    if (!digest) {
      return NULL;
    }

    digest[0] = '$';
    digest[1] = 'H';
    digestLength = 2;

    // Turn the hash into hexa. TODO: more efficient
    static const char *hex = "0123456789abcdef";
    char hashMap[DIGEST_HASH_SIZE] = {
        hex[(hash >> 28) & 0xF],
        hex[(hash >> 24) & 0xF],
        hex[(hash >> 20) & 0xF],
        hex[(hash >> 16) & 0xF],
        hex[(hash >> 12) & 0xF],
        hex[(hash >> 8)  & 0xF],
        hex[(hash >> 4)  & 0xF],
        hex[hash & 0xF]
    };
    
    // copy the hash hexa at the digest beginning
    strncpy(digest + digestLength, hashMap, DIGEST_HASH_SIZE);
    digestLength += DIGEST_HASH_SIZE;
    
    // then copy the crypto hash
    strncpy(digest + digestLength, hashStr, CRYPTO_SIZE);
    free(hashStr);
    digestLength += CRYPTO_SIZE;
    digest[digestLength] = '\0';

    // return result and its length if asked
    if (l) *l = digestLength;
    
    return digest;
}


Arrow serial_parseUri(uint32_t size, char* uri, uint32_t* uriLength_p, int ifExist) {
    TRACEPRINTF("BEGIN serial_parseUri(%s)", uri);
    Arrow a = NIL;
    uint32_t uriLength = NAN;

    char c = uri[0];
    
    if (c <= 32 || !size) { // Any control-caracters/white-spaces are considered as URI break
        a = EVE;
        uriLength = 0;
    } else switch (c) {
            case '+': // Eve
                a = EVE;
                uriLength = 0;
                break;
            case '$':
            {
                if (size != NAN && size < DIGEST_SIZE) {
                    TRACEPRINTF("serial_parseUri - not long enough for a digest: %d", size);
                    break;
                }
                
                if (uri[1] != 'H') { // Only digest are allowed in URI
                    break;
                }

                uriLength = 2;
                while ((size == NAN || uriLength < size) 
                        && (c = uri[uriLength]) > 32 && c != '+' && c != '/')
                    uriLength++;
                
                if (uriLength != DIGEST_SIZE) {
                    TRACEPRINTF("serial_parseUri - wrong digest size %d", DIGEST_SIZE);
                    break;
                }

                a = probe_digest(uri);
                
                if (a == NIL) // Non assimilated blob
                    uriLength = NAN;
                
                break;
            }
            case '/':
            { // Pair
                uint32_t tailUriLength, headUriLength;
                Arrow tail, head;
                
                if (size != NAN) size--;
                
                tail = serial_parseUri(size, uri + 1, &tailUriLength, ifExist);
                if (tailUriLength == NAN) { // Non assimilated tail
                    a = tail; // NIL or EVE
                    uriLength = NAN;
                    break;
                }
                
                char tailUriEnd = uri[1 + tailUriLength];
                if (tailUriEnd == '\0' || tailUriLength == size /* no more char */) {
                    a = tail;
                    uriLength = 1 + tailUriLength;
                    break;
                }

                char* headUriStart;
                if (tailUriEnd == '+') {
                    if (size != NAN)
                        size -= tailUriLength + 1;
                    headUriStart = uri + 1 + tailUriLength + 1;
                } else {
                    if (size != NAN)
                        size -= tailUriLength;
                    headUriStart = uri + 1 + tailUriLength;
                }
                
                head = serial_parseUri(size, headUriStart, &headUriLength, ifExist);
                if (headUriLength == NAN) { // Non assimilated head
                    a = head; // NIL or EVE
                    uriLength = NAN;
                    break;
                }

                a = assimilate_pair(tail, head, ifExist);
                if (a == EVE || a == NIL) { // Non assimilated pair
                    a = head; // NIL or EVE
                    uriLength = NAN;
                    break;
                }
                
                uriLength = 1 + tailUriLength + (tailUriEnd == '+' /* 1/0 */) + headUriLength;
                break;
            }
            default:
            { // ATOM
                uint32_t atomLength;

                // compute atom URI length 
                uriLength = 0;
                while ((size == NAN || uriLength < size)
                        && (c = uri[uriLength]) > 32 && c != '+' && c != '/')
                    uriLength++;
                assert(uriLength);

                uint8_t *atomStr = malloc(uriLength + 1);
                percent_decode(uri, uriLength, atomStr, &atomLength);
                if (atomLength < TAG_MINSIZE) {
                    a = assimilate_small(atomLength, atomStr, ifExist);
                } else if (atomLength < BLOB_MINSIZE) {
                    a = assimilate_string(CELLTYPE_TAG, atomLength, atomStr, ifExist);
                } else {
                    a = assimilate_blob(atomLength, atomStr, ifExist);
                }
                free(atomStr);
                
                if (a == NIL || a == EVE) { // Non assimilated
                    uriLength = NAN;
                }
            }
        }

    DEBUGPRINTF("END serial_parseUri(%s) = %O (length = %d)", uri, a, uriLength);
    *uriLength_p = uriLength;
    return a;
}

Arrow serial_parseURIs(uint32_t size, char *uri, int ifExist) { // TODO: document actual design
    char c;
    uint32_t uriLength, gap;
    Arrow a = serial_parseUri(size, uri, &uriLength, ifExist);
    if (uriLength == NAN)
        return a; // return NIL (wrong URI) or EVE (not assimilated)
    
    if (size != NAN) size -= uriLength;
    if (size == 0)
         return a;

    gap = skeepSpacesAndOnePlus(size, uri + uriLength);
    if (size != NAN)
       size = (gap > size ? 0 : size - gap);
    uriLength += gap;
    while ((size == NAN || size--) && (c = uri[uriLength])) {
        DEBUGPRINTF("nextUri = >%s<", uri + uriLength);
        uint32_t nextUriLength;
        Arrow b = serial_parseUri(size, uri + uriLength, &nextUriLength, ifExist);
        if (nextUriLength == NAN)
            return b; // return NIL (wrong URI) or EVE (not assimilated)
        uriLength += nextUriLength;
        if (size != NAN) size -= nextUriLength;
        gap = skeepSpacesAndOnePlus(size, uri + uriLength);
        if (size != NAN) size -= gap;
        uriLength += gap;
    
        a = assimilate_pair(a, b, ifExist);
        if (a == EVE) {
          return EVE; // not assimilated pair
        }
    }

    return a;
}

