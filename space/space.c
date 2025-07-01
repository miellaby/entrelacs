/*
// WORK IN PROGRESS
// Weak rooting / "weak" flag
// - week arrow doesn't prevent parents GC
// - removed when parents removed
// - Arrow flags:
//   R: root flag 
//   W: weak-root flag
//   If (W && !R): GC when both ends are unrooted
//
// IN STUDY
// - Weakness type: tail-weak, head-weak, both-weak
// - New API: weak(arrow)
// if: weak(A(root(atom('hello')), atom('world'))
// when 'hello' unrooted, '/hello+world' is unrooted as well
// attention: prefer xls_weak_link(C,A,B) = weak((C,A)=>(C,B)) then use xls_partnersOf(C,A)
// xls_weak(c,x) add x in context c without preventing c removal and GC when no other content
*/

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <printf.h>
#include <ctype.h>
#include <pthread.h>
#include "entrelacs/entrelacs.h"
#include "mem/mem0.h"
#include "mem/geoalloc.h"
#include "mem/mem.h"
#include "sha1.h"
#define LOG_CURRENT LOG_SPACE
#include "log/log.h"
#include "space/cell.h"
#include "space/stats.h"
#include "space/hash.h"
#include "space/assimilate.h"
#include "space/serial.h"
#include "space/weaver.h"

struct s_space_stats space_stats_zero = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}, space_stats = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Things to make the API parallelizable 
 */
static pthread_mutexattr_t apiMutexAttr;
static pthread_mutex_t apiMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() pthread_mutex_lock(&apiMutex)
#define LOCK_END() pthread_mutex_unlock(&apiMutex)
#define LOCK_OUT(X) (pthread_mutex_unlock(&apiMutex), (Arrow)X)
#define LOCK_OUT64(X) (pthread_mutex_unlock(&apiMutex), (uint64_t))
#define LOCK_OUTSTR(X) (pthread_mutex_unlock(&apiMutex), (char*)X)


static pthread_cond_t apiNowDormant;   // Signaled when all thread are ready to commit
static pthread_cond_t apiNowActive;    // Signaled once a thread has finished commit
static int transactionCount = 0; // Active Transaction counter; Incremented/Decremented by xl_begin/xl_over. Decremented when waiting for commit. Incremented again after.
static int spaceGCDone = 1; // 1->0 when a thread starts waiting in xl_commit()/xl_over(), 0->1 when all threads synced and GC is done
static int memCommitDone = 1; // 1->0 when a thread starts waiting in xl_commit(), 0->1 when all threads synced and mem_commit(); occurs only if !memCommitDone
static int memCloseDone = 1; // 1->0 when a xl_over thread starts waiting, 0->1 when all threads synced and a xl_commit() thread does commit (without actually mem_close)

#define ENTER_TRANSACTION() (!transactionCount++ ? (void)pthread_cond_signal(&apiNowActive) : (void)0)
#define LEAVE_TRANSACTION() (transactionCount ? (--transactionCount ? (void)0 : (void)pthread_cond_signal(&apiNowDormant)) : (void)0)
#define WAIT_DORMANCY() (transactionCount > 0 ? (void)pthread_cond_wait(&apiNowDormant, &apiMutex) : (void)0)

/*
 * Eve
 */
const Arrow Eve = EVE;

// The loose log.
// This is a dynamic array containing all arrows in loose state.
// it grows geometrically.
static Arrow  *looseLog = NULL;
static uint32_t looseLogMax = 0;
static uint32_t looseLogSize = 0;

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

/** return Eve */
Arrow xl_Eve() {
    return EVE;
}

uint32_t xl_hashOf(Arrow a) {
    LOCK();
    uint32_t cs = cell_getHash(a);
    return LOCK_OUT(cs);
}


Arrow xl_pair(Arrow tail, Arrow head) {
    LOCK();
    Arrow a = assimilate_pair(tail, head, 0);
    return LOCK_OUT(a);
}

