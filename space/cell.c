#include <assert.h>
#include <string.h>
#include "space/cell.h"
#include "space/hash.h"
#include "log/log.h"
#include "mem/mem.h"
#include "mem/geoalloc.h"

Address cell_getTail(Address a) {
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
        return -1; // Invalid id

    if (cell.full.type != CELLTYPE_PAIR)
        return a; // for atom, tail = self
    else
        return cell.pair.tail;
}

Address cell_getHead(Address a) {
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
        return -1; // Invalid id

    if (cell.full.type != CELLTYPE_PAIR)
        return a; // for atom, head = self
    else
        return cell.pair.head;
}

void cell_getSmallPayload(Cell *cell, char* buffer) {
    int bigSmall = cell->small.s > 8;
    memcpy(buffer, cell->small.data, bigSmall ? 8 : cell->small.s);
    if (bigSmall) {
        buffer[8] = cell->small.hash3[0] ^ cell->small.data[1] ^ cell->small.data[5];
        if (cell->small.s > 9) {
            buffer[9] = cell->small.hash3[1] ^ cell->small.data[2] ^ cell->small.data[6];
            if (cell->small.s > 10) {
                 buffer[10] = cell->small.hash3[2] ^ cell->small.data[3] ^ cell->small.data[7];
            }
        }
    }

}

/** function used to go to the first cell in a chain
 */
Address cell_jumpToFirst(Cell* cell, Address address, Address offset) {
    Address next = address;
    assert(CELL_CONTAINS_ATOM(*cell));

    int jump = cell->tagOrBlob.jump0 + 1; // Note: jump=1 means 2 shifts

    ADDRESS_JUMP(address, next, offset, jump);

    if (jump == MAX_JUMP0 + 1) {
        // one needs to look for a reattachment (sync) cell
        offset += MAX_JUMP0 + 1;
        Cell probed;
        mem_get(next, &probed.u_body);
        ONDEBUG((LOGCELL('R', next, &probed)));

        int safeguard = PROBE_LIMIT;
        while (--safeguard && !(
                probed.full.type == CELLTYPE_REATTACHMENT
                && probed.reattachment.from == address)) {
            ADDRESS_SHIFT(next, next, offset);
            mem_get(next, &probed.u_body);
            ONDEBUG((LOGCELL('R', next, &probed)));
        }
        assert(safeguard);

        next = probed.reattachment.to;
    }
    return next;
}

Address cell_jumpToNext(Cell* cell, Address address, Address offset) {
    Address next = address;
    assert(cell->full.type == CELLTYPE_SLICE);

    uint32_t jump = cell->slice.jump + 1; // Note: jump == 1 means 2 shifts

    ADDRESS_JUMP(address, next, offset, jump);

    if (jump == MAX_JUMP + 1) {
        // one needs to look for a reattachment (sync) cell
        Cell probed;
        mem_get(next, &probed.u_body);
        ONDEBUG((LOGCELL('R', next, &probed)));
        offset += MAX_JUMP + 1;
        int safeguard = PROBE_LIMIT;
        while (--safeguard && !(
                probed.full.type == CELLTYPE_REATTACHMENT
                && probed.reattachment.from == address)) {
            ADDRESS_SHIFT(next, next, offset);
            mem_get(next, &probed.u_body);
            ONDEBUG((LOGCELL('R', next, &probed)));
        }
        assert(safeguard);

        next = probed.reattachment.to;
    }
    return next;
}

