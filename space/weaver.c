#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "space/weaver.h"
#include "space/stats.h"
#include "space/cell.h"
#include "space/hash.h"
#include "log/log.h"
#include "mem/mem.h"
#include "mem/geoalloc.h"
#include "entrelacs/entrelacs.h"

// The loose log.
// This is a dynamic array containing all arrows in loose state.
// it grows geometrically.
static Arrow  *looseLog = NULL;
static uint32_t looseLogMax = 0;
uint32_t looseLogSize = 0;

void weaver_addLoose(Address a) {
    geoalloc((char**) &looseLog, &looseLogMax, &looseLogSize, sizeof (Arrow), looseLogSize + 1);
    looseLog[looseLogSize - 1] = a;
}

void weaver_removeLoose(Address a) {
    assert(looseLogSize);
    // if the arrow is at the log end, then one reduces the log
    if (looseLog[looseLogSize - 1] == a)
      looseLogSize--;
    // else ... nothing is done.
}


/** connect a (weak or strong) child pair to its parent arrow "a".
 * it consists in adding the child to the children list of "a".
 * A pair gets connected to its both parents as soon as it gets rooted
 * or one of its descendants gets connected too.
 *
 * Steps:
 * 1) if it turns out the parent arrow was loose (i.e. unconnected + unrooted), then
 *   - one removes it from the "loose" log if its a strong connection
 *   - one connects the parent arrow to its own parents (recursive connect() calls)
 * 2) One updates the children counters in parent cell. One updates child0 (child0
 *    stores the last connected strong chid)
 * 3) Unless there is only one child, then one
*     probe cells -in the way of open addressing- til finding a children holding cell
 *     with a free slot or a free cell. If a free cell is found, one turns it into a
 *     a children holding cell.
 *
 *    back-ref is put there.
 * @param a the parent
 * @param child the child
 * @param childWeakness !0 if one builds up a weak connection
 */