Arrow xl_pairMaybe(Arrow tail, Arrow head) {
    LOCK();
    Arrow a= assimilate_pair(tail, head, 1);
    return LOCK_OUT(a);
}

Arrow xl_atom(char* str) {
    uint32_t size;
    uint64_t hash = hash_string(str, &size);
    LOCK();
    Arrow a =
      (size == 0 ? // EVE has a zero payload
        EVE : (size < TAG_MINSIZE
            ? assimilate_small(size, str, 0)
            : (size >= BLOB_MINSIZE
              ? assimilate_blob(size, str, 0)
              : assimilate_tag(size, str, 0, hash))));
    return LOCK_OUT(a);
}

Arrow xl_atomMaybe(char* str) {
    uint32_t size;
    uint64_t hash = hash_string(str, &size);
    LOCK();
    Arrow a =
      (size == 0 ? EVE // EVE 0 payload
      : (size < TAG_MINSIZE
        ? assimilate_small(size, str, 1)
        : (size >= BLOB_MINSIZE
          ? assimilate_blob(size, str, 1)
          : assimilate_tag(size, str, 1, hash))));
    return LOCK_OUT(a);
}

Arrow xl_atomn(uint32_t size, char* mem) {
    Arrow a;
    LOCK();
    if (size == 0)
        a = EVE;
    else if (size < TAG_MINSIZE) {
        a = assimilate_small(size, mem, 0);
    } else if (size >= BLOB_MINSIZE) {
        a = assimilate_blob(size, mem, 0);
    } else {
        a = assimilate_tag(size, mem, 0, /*hash*/ 0);
    }
    return LOCK_OUT(a);
}

Arrow xl_atomnMaybe(uint32_t size, char* mem) {
    Arrow a;
    LOCK();
    if (size == 0)
        a = EVE;
    else if (size < TAG_MINSIZE) {
        a = assimilate_small(size, mem, 1);
    } else if (size >= BLOB_MINSIZE)
        a = assimilate_blob(size, mem, 1);
    else {
        a = assimilate_tag(size, mem, 1, /*hash*/ 0);
    }
    return LOCK_OUT(a);
}

Arrow xl_headOf(Arrow a) {
    if (a == EVE) return EVE;
    LOCK();
    return LOCK_OUT(cell_getHead(a));
}

Arrow xl_tailOf(Arrow a) {
    if (a == EVE) return EVE;
    LOCK();
    return LOCK_OUT(cell_getTail(a));
}

/** return the content behind an atom
*/
char* xl_memOf(Arrow a, uint32_t* lengthP) {
    if (a == EVE) { // Eve has an empty payload
        return cell_getPayload(EVE, NULL, lengthP);
    }

    // Lock mem access
    LOCK();

    // get the cell pointed by a
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));

    if (cell.full.type < CELLTYPE_SMALL
        || cell.full.type > CELLTYPE_BLOB) { // Not an atom (empty cell, pair, ...)
        return LOCK_OUTSTR(NULL);
    }

    
    uint32_t payloadLength;
    char* payload = cell_getPayload(a, &cell, &payloadLength);
    if (payload == NULL) { // Error
        return LOCK_OUTSTR(NULL);
    }

    if (cell.full.type == CELLTYPE_BLOB) {
        // The payload is a BLOB footprint
        size_t blobLength;
        char* blobData = mem0_loadData(payload, &blobLength);
        if (blobLength > UINT32_MAX)
          blobLength = UINT32_MAX;
        *lengthP = blobLength;
        free(payload); // one doesn't return the payload, free...
        // *lengthP = ... set by mem0_loadData call
        return LOCK_OUTSTR(blobData);
    }

    // else: Tag or small case
    *lengthP = payloadLength;
    return LOCK_OUTSTR(payload);
}

char* xl_strOf(Arrow a) {
    uint32_t lengthP;
    char* p = xl_memOf(a, &lengthP);
    return p;
}


