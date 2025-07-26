#define LOG_CURRENT LOG_SPACE
#include "assimilate.h"
#include "serial.h"
#include "log/log.h"
#include "mem/mem.h"
#include "mem/geoalloc.h"
#include "space/hash.h"
#include "space/weaver.h"
#include "space/stats.h"
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>


#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

/// increment "pebble" counters in the probing path up to the new singleton address
static void add_pebbles(Address newAddress, uint32_t hash) {
    Address probeAddress, probeOffset;
    probeAddress = get_openAddress(hash); // base address
    probeOffset = get_probeOffset(hash); // probe offset
    while (probeAddress != newAddress) {
        Cell probed;
        CELL_READ(&probed, probeAddress);
        if (probed.full.pebble != PEEBLE_MAX) {
            probed.full.pebble++;
            CELL_WRITE(&probed, probeAddress);
        }
        ADDRESS_SHIFT(probeAddress, probeAddress, probeOffset);
    }
}

/** assimilate tail->head pair
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_pair(Address tail, Address head, int ifExist) {
    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress, firstFreeAddress;
    int safeguard;

    if (tail == XL_NIL || head == XL_NIL)
        return XL_NIL;

    space_stats.get++;

    // Compute hashs
    Cell tailCell, headCell;
    CELL_READ(&tailCell, tail);
    CELL_READ(&headCell, head);
    hash = hash_pair(tailCell.arrow.hash, headCell.arrow.hash);
    hashAddress = get_openAddress(hash); // base address
    hashProbe = get_probeOffset(hash); // probe offset

    // Probe for an existing singleton
    Cell probed;
    probeAddress = hashAddress;
    firstFreeAddress = XL_EVE;
    safeguard = PROBE_LIMIT;
    while (--safeguard) {
        CELL_READ(&probed, probeAddress);

        if (probed.full.type == CELLTYPE_EMPTY)
            firstFreeAddress = probeAddress;

        else if (probed.full.type == CELLTYPE_PAIR
                && probed.pair.tail == tail
                && probed.pair.head == head
                && probeAddress /* ADAM can't be put at EVE place! TODO: optimize */) {
            space_stats.found++;
            return probeAddress; // OK: arrow found!
        }
        // Not the singleton

        if (probed.full.pebble == 0) {
            break; // Probing over. It's a miss.
        }

        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }
    assert(safeguard);

    // Miss
    if (ifExist) // one only wants to test for singleton existence in the arrows space
        return XL_EVE; // Eve means not found

    space_stats.new++;
    space_stats.pair++;

    if (firstFreeAddress == XL_EVE) {
        // You don't want safeguard = PROBE_LIMIT; otherwise
        // one may create arrows out of the probing limit
        while (--safeguard) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);

            Cell probed;
            CELL_READ(&probed, probeAddress);

            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        if (!safeguard) {
          ERRORPRINTF("can't find a free cell offset0=%d, offset=%d, mod=%d", (int)hash % PRIM1, (int)hashProbe, (int)(hashProbe % SPACE_SIZE));
        }
        assert(safeguard);
    }

    // Create a singleton

    // read the free cell
    Address newArrow = firstFreeAddress;
    Cell newCell;
    CELL_READ(&newCell, newArrow);

    // pair
    newCell.full.type = CELLTYPE_PAIR;
    newCell.pair.tail = tail;
    newCell.pair.head = head;
    newCell.arrow.hash = hash;
    newCell.arrow.RC0dCn = 0;
    newCell.arrow.child0 = XL_EVE;
    newCell.arrow.dr = space_stats.new & 0xFFu;
    CELL_WRITE(&newCell, newArrow);

    // Add "pebbles" in the probing path up to the new singleton
    add_pebbles(newArrow, hash);

    // record new arrow as loose arrow
    weaver_addLoose(newArrow);
    return newArrow;
}