/** return the payload of an arrow (blob/tag/small), NULL on error */
char* cell_getPayload(Address a, Cell* cellp, uint32_t* lengthP) {
    // the result
   char* payload = NULL;
   uint32_t size = 0;

   if (a == EVE) { // Eve has an empty payload, length = 0
       
       // allocate and return an empty string
       payload = (char*) malloc(1);
       if (!payload) { // allocation failed
           return NULL;
       }

       payload[0] = '\0';
       *lengthP = 0; // if asked, return length = 0
       return payload;
   }

   // cell pointed by a
   Cell cell = *cellp;

   if (cell.full.type == CELLTYPE_SMALL) { // type "small"
     // small typed cell is a simple case

     // allocate a buffer (size + 1 because of null terminator)
     payload = malloc(cell.small.s + 1);
     if (!payload) { // allocation failed
         return NULL;
     }

     cell_getSmallPayload(&cell, payload);
     
     // add a null terminator
     payload[cell.small.s] = '\0';
     
     *lengthP = cell.small.s;
     return payload;
   }

   // the offset used for chain retrieval
   uint32_t hChain = hash_chain(cellp) % PRIM1;
   if (!hChain) hChain = 1; // offset can't be 0

   uint32_t max = 0; // geoalloc() state variables
   geoalloc((char**) &payload, &max, &size, sizeof (char), sizeof(cell.tagOrBlob.slice0));
   if (!payload) { // allocation failed
     return NULL;
   }

   memcpy(payload, cell.tagOrBlob.slice0, sizeof(cell.tagOrBlob.slice0));
   
   Address next = cell_jumpToFirst(cellp, a, hChain);
   Cell sliceCell;
   mem_get(next, &sliceCell.u_body);
   ONDEBUG((LOGCELL('R', next, &sliceCell)));

   while (1) {
       assert (sliceCell.full.type == CELLTYPE_SLICE || sliceCell.full.type == CELLTYPE_LAST);
     
       int s = sliceCell.full.type == CELLTYPE_SLICE
         ? sizeof(sliceCell.slice.data)
         : sliceCell.last.size;

       geoalloc((char**) &payload, &max, &size, sizeof (char), size + s);
       if (!payload) { // allocation failed
         return NULL;
       }

       memcpy(&payload[size - s], sliceCell.slice.data, s);
       if (sliceCell.full.type == CELLTYPE_LAST) {
           break; // the chain is over
       }

       // go to next slice
       next = cell_jumpToNext(&sliceCell, next, hChain);
       mem_get(next, &sliceCell.u_body);
       ONDEBUG((LOGCELL('R', next, &sliceCell)));
   }

   // payload size
   *lengthP = size;

   // 2 next lines: add a trailing 0
   geoalloc((char**) &payload, &max, &size, sizeof (char), size + 1);
   if (!payload) { // allocation failed
     return NULL;
   }
   payload[size - 1] = 0;

   return payload;
}


/** log cell access.
* operation = R/W
* address
* cell pointer
*/
void cell_log(int logLevel, int line, char operation, Address address, Cell* cell) {
    static const char* cats[] = {
          "EMPTY",
          "PAIR",
          "SMALL",
          "TAG",
          "BLOB",
          "SLICE",
          "LAST",
          "REATTACHMENT",
          "CHILDREN"
      };
    unsigned type = (unsigned)cell->full.type;
    unsigned pebble = (unsigned)cell->full.pebble;
    const char* cat = cats[type];
    if (type == CELLTYPE_EMPTY) {
        LOGPRINTF(logLevel, "%d %c %06x EMPTY (pebble=%02x)",
          line, operation, address, pebble);
        return;

    }
    if (type <= CELLTYPE_ARROWLIMIT) {
      char flags[] = {
          (cell->arrow.RWWnCn & FLAGS_ROOTED ? 'R' : '.'),
          (cell->arrow.RWWnCn & FLAGS_WEAK ? 'W' : '.'),
          (cell->arrow.child0 ? (cell->arrow.RWWnCn & FLAGS_C0D ? 'O' : 'I') : '.'),
          '\0'
      };
      uint32_t hash = cell->arrow.hash;
      int weakChildrenCount = (int)((cell->arrow.RWWnCn & FLAGS_WEAKCHILDRENMASK) >> 15);
      int childrenCount = (int)(cell->arrow.RWWnCn & FLAGS_CHILDRENMASK);
      int cr = (int)cell->arrow.cr;
      int dr = (int)cell->arrow.dr;

      if (type == CELLTYPE_PAIR) {
        LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x hash=%08x Flags=%s weakCount=%04x refCount=%04x child0=%06x cr=%02x dr=%02x %s tail=%06x head=%06x",
          line, operation, address, pebble, type,
          hash, flags, weakChildrenCount, childrenCount, cell->arrow.child0, cr, dr, cat,
          cell->pair.tail,
          cell->pair.head);

      } else if (type == CELLTYPE_SMALL) {
        int s = (int)cell->small.s;
        char buffer[11];
        cell_getSmallPayload(cell, buffer);
        LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x hash=%08x Flags=%s weakCount=%04x refCount=%04x child0=%06x cr=%02x dr=%02x %s size=%d data=%.*s",
          line, operation, address, pebble, type,
          hash, flags, weakChildrenCount, childrenCount, cell->arrow.child0, cr, dr, cat,
          s, s, buffer);

      } else if (type == CELLTYPE_BLOB || type == CELLTYPE_TAG) {
        LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x hash=%08x Flags=%s weakCount=%04x refCount=%04x child0=%06x cr=%02x dr=%02x %s jump0=%1x slice0=%.7s",
          line, operation, address, pebble, type,
          hash, flags, weakChildrenCount, childrenCount, cell->arrow.child0, cr, dr, cat,
          (int)cell->tagOrBlob.jump0, cell->tagOrBlob.slice0);

      }
    } else if (type == CELLTYPE_SLICE) {
      LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x %s jump=%1x data=%.21s",
        line, operation, address, pebble, type, cat,
        (int)cell->slice.jump, cell->slice.data);

    } else if (type == CELLTYPE_LAST) {
      LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x %s size=%d data=%.*s",
        line, operation, address, pebble, type, cat,
        (int)cell->last.size,
        (int)cell->last.size,
        cell->last.data);

    } else if (type == CELLTYPE_CHILDREN) {
      char directions[] = {
          '[',
          (cell->children.directions & 0x01 ? 'O' : '.'),
          (cell->children.directions & 0x02 ? 'O' : '.'),
          (cell->children.directions & 0x04 ? 'O' : '.'),
          (cell->children.directions & 0x08 ? 'O' : '.'),
          (cell->children.directions & 0x10 ? 'O' : '.'),
          (cell->children.directions & 0x0100 ? 'T' : '.'),
          (cell->children.directions & 0x0200 ? 'T' : '.'),
          (cell->children.directions & 0x0400 ? 'T' : '.'),
          (cell->children.directions & 0x0800 ? 'T' : '.'),
          (cell->children.directions & 0x1000 ? 'T' : '.'),
          ']',
          '\0'
      };
      LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x %s C[]={%x %x %x %x %x} D=%s",
        line, operation, address, pebble, type, cat,
        cell->children.C[0],
        cell->children.C[1],
        cell->children.C[2],
        cell->children.C[3],
        cell->children.C[4],
        directions);

    } else if (type == CELLTYPE_REATTACHMENT) {
      LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x %s from=%x to=%x",
        line, operation, address, pebble, type, cat,
        (int)cell->reattachment.from, cell->reattachment.to);

    } else {
      LOGPRINTF(logLevel, "%d %c %06x pebble=%02x type=%1x ANOMALY",
        line, operation, address, pebble, type);
      assert(0 == 1);
    }
}