char* xl_digestOf(Arrow a, uint32_t *l) {
    TRACEPRINTF("BEGIN xl_digestOf(%06x)", a);

    if (a >= SPACE_SIZE) { // Address anomaly
        return NULL;
    }

    LOCK();

    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));

    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT) {
        // Address anomaly : not an arrow
        return LOCK_OUTSTR(NULL);
    }

    char *digest = serial_digest(a, &cell, l);
    
    return LOCK_OUTSTR(digest);
}

char* xl_uriOf(Arrow a, uint32_t *l) {
    LOCK();
    char *str = serial_toURI(a, l);
    return LOCK_OUTSTR(str);
}

Arrow xl_digestMaybe(char* digest) {
    TRACEPRINTF("BEGIN xl_digestMaybe(%.*s)", DIGEST_SIZE, digest);
    LOCK();
    return LOCK_OUT(probe_digest(digest));
}

Arrow xl_uri(char* aUri) {
    LOCK();
    Arrow a = serial_parseURIs(NAN, aUri, 0);
    return LOCK_OUT(a);
}

Arrow xl_uriMaybe(char* aUri) {
    LOCK();
    Arrow a = serial_parseURIs(NAN, aUri, 1);
    return LOCK_OUT(a);
}

Arrow xl_urin(uint32_t aSize, char* aUri) {
    LOCK();
    Arrow a = serial_parseURIs(aSize, aUri, 0);
    return LOCK_OUT(a);
}

Arrow xl_urinMaybe(uint32_t aSize, char* aUri) {
    LOCK();
    Arrow a = serial_parseURIs(aSize, aUri, 1);
    return LOCK_OUT(a);
}

Arrow xl_anonymous() {
    char anonymous[CRYPTO_SIZE + 1];
    Arrow a = NIL;
    do {
        char random[80];
        snprintf(random, sizeof(random), "an0nymous:)%lx", (long)rand() ^ (long)time(NULL));
        hash_crypto(80, random,  anonymous); // Access to unitialized data is wanted
        a = xl_atomMaybe(anonymous);
        assert(a != NIL);
    } while (a);
    return xl_atom(anonymous);
}

static Arrow unrootChild(Arrow child, Arrow context) {
    xl_unroot(child);
    return EVE;
}

static Arrow getHookBadge() {
    static Arrow hookBadge = EVE;
    if (hookBadge != EVE) return hookBadge; // try to avoid the costly lock.
    LOCK();
    if (hookBadge != EVE) return LOCK_OUT(hookBadge);
    hookBadge = xl_atom("XLhO0K");
    xl_root(hookBadge); 
    
    // prevent previous hooks to survive the reboot.
    xl_childrenOfCB(hookBadge, unrootChild, EVE);
    
    return LOCK_OUT(hookBadge);
}

Arrow xl_hook(void* hookp) {
  char hooks[64]; 
  snprintf(hooks, 64, "%p", hookp);
  // Note: hook must be bottom-rooted to be valid
  Arrow hook = xl_root(xl_pair(getHookBadge(), xl_atom(hooks)));
  return hook;
}

void* xl_pointerOf(Arrow a) { // Only bottom-rooted hook is accepted to limit hook forgery
    if (!xl_isPair(a) || xl_tailOf(a) != getHookBadge() || !xl_isRooted(a))
        return NULL;
    
    char* hooks = xl_strOf(xl_headOf(a));
    void* hookp;
    int n = sscanf(hooks, "%p", &hookp);
    if (!n) hookp = NULL;
    free(hooks);
    return hookp;
}

int xl_isEve(Arrow a) {
    return (a == EVE);
}

Arrow xl_isPair(Arrow a) {
    if (a == EVE) {
      return EVE; 
    }

    if (a >= SPACE_SIZE) { // Address anomaly
        return NIL;
    }

    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    LOCK_END();

    if (cell.full.type == CELLTYPE_EMPTY || cell.full.type > CELLTYPE_ARROWLIMIT)
        return NIL;

    return (cell.full.type == CELLTYPE_PAIR ? a : EVE);
}

