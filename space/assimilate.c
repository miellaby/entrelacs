#include "assimilate.h"
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "serial.h"
#include "log/log.h"
#include "mem/mem.h"
#include "mem/geoalloc.h"
#include "space/hash.h"
#include "space/weaver.h"
#include "space/stats.h"


#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

/** assimilate tail->head pair
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_pair(Arrow tail, Arrow head, int ifExist) {
    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress, firstFreeAddress;
    int safeguard;

    if (tail == NIL || head == NIL)
        return NIL;
 
    space_stats.get++;

    // Compute hashs
    hash = hash_pair(cell_getHash(tail), cell_getHash(head));
    hashAddress = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0
    
    // Probe for an existing singleton
    Cell probed;
    probeAddress = hashAddress;
    firstFreeAddress = EVE;
    safeguard = PROBE_LIMIT;
    while (--safeguard) {
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));
    
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
    // Miss

    if (!safeguard) {
      ERRORPRINTF("dubious singleton probing");
      // replay for logging
      safeguard = PROBE_LIMIT;
      probeAddress = hashAddress;
      hashProbe = hash % PRIM1; // probe offset
      while (--safeguard) {
        cell_show(probeAddress);
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
      }
    }
    assert(safeguard);
    
    if (ifExist) // one only want to test for singleton existence in the arrows space
        return EVE; // Eve means not found

    space_stats.new++;
    space_stats.pair++;

    if (firstFreeAddress == EVE) {
        // You don't want safeguard = PROBE_LIMIT; otherwise
        // one may create arrows out of the probing limit
        while (--safeguard) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
        
            Cell probed;
            mem_get(probeAddress, &probed.u_body);
            ONDEBUG((LOGCELL('R', probeAddress, &probed)));
            
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
    mem_get(newArrow, &newCell.u_body);
    ONDEBUG((LOGCELL('R', newArrow, &newCell)));

    //
    newCell.pair.tail = tail;
    newCell.pair.head = head;
    newCell.arrow.hash = hash;
    newCell.arrow.RWWnCn = 0;
    newCell.arrow.child0 = 0;
    newCell.full.type = CELLTYPE_PAIR;
    newCell.arrow.dr = space_stats.new & 0xFFu;
    mem_set(newArrow, &newCell.u_body);
    ONDEBUG((LOGCELL('W', newArrow, &newCell)));

    /* Now incremeting "pebble" counters in the probing path up to the new singleton
     */
    probeAddress = hashAddress;
    hashProbe = hash % PRIM1; // reset hashProbe to default
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    while (probeAddress != newArrow) {
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));
        if (probed.full.pebble != PEEBLE_MAX)
          probed.full.pebble++;
        
        mem_set(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('W', probeAddress, &probed)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    // record new arrow as loose arrow
    weaver_addLoose(newArrow);
    return newArrow;
}

Arrow probe_digest(char *digest) {
    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress;
    Cell cell;

    // read the hash at the digest beginning
    assert(digest[0] == '$' && digest[1] == 'H');
    int i = 2; // jump over '$H' string
    hash = HEXTOI(digest[i]);
    while (++i < 10) { // read 8 hexa digit
        hash = (hash << 4) | (uint64_t)HEXTOI(digest[i]);
    }
    
    hashAddress = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    // Probe for an existing singleton
    DEBUGPRINTF("hash is %06x so probe from %06x", hash, hashAddress);

    probeAddress = hashAddress;
    int safeguard = PROBE_LIMIT;
    while (--safeguard) { // probing limit
        Cell probed;
        
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));

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
    
    return NIL; //< Probing over. It's a miss
}

/** assimilate the arrow corresponding to data at $str with size $length and precomputed $hash.
 * Will create the singleton if missing except if $ifExist is set.
 * $str might be a blob signature or a tag content.
 */