void cell_show(Address a) {
    Cell cell;
    mem_get(a, &cell.u_body);
    cell_log(LOG_INFO, __LINE__, ' ', a, &cell);
}

/** debug: show children sequence
*
*/
void cell_showChildren(Address a) {
    INFOPRINTF("cell_showChildren a=%06x", a);

    if (a == EVE) {
        return; // Eve connectivity not traced
    }

    // get the parent cell
    Cell cell;
    mem_get(a, &cell.u_body);
    LOGCELL('R', a, &cell);

    // check arrow
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT) {
        ERRORPRINTF("Not an arrow");
        return; // invalid ID. TODO one might call cb with a
    }

    if (!(cell.arrow.RWWnCn & (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))) {
      // no child
      WARNPRINTF("Not an arrow");
      return;
    }

    // compute hash_children
    uint32_t hChild = hash_children(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    if (cell.arrow.child0) {
      // child0
      if ((cell.arrow.RWWnCn & (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK)) == 1) {
        // child0 is the only child
        INFOPRINTF("child0=%06x is the only child,", cell.arrow.child0);
        return;
      } else {
        INFOPRINTF("child0=%06x is the first child.", cell.arrow.child0);
      }
    }

    // probe children
    Address next = a;
    Cell nextCell;

    while(1) { // child probing loop

      // shift
      ADDRESS_SHIFT(next, next, hChild);

      // get the next cell
      mem_get(next, &nextCell.u_body);
      LOGCELL('R', next, &nextCell);

      if (nextCell.full.type == CELLTYPE_CHILDREN) {
        // One found a children cell

        int j = 0; //< slot index
        while (j < 5) { // slot scanning
          Address inSlot = nextCell.children.C[j];
          if (inSlot == a && (nextCell.children.directions & (1 << (8 + j)))) {
            INFOPRINTF("Terminator found at slot %d of cell@%06x.", j, next);
            return; // no child left
          } // child maybe found
          j++;
        } // slot loop
      } // children cell found
    } // child probing loop
}

uint32_t cell_getHash(Address a) {
    Cell cell;
    
    if (a == EVE)
      return EVE_HASH; // Eve hash is not zero!
    else if (a >= SPACE_SIZE)
      return 0; // Out of space
    else {
      mem_get(a, &cell.u_body);
      ONDEBUG((LOGCELL('R', a, &cell)));
      if (cell.full.type == CELLTYPE_EMPTY
         || cell.full.type > CELLTYPE_ARROWLIMIT)
        return 0; // Not an arrow
      else
        return cell.arrow.hash; // hash
    }
}