Arrow xl_isAtom(Arrow a) {
    if (a == EVE) {
      return EVE; 
    }

    if (a >= SPACE_SIZE) { // Address anomaly
        return NIL;
    }

    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    LOCK_END();

    if (cell.full.type == CELLTYPE_EMPTY || cell.full.type > CELLTYPE_ARROWLIMIT)
        return NIL;

    return (cell.full.type == CELLTYPE_PAIR ? EVE : a);
}

enum e_xlType xl_typeOf(Arrow a) {
    if (a == EVE) {
      return XL_EVE; 
    }

    if (a >= SPACE_SIZE) {
      return XL_UNDEF;
    }

    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    LOCK_END();

    if (cell.full.type > CELLTYPE_ARROWLIMIT)
        return NIL;

    static const enum e_xlType map[] = {
      XL_UNDEF, XL_PAIR, XL_ATOM, XL_ATOM, XL_ATOM
    };

    return map[cell.full.type];
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

/** Get children
*
*/
void xl_childrenOfCB(Arrow a, XLCallBack cb, Arrow context) {
    TRACEPRINTF("xl_childrenOf a=%06x", a);

    if (a == EVE) {
        return; // Eve connectivity not traced
    }

    // get the parent cell
    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));

    // check arrow
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT) {
      // invalid ID
      LOCK_END();
      return;
    }


    if (!(cell.arrow.RWWnCn & (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))) {
      // no child
      LOCK_END();
      return;
    }

    // compute hash_children
    uint32_t hChild = hash_children(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    if (cell.arrow.child0) {
      // child0
      cb(cell.arrow.child0, context);

      if ((cell.arrow.RWWnCn & (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK)) == 1) {
        // child0 is the only child
        LOCK_END();
        return;
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
      ONDEBUG((LOGCELL('R', next, &nextCell)));

      if (nextCell.full.type == CELLTYPE_CHILDREN) {
        // One found a children cell

        int j = 0; //< slot index
        while (j < 5) { // slot scanning
          Address inSlot = nextCell.children.C[j];
          if (inSlot == a && (nextCell.children.directions & (1 << (8 + j)))) {
             // terminator found
            LOCK_END();
            return; // no child left

          } else if (inSlot != 0) {
            // child maybe found
            Arrow child = inSlot;

            // get child cell
            Cell childCell;
            mem_get(child, &childCell.u_body);
            ONDEBUG((LOGCELL('R', child, &childCell)));

            // check at least one arrow end is a
            // TODO check direction so to not at a same child twice because its back-reference
            // from its both ends got gathered in the same sequence
            if (childCell.pair.head == a || childCell.pair.tail == a) {
              // child!
              cb(child, context);
            }

          } // child maybe found
          j++;
        } // slot loop
      } // children cell found
    } // child probing loop
}

typedef struct iterator_s {
    Cell     currentCell;
    Arrow    parent;
    Address  pos;
    Arrow    current;
    uint32_t offset;
    int      iSlot;
    int      iCell;
    int      type;
} iterator_t;

static int xl_enumNextChildOf(XLEnum e) {
    iterator_t *iteratorp = e;

    // iterate from current address stored in childrenOf iterator
    Address pos = iteratorp->pos;
    int     i   = iteratorp->iSlot;
    int     ic  = iteratorp->iCell;
    Arrow   a   = iteratorp->parent;
    uint32_t offset = iteratorp->offset;

    LOCK();

    // get current cell
    Cell cell;
    mem_get(pos ? pos : a, &cell.u_body);
    ONDEBUG((LOGCELL('R', pos ? pos : a, &cell)));

    if (pos == EVE || pos == a) { // current cell is parent cell
        
        // check parent arrow is unchanged
        if (iteratorp->currentCell.full.type != cell.full.type
            || iteratorp->currentCell.arrow.hash != cell.arrow.hash
            || iteratorp->currentCell.arrow.dr != cell.arrow.dr
            || memcmp(iteratorp->currentCell.arrow.def,
                 cell.arrow.def, sizeof(cell.arrow.def)) != 0) {
            TRACEPRINTF("arrow changed");
            return LOCK_OUT(0); // arrow changed
        }

        if (!(cell.arrow.RWWnCn &
          (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))) {
          // no child
          TRACEPRINTF("no child");
          return LOCK_OUT(0); // no child
        }

        if (pos == EVE) {
          if (cell.arrow.child0) {
            // return child0, and prepare listing other children 
            iteratorp->pos = a;
            iteratorp->iSlot = 0;
            iteratorp->current = cell.arrow.child0;
            TRACEPRINTF("child 0 %06x", iteratorp->current);
            return LOCK_OUT(!0);
          } else {
            pos = a;
          }
        }
        // pos == a
        if (1 == (cell.arrow.RWWnCn &
                (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))
              && cell.arrow.child0) {
          return LOCK_OUT(0); // no child left
        } else {
          // 1st shift
          ADDRESS_SHIFT(pos, pos, offset);
          mem_get(pos, &cell.u_body);
          ONDEBUG((LOGCELL('R', pos, &cell)));
          ic = 1;
          i = 0;
        }
    } else {

      // check current children cell is unchanged:
      // same child with same flags at same place
      if (iteratorp->currentCell.full.type != cell.full.type
            || iteratorp->currentCell.children.C[i] != cell.children.C[i]
            || (iteratorp->currentCell.children.directions & (1 << (8 + i)))
                 != (cell.children.directions & (1 << (8 + i)))
            || (iteratorp->currentCell.children.directions & (1 << i))
                 != (cell.children.directions & (1 << i))) {
        return LOCK_OUT(0); // unrecoverable change
      }

      // iterate
      i++;
    }

    int iProbe = MAX_CHILDCELLPROBE;
    while (iProbe) { // children cells loop
      if (cell.full.type == CELLTYPE_CHILDREN) {

        while (i < 5) { // cell slots loop
          Address inSlot = cell.children.C[i];
          if (inSlot == a && (cell.children.directions & (1 << (8 + i)))) {
            // terminator found
            iteratorp->currentCell = cell;
            iteratorp->pos     = pos;
            iteratorp->iSlot   = i;
            iteratorp->iCell  = ic;
            iteratorp->current = NIL;
            iteratorp->offset  = offset;
            TRACEPRINTF("terminator found in probed #%d", ic);
            return LOCK_OUT(0); // no child left

          } else if (inSlot != 0) {
            // child maybe found
            Arrow child = inSlot;

            // get child cell
            Cell childCell;
            mem_get(child, &childCell.u_body);
            ONDEBUG((LOGCELL('R', child, &childCell)));

            // check at least one arrow end is a
            if (childCell.pair.head == a || childCell.pair.tail == a) {
              // child!
              iteratorp->currentCell = cell;
              iteratorp->pos = pos;
              iteratorp->iSlot = i;
              iteratorp->iCell = ic;
              iteratorp->current = child;
              iteratorp->offset  = offset;
              TRACEPRINTF("child %d.%d %06x", ic, i, iteratorp->current);

              return LOCK_OUT(!0);
            }
          } // child maybe found
          i++;        
        } // slot loop
        iProbe = MAX_CHILDCELLPROBE;
      } else { // not children cell
        assert(--iProbe);
      }
      // shift
      ADDRESS_SHIFT(pos, pos, offset);
      mem_get(pos, &cell.u_body);
      ONDEBUG((LOGCELL('R', pos, &cell)));
      ic++; // next cell
      i=0; // first slot in next cell
    } // children loop

    // FIXME what happens if terminator is removed while iterating
    assert(0 == "It can't be");
    iteratorp->currentCell = cell;
    iteratorp->pos = pos;
    iteratorp->iSlot = i;
    iteratorp->iCell = ic;
    iteratorp->current = EVE;
    iteratorp->offset  = offset;
    return LOCK_OUT(0);
}

int xl_enumNext(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    return xl_enumNextChildOf(e);
}

Arrow xl_enumGet(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    return iteratorp->current;
}

void xl_freeEnum(XLEnum e) {
    free(e);
}

XLEnum xl_childrenOf(Arrow a) {
    TRACEPRINTF("xl_childrenOf a=%06x", a);

    if (a == EVE) {
        return NULL; // Eve connectivity not traced
    }

    // get parent cell
    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
      || cell.full.type > CELLTYPE_ARROWLIMIT)
      return (LOCK_END(), NULL); // Invalid id
    LOCK_END();
    
    // compute hash_children
    uint32_t hChild = hash_children(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    iterator_t *iteratorp = (iterator_t *) malloc(sizeof (iterator_t));
    assert(iteratorp);
    iteratorp->type = 0; // iterator type = childrenOf
    iteratorp->offset = hChild; // hChild
    iteratorp->parent = a; // parent arrow
    iteratorp->current = EVE; // current child
    iteratorp->pos = EVE; // current cell holding child back-ref
    iteratorp->iSlot = 0; // position of child back-ref in cell : 0..5
    iteratorp->currentCell = cell; // user to detect change
    return iteratorp;
}

Arrow xl_childOf(Arrow a) {
    XLEnum e = xl_childrenOf(a);
    if (!e) return EVE;
    int n = 0;
    Arrow chosen = EVE;
    while (xl_enumNext(e)) {
        Arrow child = xl_enumGet(e);
        n++;
        if (n == 1 || rand() % n == 1) {
            chosen = child;
        }
    }
    return chosen;
}

/** root an arrow */
Arrow xl_root(Arrow a) {
    if (a == EVE) {
        return EVE; // no
    }
    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT) {
      // bad reference
      WARNPRINTF("Not an arrow");
      return LOCK_OUT(EVE);
    }
    
    if (cell.arrow.RWWnCn & FLAGS_ROOTED) {
      // already rooted
      DEBUGPRINTF("%06x already rooted", a);
      return LOCK_OUT(a);
    }
    
    // This arrow was not rooted yet.
    // If it had no child, it was not connected.
    int loose = !(cell.arrow.RWWnCn & FLAGS_CHILDRENMASK);

    // change the arrow to ROOTED state
    cell.arrow.RWWnCn = cell.arrow.RWWnCn | FLAGS_ROOTED;
    mem_set(a, &cell.u_body);
    ONDEBUG((LOGCELL('W', a, &cell)));

    space_stats.root++;

    if (cell.full.type == CELLTYPE_PAIR) { // parent arrow is a pair
        int unconnected = !(cell.arrow.RWWnCn
                            & (FLAGS_CHILDRENMASK
                               | FLAGS_WEAKCHILDRENMASK));
        if (unconnected) { // parent arrow was not connected yet
            // it is not anymore. So one connect it to its own parents
            weaver_connect(cell.pair.tail, a, 0, 1);
            weaver_connect(cell.pair.head, a, 0, 0);
        }
    }
    
    if (loose) {
        // (try to) remove 'a' from the loose log
        weaver_removeLoose(a);
    }
    
    return LOCK_OUT(a);
}