Arrow assimilate_string(int cellType, int length, char* str, uint64_t payloadHash, int ifExist) {
    Address hashAddress, hashProbe, hChain;
    uint32_t l;
    uint32_t hash;
    Address probeAddress, firstFreeAddress, next;
    Cell probed, sliceCell;

    unsigned i, safeguard, jump;
    char c, *p;

    space_stats.atom++;

    if (!length) {
        return EVE;
    }

    hash = payloadHash & 0xFFFFFFFFU;
    hashAddress = hash % PRIM0;
    hashProbe = hash % PRIM1;
    if (!hashProbe) hashProbe = 1; // offset can't be 0
   
    
    // Search for an existing singleton
    probeAddress = hashAddress;
    firstFreeAddress = EVE;
    while (1) {
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));

        if (probed.full.type == CELLTYPE_EMPTY) {
            firstFreeAddress = probeAddress;

        } else if (probed.full.type == cellType
                  && probed.tagOrBlob.hash == hash
                  && !memcmp(probed.tagOrBlob.slice0, str, sizeof(probed.tagOrBlob.slice0))) {
        	// chance we found it
          // now comparing the whole string
          hChain = hash_chain(&probed) % PRIM1;
          if (!hChain) hChain = 1; // offset can't be 0

          p = str + sizeof(probed.tagOrBlob.slice0);
          l = length - sizeof(probed.tagOrBlob.slice0);
          assert(l > 0);
          i = 0;

          next = cell_jumpToFirst(&probed, probeAddress, hChain);
          mem_get(next, &sliceCell.u_body);
          ONDEBUG((LOGCELL('R', next, &sliceCell)));

          while ((c = *p++) == sliceCell.slice.data[i++] && --l) { // cmp loop
            if (i == sizeof(sliceCell.slice.data)) {
                // one shifts to the next linked cell
                if (sliceCell.full.type == CELLTYPE_LAST) {
                    break; // the chain is over
                }
                next = cell_jumpToNext(&sliceCell, next, hChain);
                mem_get(next, &sliceCell.u_body);
                ONDEBUG((LOGCELL('R', next, &sliceCell)));
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
        return EVE;
    }
    // else ... one creates a corresponding arrow right now

    space_stats.new++;
    space_stats.atom++;

    if (firstFreeAddress == EVE) {
        Cell probed;

        int safeguard = PROBE_LIMIT;
        while (--safeguard) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);

            mem_get(probeAddress, &probed.u_body);
            ONDEBUG((LOGCELL('R', probeAddress, &probed)));
            
            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        assert(safeguard);
    }

   
    // Create an arrow representation

    Address current, newArrow = firstFreeAddress;
    Cell newCell, nextCell, currentCell;

    mem_get(newArrow, &newCell.u_body);
    ONDEBUG((LOGCELL('R', newArrow, &newCell)));


    /*   |  hash  | slice0  | J | R.W.Wn.Cn |  Child0  | cr | dr |
    *       4          7        1                     
    */
    newCell.tagOrBlob.hash = hash;
    memcpy(newCell.tagOrBlob.slice0, str, sizeof(newCell.tagOrBlob.slice0));
    newCell.arrow.RWWnCn = 0;
    newCell.arrow.child0 = 0;
    newCell.full.type = cellType;
    newCell.arrow.dr = space_stats.new & 0xFFu;

    hChain = hash_chain(&newCell) % PRIM1;
    if (!hChain) hChain = 1; // offset can't be 0

    Address offset = hChain;
    ADDRESS_SHIFT(newArrow, next, offset);
    
    mem_get(next, &nextCell.u_body);
    ONDEBUG((LOGCELL('R', next, &nextCell)));

    jump = 0;
    safeguard = PROBE_LIMIT;
    while (--safeguard && nextCell.full.type != CELLTYPE_EMPTY) {
        jump++;
        ADDRESS_SHIFT(next, next, offset);

        mem_get(next, &nextCell.u_body);
        ONDEBUG((LOGCELL('R', next, &nextCell)));
    }
    assert(safeguard);

    if (jump >= MAX_JUMP) {
        Address sync = next;
        Cell syncCell = nextCell; 
        syncCell.full.type = CELLTYPE_REATTACHMENT;
        syncCell.reattachment.from = newArrow;
        syncCell.reattachment.to   = newArrow; // will be overwritten as soon as possible
        
        mem_set(sync, &syncCell.u_body);
        ONDEBUG((LOGCELL('W', sync, &syncCell)));
        
        offset = hChain;
        ADDRESS_SHIFT(next, next, offset);
        
        mem_get(next, &nextCell.u_body);
        ONDEBUG((LOGCELL('R', next, &nextCell)));

        safeguard = PROBE_LIMIT;
        while (--safeguard && nextCell.full.type == CELLTYPE_EMPTY) {
          ADDRESS_SHIFT(next, next, offset);

          mem_get(next, &nextCell.u_body);
          ONDEBUG((LOGCELL('R', next, &nextCell)));
        }
        assert(safeguard);

        syncCell.reattachment.to = next;
        
        mem_set(sync, &syncCell.u_body);
        ONDEBUG((LOGCELL('W', sync, &syncCell)));

        jump = MAX_JUMP;
    }

    newCell.tagOrBlob.jump0 = jump;

    mem_set(newArrow, &newCell.u_body);
    ONDEBUG((LOGCELL('W', newArrow, &newCell)));
    
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

            mem_get(next, &nextCell.u_body);
            ONDEBUG((LOGCELL('R', next, &nextCell)));
            
            jump = 0;
            int safeguard = PROBE_LIMIT;
            while (nextCell.full.type != CELLTYPE_EMPTY && --safeguard) {
                jump++;
                ADDRESS_SHIFT(next, next, offset);
            
                mem_get(next, &nextCell.u_body);
                ONDEBUG((LOGCELL('R', next, &nextCell)));
            }
            assert(safeguard);
            
            if (jump >= MAX_JUMP) {
              Address sync = next;
              Cell syncCell = nextCell; 
              syncCell.full.type = CELLTYPE_REATTACHMENT;
              syncCell.reattachment.from = current;
              syncCell.reattachment.to   = current; // will be overwritten as soon as possible
              
              mem_set(sync, &syncCell.u_body);
              ONDEBUG((LOGCELL('W', sync, &syncCell)));
              
              offset = hChain;
              ADDRESS_SHIFT(next, next, offset);
              
              mem_get(next, &nextCell.u_body);
              ONDEBUG((LOGCELL('R', next, &nextCell)));

              int safeguard = PROBE_LIMIT;
              while (--safeguard && nextCell.full.type == CELLTYPE_EMPTY) {
                ADDRESS_SHIFT(next, next, offset);

                mem_get(next, &nextCell.u_body);
                ONDEBUG((LOGCELL('R', next, &nextCell)));
              }
              assert(safeguard);

              syncCell.reattachment.to = next;
              
              mem_set(sync, &syncCell.u_body);
              ONDEBUG((LOGCELL('W', sync, &syncCell)));

              jump = MAX_JUMP;
            }
            currentCell.full.type = CELLTYPE_SLICE;
            currentCell.slice.jump = jump;

            mem_set(current, &currentCell.u_body);
            ONDEBUG((LOGCELL('W', current, &currentCell)));

            i = 0;
            current = next;
            mem_get(current, &currentCell.u_body);
        }

    }
    currentCell.full.type = CELLTYPE_LAST;
    currentCell.last.size = 1 + i /* size */;
    mem_set(current, &currentCell.u_body);
    ONDEBUG((LOGCELL('W', current, &currentCell)));

    // Now incremeting "pebble" counters in the probing path up to the new singleton
    // important to reinitialize probing variables  
    probeAddress = /* hash % PRIM0 */ hashAddress;
    hashProbe = hash % PRIM1;
    if (!hashProbe)
      hashProbe = 1;
    while (probeAddress != newArrow) {
        Cell probed;
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));
        if (probed.full.pebble != PEEBLE_MAX)
          probed.full.pebble++;
        
        mem_set(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('W', probeAddress, &probed)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    // log loose arrow
    weaver_addLoose(newArrow);
    return newArrow;
}