void weaver_connect(Arrow a, Arrow child, int childWeakness, int outgoing) {
    TRACEPRINTF("weaver_connect child=%06x to a=%06x weakness=%1x outgoing=%1x", child, a, childWeakness, outgoing);
    if (a == EVE) return; // One doesn't store Eve connectivity. 18/8/11 Why not?
    space_stats.connect++;
    
    // get the cell at a
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    assert(cell.full.type != CELLTYPE_EMPTY && cell.full.type <= CELLTYPE_ARROWLIMIT);
  
    // is the parent arrow (@a) loose or connected?
    int childrenCount = (int)(cell.arrow.RWWnCn & FLAGS_CHILDRENMASK);
    int weakChildrenCount = (int)((cell.arrow.RWWnCn & FLAGS_WEAKCHILDRENMASK) >> 15);
    int loose = (childrenCount == 0 && !(cell.arrow.RWWnCn & FLAGS_ROOTED));
    int unconnected = loose & weakChildrenCount == 0; // a loose arrow without even weak children
    if (loose && !childWeakness) {  // parent is not loose anymore
        // (try to) remove 'a' from the loose log if strong child
        weaver_removeLoose(a);
    }

    if (unconnected) { // the parent arrow was not connected yet
        // the parent arrow is now connected. So it gets "connected" to both its own parents.
        if (cell.full.type == CELLTYPE_PAIR) {
            weaver_connect(cell.pair.tail, a, 0, 1 /* outgoing */);
            weaver_connect(cell.pair.head, a, 0, 0 /* incoming */);
        }
    }

    // detects there's no terminator at the children list end
    int noTerminatorYet = (weakChildrenCount == 0
                           && (childrenCount == 0
                            || childrenCount == 1 && cell.arrow.child0));

    // Update children counters in parent cell    
    if (childWeakness) {
      if (weakChildrenCount < MAX_WEAKREFCOUNT) {
          weakChildrenCount++;
      }
    } else {
      if (childrenCount < MAX_REFCOUNT) {
        childrenCount++;
      }
    }

    cell.arrow.RWWnCn = cell.arrow.RWWnCn
       & ((FLAGS_WEAKCHILDRENMASK | FLAGS_CHILDRENMASK) ^ 0xFFFFFFFFu) 
       | (weakChildrenCount << 15)
       | childrenCount;

    // Update child0
    if (!childWeakness) {
      // child0 always point the last strongly connnected child
      Arrow lastChild0 = cell.arrow.child0;
      int lastChild0Direction = ((cell.arrow.RWWnCn & FLAGS_C0D) ? 1 : 0);
      cell.arrow.child0 = child;

      if (outgoing) {
        cell.arrow.RWWnCn |= FLAGS_C0D;
      } else {
        cell.arrow.RWWnCn &= (FLAGS_C0D ^ 0xFFFFFFFFu);
      }

      child = lastChild0; // Now one puts the previous value of child0 into another cell
      outgoing = lastChild0Direction;
    }
    
    // write parent cell
    mem_set(a, &cell.u_body);
    ONDEBUG((LOGCELL('W', a, &cell)));

    if (child == EVE) {
      // child0 was empty and got the back-ref, nothing left to do
      return;
    }
  
    // Now, one puts a child reference into a children cell

    // compute hChild used by probing 
    uint32_t hChild = hash_children(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    // probing data
    Address next = a;
    Cell nextCell;
    int j; //< slot index

    while(1) { // probing loop

      // shift
      ADDRESS_SHIFT(next, next, hChild);

      // get the next cell
      mem_get(next, &nextCell.u_body);
      ONDEBUG((LOGCELL('R', next, &nextCell)));

      if (nextCell.full.type == CELLTYPE_EMPTY) {
        // One found a free cell

        // Turn the cell into a children cell
        memset(&nextCell.children.C, 0, sizeof(nextCell.children.C));
        nextCell.children.directions = 0;
        nextCell.full.type = CELLTYPE_CHILDREN;
       // as j=0, will fill up the first slot 
      }

      if (nextCell.full.type == CELLTYPE_CHILDREN) {
        // One found a children cell
        j = 0;
        while (j < 5) { // slot scanning
          Address inSlot = nextCell.children.C[j];
          
          if (noTerminatorYet) { // need to put a terminator
            /* why putting a terminator as soon?
               A: because one may probe several times the same cell,
               One risks couting the same children several times
               and eventualy adding a terminator at the wrong place by
               truncating the chain.
            */
            if (inSlot == 0 && child == a)
              break; // free slot, put the terminator here

            if (inSlot == 0) {
              // free slot, put child in free slot
              nextCell.children.C[j] = child;
          
              // set/reset flags
              nextCell.children.directions =
                nextCell.children.directions
                & ((0x101 << j) ^ 0xFFFF)
                | (outgoing ? (1 << j) : 0);
              mem_set(next, &nextCell.u_body);
              ONDEBUG((LOGCELL('W', next, &nextCell)));
        
              // now search for a free slot to put a terminator
              child = a;
            }
          } else { // normal case
            if (inSlot == 0)
              break; // free slot found

            if (inSlot == a && (nextCell.children.directions & (0x100 << j))) {
              // One found "a" children terminator

              // replace with child
              nextCell.children.C[j] = child;
            
              // set/reset flags
              nextCell.children.directions =
                nextCell.children.directions
                & ((1 << j) ^ 0xFFFF)
                & ((1 << (8 + j)) ^ 0xFFFF)
                | (outgoing ? (1 << j) : 0);
              mem_set(next, &nextCell.u_body);
              ONDEBUG((LOGCELL('W', next, &nextCell)));
        
              // Now one keeps on probing for a free slot to put the terminator into
              child = a;
            } // terminator found
          } // !noTerminatorYet

          j++;
        } // slot loop

        if (j < 5) { // probing over, free slot found
          break;
        }
      } // children cell 
    } // probing loop

    // fill up the free slot (terminator OR child)
    nextCell.children.C[j] = child;
    nextCell.children.directions =
      nextCell.children.directions
      & ((0x101 << j) ^ 0xFFFF)
      | ((child != a && outgoing) ? (1 << j) : 0) // outgoing flag
      | (child == a ? (0x100 << j) : 0); // terminator flag if one puts the terminator
    mem_set(next, &nextCell.u_body);
    ONDEBUG((LOGCELL('W', next, &nextCell)));
 }

/** disconnect a child pair from its parent arrow "a".
 * - it consists in removing the child from the children list of "a".
 * - a pair gets disconnected from its both parents as soon as it gets both unrooted and child-less
 *
 * Steps:
 * 1) compute counters
 * 2) if (child in child0 slot) remove it, update counters and done
 * 3) compute hash_children
 * 4) Scan children cells
 * 5) Scan children in each cell.
 * 6) When the child back ref is found, delete it
 * 7) If cell is empty, one may free it.
 * 8) If both counters == 0 or unique child in child0, one keeps on scaning
 *    the terminator
      When found, emptying and freeing     
 * 9) If counters == 0, and parent is not rooted, then it gets loose.
 *      * So one adds it to the "loose log"
 *      * one disconnects "a" from its both parents.
 */
void weaver_disconnect(Arrow a, Arrow child, int weakness, int outgoing) {
    TRACEPRINTF("weaver_disconnect child=%06x from a=%06x weakness=%1x outgoing=%1x", child, a, weakness, outgoing);
    space_stats.disconnect++;
    if (a == EVE) return; // One doesn't store Eve connectivity.

    // get parent arrow definition
    Cell parent;
    mem_get(a, &parent.u_body);
    ONDEBUG((LOGCELL('R', a, &parent)));
    // check it's a valid arrow
    assert(parent.full.type != CELLTYPE_EMPTY && parent.full.type <= CELLTYPE_ARROWLIMIT);

    // get counters
    int childrenCount = (int)(parent.arrow.RWWnCn & FLAGS_CHILDRENMASK);
    int weakChildrenCount = (int)((parent.arrow.RWWnCn & FLAGS_WEAKCHILDRENMASK) >> 15);

    // update child counters
    if (weakness) {
      if (weakChildrenCount < MAX_WEAKREFCOUNT) {
        weakChildrenCount--;
      }
    } else {
      if (childrenCount < MAX_REFCOUNT) {
        childrenCount--;
      }
    }
    parent.arrow.RWWnCn = parent.arrow.RWWnCn
       & ((FLAGS_WEAKCHILDRENMASK | FLAGS_CHILDRENMASK) ^ 0xFFFFFFFFu) 
       | (weakChildrenCount << 15) | childrenCount;

    // check "loose" status evolution after child counters update
    int loose = (childrenCount == 0
                 && !(parent.arrow.RWWnCn & FLAGS_ROOTED));
    if (!weakness && loose) { // parent arrow got loose by removing the last strong child
       // add 'a' into the loose log if strong child
       weaver_addLoose(a);
    }

    if (parent.full.type == CELLTYPE_PAIR) { // parent arrow is a pair
        int disconnected = (childrenCount == 0 && weakChildrenCount == 0
                        && !(parent.arrow.RWWnCn & FLAGS_ROOTED));
        if (disconnected) { // got disconnected
            // So one "disconnects" it from its own parents.
            weaver_disconnect(parent.pair.tail, a, 0, 1);
            weaver_disconnect(parent.pair.head, a, 0, 0);
        }
    }

    // child0 simple case
    if (parent.arrow.child0 == child) {
      int child0direction = parent.arrow.RWWnCn & FLAGS_C0D;
      if (child0direction && outgoing || !(child0direction || outgoing)) {
        parent.arrow.child0 = 0;
        parent.arrow.RWWnCn &= (FLAGS_C0D ^ 0xFFFFFFFFu);
        child = 0; // OK
      }
    }

    // write parent cell
    mem_set(a, &parent.u_body);
    ONDEBUG((LOGCELL('W', a, &parent)));

    if (child == 0) { // OK done
      return;
    }

    // compute parent arrow hChild (offset for child list)
    uint32_t hChild = hash_children(&parent) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    // probing data
    Address next = a;
    Cell nextCell;
    int j; //< slot index

    // If one child, one removes the list terminator
    int removeTerminatorNow = 
            !weakness
              && weakChildrenCount == 0
              && (childrenCount == 0
                  || childrenCount == 1
                     && parent.arrow.child0 != 0)
            || weakness
              && weakChildrenCount == 0
              && (childrenCount == 0
                 || childrenCount == 1
                    && parent.arrow.child0 != 0);

    while(1) { // child probing loop

      // shift
      ADDRESS_SHIFT(next, next, hChild);

      // get the next cell
      mem_get(next, &nextCell.u_body);
      ONDEBUG((LOGCELL('R', next, &nextCell)));

      if (nextCell.full.type == CELLTYPE_CHILDREN) {
        // One found a children cell

        j = 0;
        while (j < 5) { // slot scanning
            Arrow inSlot = nextCell.children.C[j];
            if (inSlot == child
                && (   child == a && (nextCell.children.directions & (1 << (j + 8)))
                    || child != a && 
                      (    outgoing && (nextCell.children.directions & (1 << j))
                       || !outgoing && !(nextCell.children.directions & (1 << j))))
               ) { // back-ref or terminator found
              
              // unload slot
              nextCell.children.C[j] = 0;
          
              if (nextCell.children.C[0] | nextCell.children.C[1]
                  | nextCell.children.C[2] | nextCell.children.C[3]
                  | nextCell.children.C[4]) {
                  // cell is not fully empty
                  // set/reset flags
                  nextCell.children.directions =
                    nextCell.children.directions
                    & (((1 << j) | (1 << (8 + j))) ^ 0xFFFF);
     
              } else {
                  // Empty children cell can be recycled
                  // because the terminator isn't inside
                  //
                  // cell is fully empty
                  // mark as is
                  memset(nextCell.full.data, 0, sizeof(nextCell.full.data));
                  nextCell.full.type = CELLTYPE_EMPTY;
                  // but don't delete pebble!
              } 
              mem_set(next, &nextCell.u_body);
              ONDEBUG((LOGCELL('W', next, &nextCell)));

              if (!removeTerminatorNow) {
                return; // back-ref removed. Job done

              } else if (child == a) {
                return; // terminator removed. Job done

              }  else {
                // back-ref removed but not terminator yet
                child = a;
              }

            } // target found
            j++;
        } // slot loop
      } // children cell found
    } // child probing loop
} // disconnect





/* TODO Réécrire forget en ajoutant la suppression des weak */


/** forget a loose arrow, that is actually remove it from the main memory */
void weaver_forgetLoose(Arrow a) {
    TRACEPRINTF("forget loose arrow %06x", a);
    space_stats.forget++;

    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    assert(cell.full.type != CELLTYPE_EMPTY
        || cell.full.type <= CELLTYPE_ARROWLIMIT);

    uint32_t hash = cell.arrow.hash;
    Address hashAddress; // base address
    Address hashProbe; // probe offset
    
    if (cell.full.type == CELLTYPE_TAG
        || cell.full.type == CELLTYPE_BLOB) {
        // Free chain
        // FIXME this code doesn't erase reattachment cells :( don't use jump functions
        Address hChain = hash_chain(&cell) % PRIM1;
        if (!hChain) hChain = 1; // offset can't be 0

        Address next = cell_jumpToFirst(&cell, a, hChain);
        Cell sliceCell;
        mem_get(next, &sliceCell.u_body);
        ONDEBUG((LOGCELL('R', next, &sliceCell)));
        while (sliceCell.full.type == CELLTYPE_SLICE) {
            // free cell
            Address current = next;
            next = cell_jumpToNext(&sliceCell, next, hChain);

            memset(sliceCell.full.data, 0, sizeof(sliceCell.full.data));
            sliceCell.full.type = CELLTYPE_EMPTY;
 
            mem_set(current, &sliceCell.u_body);
            ONDEBUG((LOGCELL('W', current, &sliceCell)));
            
            mem_get(next, &sliceCell.u_body);
            ONDEBUG((LOGCELL('R', next, &sliceCell)));
        }
        assert(sliceCell.full.type == CELLTYPE_LAST);
        // free cell
        memset(sliceCell.full.data, 0, sizeof(sliceCell.full.data));
        sliceCell.full.type = CELLTYPE_EMPTY;
 
        mem_set(next, &sliceCell.u_body);
        ONDEBUG((LOGCELL('W', next, &sliceCell)));
    }

    // Free definition start cell
    memset(cell.full.data, 0, sizeof(cell.full.data));
    cell.full.type = CELLTYPE_EMPTY;
    mem_set(a, &cell.u_body);
    ONDEBUG((LOGCELL('W', a, &cell)));

    hashAddress = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    /* Now decremeting "pebble" counters in the probing path
     * up to the forgotten singleton
     */
    Address probeAddress = hashAddress;
    // ONDEBUG(if (probeAddress != a) DEBUGPRINTF("real address %X != open Address %X", a, probeAddress));
    while (probeAddress != a) {
        mem_get(probeAddress, &cell.u_body);
        ONDEBUG((LOGCELL('R', probeAddress, &cell)));
        if (cell.full.pebble)
          cell.full.pebble--;

        mem_set(probeAddress, &cell.u_body);
        ONDEBUG((LOGCELL('W', probeAddress, &cell)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

}


void weaver_performGC() {
    TRACEPRINTF("BEGIN weaver_performGC()");

    for (unsigned i = looseLogSize; i > 0; i--) { // loose stack scanning
        Arrow a = looseLog[i - 1];

        if (cell_isLoose(a)) { // a loose arrow is removed NOW
            weaver_forgetLoose(a);
        }
    }

    TRACEPRINTF("END spaceGC() looseLogSize=%d get=%d root=%d unroot=%d new=%d (pair=%d atom=%d) found=%d connect=%d, disconnect=%d forget=%d",
        looseLogSize, space_stats.get, space_stats.root,
        space_stats.unroot, space_stats.new, 
        space_stats.pair, space_stats.atom, space_stats.found,
        space_stats.connect, space_stats.disconnect, space_stats.forget);
    space_stats = space_stats_zero;

    zeroalloc((char**) &looseLog, &looseLogMax, &looseLogSize);
}

int weaver_init() {
    geoalloc((char**) &looseLog, &looseLogMax, &looseLogSize, sizeof(Arrow), 0);
    return 0;
}
void weaver_destroy() {
    free(looseLog);
}