/** unroot a rooted arrow */
Arrow xl_unroot(Arrow a) {
    if (a == EVE)
        return EVE; // no.

    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));

    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT) {
      // bad reference
      WARNPRINTF("Not an arrow");
      return LOCK_OUT(EVE);
    }
    
    if (!(cell.arrow.RWWnCn & FLAGS_ROOTED)) {
      // already unrooted
      DEBUGPRINTF("%06x already unrooted", a);

      return LOCK_OUT(a);
    }

    space_stats.unroot++;

    // change the arrow to UNROOTED state
    cell.arrow.RWWnCn ^= FLAGS_ROOTED;
    mem_set(a, &cell.u_body);
    ONDEBUG((LOGCELL('W', a, &cell)));

    // If this arrow has no strong child left, it's now loose
    int loose = !(cell.arrow.RWWnCn & FLAGS_CHILDRENMASK);
    if (loose) {
        // loose log
        weaver_addLoose(a);

        if (cell.full.type == CELLTYPE_PAIR) { // parent arrow is a pair
            int disconnected = !(cell.arrow.RWWnCn
                                 & (FLAGS_CHILDRENMASK
                                    | FLAGS_WEAKCHILDRENMASK));
            if (disconnected) { // got disconnected
              // one disconnects it from its own parents.
              weaver_disconnect(cell.pair.tail, a, 0, 1);
              weaver_disconnect(cell.pair.head, a, 0, 0);
            }
        }
    }
    return LOCK_OUT(a);
}