Address probe_digest(char *digest) {
    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress;

    // read the hash at the digest beginning
    assert(digest[0] == '$' && digest[1] == 'H');
    int i = 2; // jump over '$H' string
    hash = HEXTOI(digest[i]);
    while (++i < 10) { // read 8 hexa digit
        hash = (hash << 4) | (uint32_t)HEXTOI(digest[i]);
    }

    hashAddress = get_openAddress(hash); // base address
    hashProbe = get_probeOffset(hash); // probe offset

    // Probe for an existing singleton
    DEBUGPRINTF("hash is %06x so probe from %06x", hash, hashAddress);

    probeAddress = hashAddress;
    int safeguard = PROBE_LIMIT;
    while (--safeguard) { // probing limit
        Cell probed;
        CELL_READ(&probed, probeAddress);

        if (probed.full.type != CELLTYPE_EMPTY
            && probed.full.type <= CELLTYPE_ARROWLIMIT
            && probed.arrow.hash == hash) { // found candidate

            // get its digest
            char* otherDigest = serial_digest(probeAddress, &probed, NULL);
            if (!strncmp(otherDigest, digest, DIGEST_SIZE)) { // both digest match
                free(otherDigest);
                return probeAddress; // hit! arrow found!
            } else { // full digest mismatch
              free(otherDigest);
            }
        }

        if (probed.full.pebble == 0) {
          break; // Probing over
        }

        // shift probe
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    return XL_NIL; //< Probing over. It's a miss
}

/** assimilate the arrow corresponding to data at $str with size $length and precomputed $hash.
 * Will create the singleton if missing except if $ifExist is set.
 * $str might be a blob signature or a tag content.
 */
Address assimilate_string(int cellType, const int length, const uint8_t *str, int ifExist) {
    Address hashAddress, hashProbe, hChain;
    uint32_t l;
    Address probeAddress, firstFreeAddress, next;
    Cell probed, sliceCell;

    unsigned i, safeguard, jump;
    uint8_t c;
    const uint8_t *p;

    space_stats.atom++;

    if (!length) {
        return XL_EVE;
    }

    uint32_t hash = (uint32_t) hash_raw(str, length);
    hashAddress = get_openAddress(hash); // base address
    hashProbe = get_probeOffset(hash); // probe offset


    // Search for an existing singleton
    probeAddress = hashAddress;
    firstFreeAddress = XL_EVE;
    while (1) {
        CELL_READ(&probed, probeAddress);

        if (probed.full.type == CELLTYPE_EMPTY) {
            firstFreeAddress = probeAddress;

        } else if (probed.full.type == cellType
                  && probed.tagOrBlob.hash == hash
                  && !memcmp(probed.tagOrBlob.slice0, str, sizeof(probed.tagOrBlob.slice0))) {
        	// chance we found it
          // now comparing the whole string
          hChain = hash_chain(&probed);

          p = str + sizeof(probed.tagOrBlob.slice0);
          l = length - sizeof(probed.tagOrBlob.slice0);
          assert(l > 0);
          i = 0;

          next = cell_jumpToFirst(&probed, probeAddress, hChain);
          CELL_READ(&sliceCell, next);
          while ((c = *p++) == sliceCell.slice.data[i++] && --l) { // cmp loop
            if (i == sizeof(sliceCell.slice.data)) {
                // one shifts to the next linked cell
                if (sliceCell.full.type == CELLTYPE_LAST) {
                    break; // the chain is over
                }
                next = cell_jumpToNext(&sliceCell, next, hChain);
                CELL_READ(&sliceCell, next);
                i = 0;
            }
          } // cmp loop
          if (!l && sliceCell.full.type == CELLTYPE_LAST
             && sliceCell.last.size == i) {
            // exact match
            space_stats.found++;
            return probeAddress; // found arrow
          } // match
        } // if candidate
        // Not the singleton

        if (!probed.full.pebble) { // nothing further
            break; // Miss
        }

        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    if (ifExist) {
        // if one only wants to check arrow existency, then a miss is a miss
        return XL_EVE;
    }
    // else ... one creates a corresponding arrow right now

    space_stats.new++;
    space_stats.atom++;

    if (firstFreeAddress == XL_EVE) {
        Cell probed;

        int safeguard = PROBE_LIMIT;
        while (--safeguard) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
            CELL_READ(&probed, next);

            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        assert(safeguard);
    }


    // Create an arrow representation

    Address current, newArrow = firstFreeAddress;
    Cell newCell;
    CELL_READ(&newCell, newArrow);

    /*   |  hash  | slice0  | J |  R.C0d.Cn |  Child0  | cr | dr |
    *       4          7      1
    */
    newCell.full.type = cellType;
    memcpy(newCell.tagOrBlob.slice0, str, sizeof(newCell.tagOrBlob.slice0));
    newCell.arrow.hash = hash;
    newCell.arrow.RC0dCn = 0;
    newCell.arrow.child0 = XL_EVE;
    newCell.arrow.dr = space_stats.new & 0xFFu;

    hChain = hash_chain(&newCell);

    Address offset = hChain;
    ADDRESS_SHIFT(newArrow, next, offset);

    Cell nextCell, currentCell;
    CELL_READ(&nextCell, next);

    jump = 0;
    safeguard = PROBE_LIMIT;
    while (--safeguard && nextCell.full.type != CELLTYPE_EMPTY) {
        jump++;
        ADDRESS_SHIFT(next, next, offset);
        CELL_READ(&nextCell, next);
    }
    assert(safeguard);

    if (jump >= MAX_JUMP) {
        Address sync = next;
        Cell syncCell = nextCell;
        syncCell.full.type = CELLTYPE_REATTACHMENT;
        syncCell.reattachment.from = newArrow;
        syncCell.reattachment.to   = newArrow; // will be overwritten as soon as possible

        CELL_WRITE(&syncCell, next);

        offset = hChain;
        ADDRESS_SHIFT(next, next, offset);
        CELL_READ(&nextCell, next);

        safeguard = PROBE_LIMIT;
        while (--safeguard && nextCell.full.type == CELLTYPE_EMPTY) {
          ADDRESS_SHIFT(next, next, offset);
          CELL_READ(&nextCell, next);
        }
        assert(safeguard);

        syncCell.reattachment.to = next;
        CELL_WRITE(&syncCell, sync);

        jump = MAX_JUMP;
    }

    newCell.tagOrBlob.jump0 = jump;
    CELL_WRITE(&newCell, newArrow);

    current = next;
    currentCell = nextCell;
    p = str + sizeof(newCell.tagOrBlob.slice0);
    l = length - sizeof(newCell.tagOrBlob.slice0);
    i = 0;
    c = 0;
    assert(l);
    while (1) {
        c = *p++;
        currentCell.slice.data[i] = c;
        if (!--l) break;
        if (++i == sizeof(currentCell.slice.data)) {

            offset = hChain;
            ADDRESS_SHIFT(current, next, offset);
            CELL_READ(&nextCell, next);

            jump = 0;
            int safeguard = PROBE_LIMIT;
            while (nextCell.full.type != CELLTYPE_EMPTY && --safeguard) {
                jump++;
                ADDRESS_SHIFT(next, next, offset);
                CELL_READ(&nextCell, next);
            }
            assert(safeguard);

            if (jump >= MAX_JUMP) {
              Address sync = next;
              Cell syncCell = nextCell;
              syncCell.full.type = CELLTYPE_REATTACHMENT;
              syncCell.reattachment.from = current;
              syncCell.reattachment.to   = current; // will be overwritten as soon as possible
              CELL_WRITE(&syncCell, sync);

              offset = hChain;
              ADDRESS_SHIFT(next, next, offset);
              CELL_READ(&nextCell, next);

              int safeguard = PROBE_LIMIT;
              while (--safeguard && nextCell.full.type == CELLTYPE_EMPTY) {
                ADDRESS_SHIFT(next, next, offset);
                CELL_READ(&nextCell, next);
              }
              assert(safeguard);

              syncCell.reattachment.to = next;
              CELL_WRITE(&syncCell, sync);
              jump = MAX_JUMP;
            }
            currentCell.full.type = CELLTYPE_SLICE;
            currentCell.slice.jump = jump;
            CELL_WRITE(&currentCell, current);

            i = 0;
            current = next;
            CELL_READ(&currentCell, current);
        }

    }
    currentCell.full.type = CELLTYPE_LAST;
    currentCell.last.size = 1 + i /* size */;
    CELL_WRITE(&currentCell, current);

    // Add "pebbles" in the probing path up to the new singleton
    add_pebbles(newArrow, hash);

    // log loose arrow
    weaver_addLoose(newArrow);
    return newArrow;
}

/** assimilate a tag arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_tag(const uint32_t size, const uint8_t* data, int ifExist) {
    return assimilate_string(CELLTYPE_TAG, size, data, ifExist);
}

/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_blob(const uint32_t size, const uint8_t* data, int ifExist) {
    char signature[CRYPTO_SIZE + 1];
    hash_crypto(size, data, signature);
    assert(strlen(signature) == CRYPTO_SIZE);

    // A BLOB consists in:
    // - A signature, stored as a specialy typed tag in the arrows space.
    // - data stored separatly in some traditional filer outside the arrows space.
    mem0_saveData(signature, (size_t)size, data);
    // TODO: remove data when cell at h is recycled.
    return assimilate_string(CELLTYPE_BLOB, CRYPTO_SIZE, (uint8_t *)signature, ifExist);
}

/** assimilate small
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Address assimilate_small(const int length, const uint8_t* str, int ifExist) {
    DEBUGPRINTF("small(%02x %.*s %1x) begin", length, length, str, ifExist);

    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress, firstFreeAddress;
    uint32_t uint_buffer[3];
    uint8_t* buffer = (uint8_t *)uint_buffer;

    if (length == 0 || length > 11)
        return XL_NIL;


    memcpy(buffer + 4, str, (length > 8 ? 8 : length));
    if (length < 8) {
        memset(buffer + 4 + length, (uint8_t)length, 8 - length);
    }
    if (length > 8) {
        // trust me
        memcpy(buffer, str + 7, length - 7);
        *buffer = (uint8_t)length;
        if (length < 11) {
           memset(buffer + length - 7, (char)length, 11 - length);
        }
    } else {
      memset(buffer, (uint8_t)length, 4);
    }

    /*| s | hash3 |        data       |  R.C0d.Cn  |  Child0  | cr | dr |
    *   s : small size (0 < s <= 11)
    *   hash3: (data 1st word ^ 2d word ^ 3d word) with s completion
    */
    buffer[1] = buffer[1] ^ buffer[5] ^ buffer[9];
    buffer[2] = buffer[2] ^ buffer[6] ^ buffer[10];
    buffer[3] = buffer[3] ^ buffer[7] ^ buffer[11];

    space_stats.get++;

    // Compute hashs
    hash = uint_buffer[0];
    hashAddress = get_openAddress(hash); // base address
    hashProbe = get_probeOffset(hash); // probe offset

    // Probe for an existing singleton
    probeAddress = hashAddress;
    firstFreeAddress = XL_EVE;

    int safeguard = PROBE_LIMIT;
    while (--safeguard) { // probe loop

        // get probed cell
        Cell probed;
        CELL_READ(&probed, probeAddress);

        if (probed.full.type == CELLTYPE_EMPTY) {
            // save the address of the free cell encountered
            firstFreeAddress = probeAddress;

        } else if (probed.full.type == CELLTYPE_SMALL
                && 0 == memcmp(probed.full.data, buffer, 12)) {
            space_stats.found++;
            return probeAddress; // OK: arrow found!
        }
        // Not the singleton

        if (probed.full.pebble == 0) {
            break; // Probing over. It's a miss.
        }

        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }
    // Miss

    if (ifExist) // one only wants to test for singleton existence in the arrows space
        return XL_EVE; // Eve means not found

    space_stats.new++;
    space_stats.pair++;

    if (firstFreeAddress == XL_EVE) {
        Cell probed;
        safeguard = PROBE_LIMIT;
        while (--safeguard) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
            CELL_READ(&probed, probeAddress);
            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        assert(safeguard);
    }

    // Create a singleton
    Address newArrow = firstFreeAddress;
    Cell newCell;
    CELL_READ(&newCell, newArrow);
    memcpy(&newCell, buffer, 12);
    newCell.arrow.hash = hash;
    newCell.arrow.RC0dCn = 0;
    newCell.arrow.child0 = XL_EVE;
    newCell.full.type = CELLTYPE_SMALL;
    newCell.arrow.dr = space_stats.new & 0xFFu;
    memcpy(newCell.full.data, buffer, 12);
    CELL_WRITE(&newCell, newArrow);

    // Add "pebbles" in the probing path up to the new singleton
    add_pebbles(newArrow, hash);

    // record new arrow as loose arrow
    weaver_addLoose(newArrow);
    return newArrow;
}