/** assimilate a tag arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_tag(uint32_t size, char* data, int ifExist, uint64_t hash) {
    if (!hash) hash = hash_raw(data, size);
    return assimilate_string(CELLTYPE_TAG, size, data, hash, ifExist);
}

/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_blob(uint32_t size, char* data, int ifExist) {
    char signature[CRYPTO_SIZE + 1];
    hash_crypto(size, data, signature);
    uint32_t signature_size;
    uint64_t hash = hash_string(signature, &signature_size);

    // A BLOB consists in:
    // - A signature, stored as a specialy typed tag in the arrows space.
    // - data stored separatly in some traditional filer outside the arrows space.
    mem0_saveData(signature, (size_t)size, data);
    // TODO: remove data when cell at h is recycled.

    return assimilate_string(CELLTYPE_BLOB, signature_size, signature, hash, ifExist);
}
  
/** assimilate small
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
Arrow assimilate_small(int length, char* str, int ifExist) {
    DEBUGPRINTF("small(%02x %.*s %1x) begin", length, length, str, ifExist);

    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress, firstFreeAddress;
    uint32_t uint_buffer[3];
    char* buffer = (char *)uint_buffer;

    if (length == 0 || length > 11)
        return NIL;

    
    memcpy(buffer + 4, str, (length > 8 ? 8 : length));
    if (length < 8) {
        memset(buffer + 4 + length, (char)length, 8 - length);
    }
    if (length > 8) {
        // trust me
        memcpy(buffer, str + 7, length - 7);
        *buffer = (char)length;
        if (length < 11) {
           memset(buffer + length - 7, (char)length, 11 - length);
        }
    } else {
      memset(buffer, (char)length, 4);
    }
    
    /*| s | hash3 |        data       | R.W.Wn.Cn  |  Child0  | cr | dr |
    *   s : small size (0 < s <= 11)
    *   hash3: (data 1st word ^ 2d word ^ 3d word) with s completion 
    */
    buffer[1] = buffer[1] ^ buffer[5] ^ buffer[9];
    buffer[2] = buffer[2] ^ buffer[6] ^ buffer[10];
    buffer[3] = buffer[3] ^ buffer[7] ^ buffer[11];

    space_stats.get++;

    // Compute hashs
    hash = uint_buffer[0];
    hashAddress = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0
    
    // Probe for an existing singleton
    probeAddress = hashAddress;
    firstFreeAddress = EVE;

    int safeguard = PROBE_LIMIT;
    while (--safeguard) { // probe loop
    
        // get probed cell
        Cell probed;
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));

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

    if (ifExist) // one only want to test for singleton existence in the arrows space
        return EVE; // Eve means not found

    space_stats.new++;
    space_stats.pair++;

    if (firstFreeAddress == EVE) {
        Cell probed;
        safeguard = PROBE_LIMIT;
        while (--safeguard) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
            mem_get(probeAddress, &probed.u_body);
            ONDEBUG((LOGCELL('R', probeAddress, &probed)));
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
    mem_get(newArrow, &newCell.u_body);
    ONDEBUG((LOGCELL('R', newArrow, &newCell)));

    memcpy(&newCell, buffer, 12);
    newCell.arrow.RWWnCn = 0;
    newCell.arrow.child0 = 0;
    newCell.full.type = CELLTYPE_SMALL;
    newCell.arrow.dr = space_stats.new & 0xFFu;
    memcpy(newCell.full.data, buffer, 12);
    mem_set(newArrow, &newCell.u_body);
    ONDEBUG((LOGCELL('W', newArrow, &newCell)));

    /* Now incremeting "more" counters in the probing path up to the new singleton
     */
    probeAddress = hashAddress;
    hashProbe = hash % PRIM1; // reset hashProbe to default
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    while (probeAddress != newArrow) {

        // get probed cell
        Cell probed;
        mem_get(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &probed)));
        if (probed.full.pebble != PEEBLE_MAX)
          probed.full.pebble++;
        
        mem_set(probeAddress, &probed.u_body);
        ONDEBUG((LOGCELL('W', probeAddress, &probed)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    // record new arrow as loose arrow
    weaver_addLoose(newArrow);
    return newArrow;
}