/** return the root status */
int xl_isRooted(Arrow a) {
    if (a == EVE) return EVE;

    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    LOCK_END();
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return EVE;
    else if (cell.arrow.RWWnCn & FLAGS_ROOTED)
      return a;
    else
      return EVE;
}

/** check if an arrow is loose */
int xl_isLoose(Arrow a) {
    if (a == EVE) return EVE;

    LOCK();
    Cell cell;
    mem_get(a, &cell.u_body);
    ONDEBUG((LOGCELL('R', a, &cell)));
    LOCK_END();
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return 0;

    if (cell.arrow.RWWnCn & FLAGS_ROOTED)
      return 0;

    if (cell.arrow.RWWnCn & FLAGS_CHILDRENMASK)
      return 0;

    return 1;
}

int xl_equal(Arrow a, Arrow b) {
    return (a == b);
}




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

void xl_begin() {
    LOCK();
    ENTER_TRANSACTION();
    mem_open();
    LOCK_END();
}

/** type an arrow as a "state" arrow.
 *  TODO: is it necessary?
 */
Arrow xl_state(Arrow a) {
    static Arrow systemStateBadge = EVE;
    if (systemStateBadge == EVE) { // to avoid locking in most cases
            LOCK();
            if (systemStateBadge == EVE) {
                systemStateBadge = xl_atom("XLsTAT3");
                xl_root(systemStateBadge);

                // prevent previous session saved states to survive the reboot.
                xl_childrenOfCB(systemStateBadge, unrootChild, EVE);
            }
            LOCK_END();
    }
    return xl_pair(systemStateBadge, a);
}

void weaver_performGC() {
    TRACEPRINTF("BEGIN weaver_performGC()");

    for (unsigned i = looseLogSize; i > 0; i--) { // loose stack scanning
        Arrow a = looseLog[i - 1];

        if (xl_isLoose(a)) { // a loose arrow is removed NOW
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

    spaceGCDone = 1; // space GC occured
    
    if (mem_yield() == 1) // also mem commmit occured
        memCommitDone = 1;
}

void xl_over() {
    LOCK();
    spaceGCDone = 0; //< record the need to GC stuff before thread syncing
    memCloseDone = 0; //< record the need to close mem before thread syncing
    LEAVE_TRANSACTION();
    do {
        WAIT_DORMANCY();
    } while (transactionCount > 0); // spurious wakeup check
    
    if (!spaceGCDone) { // no other thread has GC yet
        weaver_performGC();
    }
    
    if (!memCloseDone && memCommitDone) { // only if not done yet and no commit pending
      INFOPRINTF("xl_over closes mem");
      mem_close();
      memCloseDone = 1;
   }
    
    LOCK_END();
}

void xl_commit() {
    TRACEPRINTF("xl_commit (looseLogSize = %d)", looseLogSize);
    
    LOCK();
    spaceGCDone = 0; //< record the need to GC stuff before thread syncing
    memCommitDone = 0; //< record the need to commit stuff before threads syncing
    LEAVE_TRANSACTION();
    do {
        WAIT_DORMANCY();
    } while (transactionCount > 0); // spurious wakeup check
    
    if (!spaceGCDone) {
        weaver_performGC();
    }
    
    if (!memCommitDone) { // if no thread has commited yet
        mem_commit();
        memCommitDone = 1;
        memCloseDone = 1; // < prevents xl_over() threads to close mem because there is at least one commit thread (this one)  
    }

    ENTER_TRANSACTION();
    LOCK_END();
    
}

/** xl_yield */
void xl_yield(Arrow a) {
    return; // FIXME je désactive yield tant que je n'ai pas trouvé une façon correcte de préserver des flèches manipulées en dehors de xl_run() 
    Arrow stateArrow = EVE;
    TRACEPRINTF("xl_yield (looseLogSize = %d)", looseLogSize);
    if (a != EVE) {
        stateArrow = xl_state(a);
        xl_root(stateArrow);
    }
    xl_over();
    xl_begin();
    if (stateArrow != EVE) {
        xl_unroot(stateArrow);
    }
}

/** printf extension for arrow (%O specifier) */
static int printf_arrow_extension(FILE *stream,
        const struct printf_info *info,
        const void *const *args) {
    static const char* nilFakeURI = "(NIL)";
    Arrow arrow = *((Arrow*) (args[0]));
    char* uri = arrow != NIL ? xl_uriOf(arrow, NULL) : (char *)nilFakeURI;
    if (uri == NULL)
        uri = (char *)nilFakeURI;
    int len = fprintf(stream, "%*s", (info->left ? -info->width : info->width), uri);
    if (uri != nilFakeURI)
        free(uri);
    return len;
}

/** printf extension "arginfo" */
static int printf_arrow_arginfo_size(const struct printf_info *info, size_t n,
        int *argtypes, int *size) {
    /* We always take exactly one argument and this is a pointer to the
       structure.. */
    if (n > 0)
        argtypes[0] = PA_INT;
    return 1;
}

/** initialize the Entrelacs system */
int xl_init() {
    static int xl_init_done = 0;
    if (xl_init_done) return 0;
    xl_init_done = 1;
    
    assert(sizeof(CellBody) == sizeof(Cell));

    pthread_cond_init(&apiNowActive, NULL);
    pthread_cond_init(&apiNowDormant, NULL);

    pthread_mutexattr_init(&apiMutexAttr);
    pthread_mutexattr_settype(&apiMutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&apiMutex, &apiMutexAttr);

    int rc = mem_init();
    if (rc < 0) { // problem
       return rc;
    }


    // register a printf extension for arrow (glibc only!)
    register_printf_specifier('O', printf_arrow_extension, printf_arrow_arginfo_size);
    
    if (rc) { // very first start
        // Eve

        rc = mem_open();
        if (rc < 0) { // problem
           return rc;
        }
        
        Cell EveCell;
        EveCell.pair.tail = EVE;
        EveCell.pair.head = EVE;
        EveCell.arrow.hash = EVE_HASH;
        EveCell.arrow.RWWnCn = FLAGS_ROOTED;
        EveCell.arrow.child0 = 0;
        EveCell.full.type = CELLTYPE_PAIR;
        EveCell.arrow.dr = 0;
        mem_set(EVE, &EveCell.u_body);
        ONDEBUG((LOGCELL('W', EVE, &EveCell)));
        mem_commit();
        mem_close();
    }
    
    
    geoalloc((char**) &looseLog, &looseLogMax, &looseLogSize, sizeof(Arrow), 0);
    return rc;
}

void xl_destroy() {
    // TODO complete
    free(looseLog);
    
    pthread_cond_destroy(&apiNowActive);
    pthread_cond_destroy(&apiNowDormant);
    pthread_mutexattr_destroy(&apiMutexAttr);
    pthread_mutex_destroy(&apiMutex);

    mem_destroy();
}

int space_unitTest() {
    Address a = 0x7f3f3a;
    Address offset = 0x7a5dc6;
    Address result;
    int many = 100;
    for (int i = 0; i < many; i++) {
      ADDRESS_SHIFT(a, a, offset);
    }
    result = a;
    a = 0x7f3f3a;
    offset = 0x7a5dc6;
    ADDRESS_JUMP(a, a, offset, many);
    assert(result == a);
    return 0;
}