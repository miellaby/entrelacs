#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
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

struct s_space_stats space_stats_zero = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, space_stats = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/** Things to make the API parallelizable
 */
static pthread_mutexattr_t apiMutexAttr;
static pthread_mutex_t apiMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() pthread_mutex_lock(&apiMutex)
#define UNLOCK() pthread_mutex_unlock(&apiMutex)
#define LOCK_OUT(X) (UNLOCK(), (Address)X)
#define LOCK_OUT64(X) (UNLOCK(), (uint64_t))
#define LOCK_OUTSTR(X) (UNLOCK(), (char *)X)
#define LOCK_OUTRAW(X) (UNLOCK(), (uint8_t *)X)

static pthread_cond_t apiNowDormant;  // Signaled when all thread are ready to commit
static pthread_cond_t apiNowActive;   // Signaled once a thread has finished commit
static int transactionCount = 0;      // Active Transaction counter; Incremented/Decremented by xl_open/xl_close. Decremented when waiting for commit. Incremented again after.
static int spaceGCDone = 1;           // 1->0 when a thread starts waiting in xl_commit()/xl_close(), 0->1 when all threads synced and GC is done
static int memCommitDone = 1;         // 1->0 when a thread starts waiting in xl_commit(), 0->1 when all threads synced and mem_commit(); occurs only if !memCommitDone
static int memCloseDone = 1;          // 1->0 when a xl_close thread starts waiting, 0->1 when all threads synced and a xl_commit() thread does commit (without actually mem_close)

#define ENTER_TRANSACTION() (!transactionCount++ ? (void)pthread_cond_signal(&apiNowActive) : (void)0)
#define LEAVE_TRANSACTION() (transactionCount ? (--transactionCount ? (void)0 : (void)pthread_cond_signal(&apiNowDormant)) : (void)0)
#define WAIT_DORMANCY() (transactionCount > 0 ? (void)pthread_cond_wait(&apiNowDormant, &apiMutex) : (void)0)

/*
 * Eve
 */
const Address Eve = EVE;

/** return Eve */
Address xl_Eve() {
    return EVE;
}

uint32_t xl_hashOf(Address a) {
    if (a == EVE)
      return hash_eve(); // Eve hash is not zero!

    if (a >= SPACE_SIZE)
      return 0; // Out of space

    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell))
        return 0; // Not an arrow

    uint32_t cs = cell.arrow.hash; // hash
    return LOCK_OUT(cs);
}

Address xl_pair(Address tail, Address head) {
    LOCK();
    Address a = assimilate_pair(tail, head, 0);
    return LOCK_OUT(a);
}

Address xl_pairMaybe(Address tail, Address head) {
    LOCK();
    Address a = assimilate_pair(tail, head, 1);
    return LOCK_OUT(a);
}

Address xl_atom(const char *str) {
    uint32_t size = strlen(str);
    LOCK();
    Address a = (size == 0 ?  // EVE has a zero payload
                     EVE
                           : (size < TAG_MINSIZE ? assimilate_small(size, (uint8_t *)str, 0)
                                                 : (size >= BLOB_MINSIZE ? assimilate_blob(size, (uint8_t *)str, 0) : assimilate_tag(size, (uint8_t *)str, 0))));
    return LOCK_OUT(a);
}

Address xl_atomMaybe(const char *str) {
    uint32_t size = strlen(str);
    LOCK();
    Address a = (size == 0 ? EVE  // EVE 0 payload
                           : (size < TAG_MINSIZE ? assimilate_small(size, (uint8_t *)str, 1)
                                                 : (size >= BLOB_MINSIZE ? assimilate_blob(size, (uint8_t *)str, 1) : assimilate_tag(size, (uint8_t *)str, 1))));
    return LOCK_OUT(a);
}

Address xl_atomn(const uint32_t size, const uint8_t *mem) {
    Address a;
    LOCK();
    if (size == 0)
        a = EVE;
    else if (size < TAG_MINSIZE) {
        a = assimilate_small(size, mem, 0);
    } else if (size >= BLOB_MINSIZE) {
        a = assimilate_blob(size, mem, 0);
    } else {
        a = assimilate_tag(size, mem, 0);
    }
    return LOCK_OUT(a);
}

Address xl_atomnMaybe(const uint32_t size, const uint8_t *mem) {
    Address a;
    LOCK();
    if (size == 0)
        a = EVE;
    else if (size < TAG_MINSIZE) {
        a = assimilate_small(size, mem, 1);
    } else if (size >= BLOB_MINSIZE)
        a = assimilate_blob(size, mem, 1);
    else {
        a = assimilate_tag(size, mem, 1);
    }
    return LOCK_OUT(a);
}


Address xl_headOf(Address a) {
    if (a == EVE)
        return EVE;
    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {
        return LOCK_OUT(XL_NIL);
    } else if (!cell_isPair(&cell)) {
        return LOCK_OUT(a); // atom parents are itself
    } else {
        return LOCK_OUT(cell_getHead(&cell));
    }
}

Address xl_tailOf(Address a) {
    if (a == EVE)
        return EVE;
    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {
        return LOCK_OUT(XL_NIL);
    } else if (!cell_isPair(&cell)) {
        return LOCK_OUT(a); // atom parents are itself
    } else {
        return LOCK_OUT(cell_getTail(&cell));
    }
}

/** return the content behind an atom
*/
uint8_t *xl_memOf(Address a, uint32_t *lengthP) {
    if (a == EVE) {  // Eve has no payload
        *lengthP = 0;
        return NULL;
    }

    // Lock mem access
    LOCK();

    // get the cell pointed by a
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {  // wrong id
        return LOCK_OUTRAW(NULL);
    }
    if (!cell_isAtom(&cell)) {  // wrong arrow type
        return LOCK_OUTRAW(NULL);
    }
    uint32_t payloadLength;
    uint8_t *payload = cell_getPayload(&cell, a, &payloadLength);
    if (payload == NULL) {  // Error
        return LOCK_OUTRAW(NULL);
    }

    if (cell.full.type == CELLTYPE_BLOB) {
        // The payload is a BLOB footprint
        char *footprint = (char *)payload;
        size_t blobLength;
        uint8_t *blobData = mem0_loadData(footprint, &blobLength);
        if (blobLength > UINT32_MAX)
            blobLength = UINT32_MAX;
        *lengthP = blobLength;
        free(payload);  // one doesn't return the payload, free...
        // *lengthP = ... set by mem0_loadData call
        return LOCK_OUTRAW(blobData);
    }

    // else: Tag or small case
    *lengthP = payloadLength;
    return LOCK_OUTRAW(payload);
}

int xl_read(Address a, XLType *type_p, uint32_t *hash_p, Address *tail_p, Address *head_p, uint8_t **raw_p, uint32_t *size_p) {
    if (a >= SPACE_SIZE) {
        return 1;
    }
    if (a == EVE) {  // Eve has an empty payload
        *type_p = EVE;
        *tail_p = EVE;
        *head_p = EVE;
        *hash_p = hash_eve();
        *size_p = 0;
        *raw_p = NULL;
        return 0;
    }

    // Lock mem access
    LOCK();

    // get the cell pointed by a
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {  // wrong id
        UNLOCK();
        return XL_NIL;
    }
    *hash_p = cell.arrow.hash;
    if (cell_isPair(&cell)) {
        *type_p = XL_PAIR;
        *tail_p = cell.pair.tail;
        *head_p = cell.pair.head;
        *raw_p = NULL;
        *size_p = 0;
    } else {
        *type_p = XL_ATOM;
        *tail_p = a;
        *head_p = a;
        *raw_p = xl_memOf(a, size_p);
    }
    UNLOCK();
    return 0;
}

char *xl_strOf(Address a) {
    uint32_t lengthP;
    uint8_t *p = xl_memOf(a, &lengthP);
    return (char *)p;
}

char *xl_digestOf(Address a, uint32_t *l) {
    TRACEPRINTF("BEGIN xl_digestOf(%06x)", a);

    if (a >= SPACE_SIZE) {  // Address anomaly
        return NULL;
    }

    LOCK();

    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {  // wrong id
        return LOCK_OUTSTR(NULL);
    }
    char *digest = serial_digest(a, &cell, l);
    return LOCK_OUTSTR(digest);
}

char *xl_uriOf(Address a, uint32_t *l) {
    LOCK();
    char *str = serial_toURI(a, l);
    return LOCK_OUTSTR(str);
}

Address xl_digestMaybe(char *digest) {
    TRACEPRINTF("BEGIN xl_digestMaybe(%.*s)", DIGEST_SIZE, digest);
    LOCK();
    return LOCK_OUT(probe_digest(digest));
}

Address xl_uri(char *aUri) {
    LOCK();
    Address a = serial_parseURIs(NAN, aUri, 0);
    return LOCK_OUT(a);
}

Address xl_uriMaybe(char *aUri) {
    LOCK();
    Address a = serial_parseURIs(NAN, aUri, 1);
    return LOCK_OUT(a);
}

Address xl_urin(uint32_t aSize, char *aUri) {
    LOCK();
    Address a = serial_parseURIs(aSize, aUri, 0);
    return LOCK_OUT(a);
}

Address xl_urinMaybe(uint32_t aSize, char *aUri) {
    LOCK();
    Address a = serial_parseURIs(aSize, aUri, 1);
    return LOCK_OUT(a);
}

static void generate_random(char *buffer) {
    // FIXME actual randomness
    char random[80];
    memset(random, 0, 80);
    snprintf(random, sizeof(random), "an0nymous:)%lx", (long)rand() ^ (long)time(NULL));
    hash_crypto(sizeof(random), (uint8_t *)random,
                buffer);  // Access to unitialized data is wanted
}

Address xl_anonymous() {
    char random[CRYPTO_SIZE + 1];
    Address a = XL_NIL;
    do {  // select a random key, check the atom doesn't exit
        generate_random(random);
        a = xl_atomMaybe(random);
        assert(a != XL_NIL);
    } while (a);
    return xl_atom(random);
}

int xl_isEve(Address a) {
    return (a == EVE);
}

Address xl_isPair(Address a) {
    if (a == EVE) {
        return EVE;
    }

    if (a >= SPACE_SIZE) {  // Address anomaly
        return XL_NIL;
    }

    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    UNLOCK();
    if (!cell_isArrow(&cell)) {  // wrong id
        return XL_NIL;
    }
    return (cell_isPair(&cell) ? a : EVE);
}

Address xl_isAtom(Address a) {
    if (a == EVE) {
        return EVE;
    }

    if (a >= SPACE_SIZE) {  // Address anomaly
        return XL_NIL;
    }

    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    UNLOCK();
    if (!cell_isArrow(&cell)) {  // wrong id
        return XL_NIL;
    }
    return (cell_isAtom(&cell) ? a : EVE);
}

enum e_xlType xl_typeOf(Address a) {
    static const enum e_xlType map[] = { XL_UNDEF, XL_PAIR, XL_ATOM, XL_ATOM, XL_ATOM };

    if (a == EVE) {
        return EVE;
    }

    if (a >= SPACE_SIZE) {
        return XL_UNDEF;
    }

    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    UNLOCK();
    if (!cell_isArrow(&cell)) {  // wrong id
        return XL_UNDEF;
    }
    return map[cell.full.type];
}

/** Get children
*
*/
void xl_childrenOfCB(Address a, XLCallBack cb, void* context) {
    TRACEPRINTF("xl_childrenOfCB a=%06x", a);

    if (a == EVE) {
        return;  // Eve connectivity not traced
    }

    // get the parent cell
    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    uint32_t childCount = cell_getChildCount(&cell);
    if (childCount == (uint32_t)-1) {
        ERRORPRINTF("Not an arrow");
        UNLOCK();
        return;
    }
    if (childCount == 0) {
        TRACEPRINTF("No child");
        UNLOCK();
        return;
    }

    // compute hash_children
    uint32_t hChild = hash_children(&cell);

    if (cell.arrow.child0 != EVE) {
        cb(cell.arrow.child0, context);

        if (childCount == 1) {
            // child0 is the only child
            UNLOCK();
            return;
        }
    }

    // probe children
    Address next = a;
    Cell nextCell;

    while (1) {  // child probing loop
        ADDRESS_SHIFT(next, next, hChild);
        CELL_READ(&nextCell, next);
        if (nextCell.full.type == CELLTYPE_CHILDREN) {
            // found a children cell
            int j = 0;       //< slot index
            while (j < 5) {  // slot scanning
                Address inSlot = nextCell.children.C[j];
                if (inSlot == a && (nextCell.children.directions & (1 << (8 + j)))) {
                    // terminator found
                    UNLOCK();
                    return;  // no child left

                } else if (inSlot != 0) {
                    // child maybe found
                    Address child = inSlot;

                    // get child cell
                    Cell childCell;
                    CELL_READ(&childCell, child);

                    // check at least one arrow end is a
                    // TODO check direction so to not at a same child twice because its back-reference
                    // from its both ends got gathered in the same sequence
                    if (childCell.pair.head == a || childCell.pair.tail == a) {
                        // child!
                        cb(child, context);
                    }

                }  // child maybe found
                j++;
            }  // slot loop
        }  // children cell found
    }  // child probing loop
}

typedef struct iterator_s {
    Cell currentCell;
    Address parent;
    Address pos;
    Address current;
    uint32_t offset;
    int iSlot;
    int iCell;
    int type;
} iterator_t;

static int xl_enumNextChildOf(XLEnum e) {
    iterator_t *iteratorp = e;

    // iterate from current address stored in childrenOf iterator
    Address pos = iteratorp->pos;
    int i = iteratorp->iSlot;
    int ic = iteratorp->iCell;
    Address a = iteratorp->parent;
    uint32_t offset = iteratorp->offset;

    LOCK();

    // get current cell
    Cell cell;
    CELL_READ(&cell, pos ? pos : a);

    if (pos == EVE || pos == a) {  // current cell is parent cell

        // check parent arrow is unchanged
        if (iteratorp->currentCell.full.type != cell.full.type || iteratorp->currentCell.arrow.hash != cell.arrow.hash || iteratorp->currentCell.arrow.dr != cell.arrow.dr ||
            memcmp(iteratorp->currentCell.arrow.def, cell.arrow.def, sizeof(cell.arrow.def)) != 0) {
            TRACEPRINTF("arrow changed");
            return LOCK_OUT(0);  // arrow changed
        }
        uint32_t childCount = cell_getChildCount(&cell);
        if (childCount == (uint32_t)-1) {
            ERRORPRINTF("Not an arrow");
            return LOCK_OUT(0);
        }
        if (childCount == 0) {
            TRACEPRINTF("no child");
            return LOCK_OUT(0);  // no child
        }

        if (pos == EVE) {
            if (cell.arrow.child0 != EVE) {
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
        if (1 == childCount && cell.arrow.child0 != EVE) {
            return LOCK_OUT(0);  // no child left
        } else {
            // 1st shift
            ADDRESS_SHIFT(pos, pos, offset);
            CELL_READ(&cell, pos);
            ic = 1;
            i = 0;
        }
    } else {
        // check current children cell is unchanged:
        // same child with same flags at same place
        if (iteratorp->currentCell.full.type != cell.full.type || iteratorp->currentCell.children.C[i] != cell.children.C[i] ||
            (iteratorp->currentCell.children.directions & (1 << (8 + i))) != (cell.children.directions & (1 << (8 + i))) ||
            (iteratorp->currentCell.children.directions & (1 << i)) != (cell.children.directions & (1 << i))) {
            return LOCK_OUT(0);  // unrecoverable change
        }

        // iterate
        i++;
    }

    int iProbe = MAX_CHILDCELLPROBE;
    while (iProbe) {  // children cells loop
        if (cell.full.type == CELLTYPE_CHILDREN) {
            while (i < 5) {  // cell slots loop
                Address inSlot = cell.children.C[i];
                if (inSlot == a && (cell.children.directions & (1 << (8 + i)))) {
                    // terminator found
                    iteratorp->currentCell = cell;
                    iteratorp->pos = pos;
                    iteratorp->iSlot = i;
                    iteratorp->iCell = ic;
                    iteratorp->current = XL_NIL;
                    iteratorp->offset = offset;
                    TRACEPRINTF("terminator found in probed #%d", ic);
                    return LOCK_OUT(0);  // no child left

                } else if (inSlot != 0) {
                    // child maybe found
                    Address child = inSlot;

                    // get child cell
                    Cell childCell;
                    CELL_READ(&childCell, child);

                    // check at least one arrow end is a
                    if (childCell.pair.head == a || childCell.pair.tail == a) {
                        // child!
                        iteratorp->currentCell = cell;
                        iteratorp->pos = pos;
                        iteratorp->iSlot = i;
                        iteratorp->iCell = ic;
                        iteratorp->current = child;
                        iteratorp->offset = offset;
                        TRACEPRINTF("child %d.%d %06x", ic, i, iteratorp->current);

                        return LOCK_OUT(!0);
                    }
                }  // child maybe found
                i++;
            }  // slot loop
            iProbe = MAX_CHILDCELLPROBE;
        } else {  // not children cell
            assert(--iProbe);
        }
        // shift
        ADDRESS_SHIFT(pos, pos, offset);
        CELL_READ(&cell, pos);
        ic++;   // next cell
        i = 0;  // first slot in next cell
    }  // children loop

    // FIXME what happens if terminator is removed while iterating
    assert(0 == "It can't be");
    iteratorp->currentCell = cell;
    iteratorp->pos = pos;
    iteratorp->iSlot = i;
    iteratorp->iCell = ic;
    iteratorp->current = EVE;
    iteratorp->offset = offset;
    return LOCK_OUT(0);
}

int xl_enumNext(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    return xl_enumNextChildOf(e);
}

Address xl_enumGet(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    return iteratorp->current;
}

void xl_enumFree(XLEnum e) {
    assert(e);
    free(e);
}

XLEnum xl_childrenOf(Address a) {
    TRACEPRINTF("xl_childrenOf a=%06x", a);

    if (a == EVE) {
        ERRORPRINTF("xl_childrenOf Eve");
        return NULL;  // Eve connectivity not traced
    }

    // get parent cell
    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    UNLOCK();
    if (!cell_isArrow(&cell)) {
        return NULL;  // Invalid id
    }

    // compute hash_children
    uint32_t hChild = hash_children(&cell);

    iterator_t *iteratorp = (iterator_t *)malloc(sizeof(iterator_t));
    assert(iteratorp);
    iteratorp->type = 0;            // iterator type = childrenOf
    iteratorp->offset = hChild;     // hChild
    iteratorp->parent = a;          // parent arrow
    iteratorp->current = EVE;       // current child
    iteratorp->pos = EVE;           // current cell holding child back-ref
    iteratorp->iSlot = 0;           // position of child back-ref in cell : 0..5
    iteratorp->currentCell = cell;  // user to detect change
    return iteratorp;
}

/** root an arrow */
Address xl_root(Address a) {
    if (a == EVE) {
        return EVE;  // no
    }
    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {
        WARNPRINTF("%06x not an arrow", a);
        return LOCK_OUT(EVE);
    }
    if (cell_isRooted(&cell)) {
        DEBUGPRINTF("%06x already rooted", a);
        return LOCK_OUT(a);
    }

    space_stats.root++;

    // This arrow was not rooted yet.
    // If it had no child, it was not connected.
    int loose = (cell_getChildCount(&cell) == 0);

    // change the arrow to ROOTED state
    cell.arrow.RC0dCn |= FLAGS_ROOTED;
    CELL_WRITE(&cell, a);

    if (loose) { // arrow was loose (neith rooted not connected)
        // (try to) remove 'a' from the loose log
        weaver_removeLoose(a);
        if (cell_isPair(&cell)) { // arrow is a pair
            // it is not anymore. So one connect it to its own parents
            weaver_connect(cell.pair.tail, a, 1);
            weaver_connect(cell.pair.head, a, 0);
        }
    }
    return LOCK_OUT(a);
}

/** unroot a rooted arrow */
Address xl_unroot(Address a) {
    if (a == EVE)
        return EVE;  // no.

    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {
        WARNPRINTF("%06x not an arrow", a);
        return LOCK_OUT(EVE);
    }
    if (!cell_isRooted(&cell)) {
        DEBUGPRINTF("%06x already unrooted", a);
        return LOCK_OUT(a);
    }

    space_stats.unroot++;

    // change the arrow to UNROOTED state
    cell.arrow.RC0dCn ^= FLAGS_ROOTED;
    CELL_WRITE(&cell, a);

    int loose = (cell_getChildCount(&cell) == 0);
    if (loose) { // this arrow has no child
        // add arrow to the loose log
        weaver_addLoose(a);
        if (cell_isPair(&cell)) { // arrow is a pair
            // one disconnects it from its own parents.
            weaver_disconnect(cell.pair.tail, a, 1);
            weaver_disconnect(cell.pair.head, a, 0);
        }
    }
    return LOCK_OUT(a);
}

/** return the root status */
int xl_isRooted(Address a) {
    if (a == EVE)
        return EVE;
    LOCK();
    Cell cell;
    CELL_READ(&cell, a);
    if (!cell_isArrow(&cell)) {
        WARNPRINTF("%06x not an arrow", a);
        return LOCK_OUT(EVE);
    }
    return LOCK_OUT(cell_isRooted(&cell) ? a : EVE);
}

int xl_equal(Address a, Address b) {
    return (a == b);
}

void xl_open() {
    LOCK();
    ENTER_TRANSACTION();
    mem_open();
    UNLOCK();
}

void xl_close() {
    TRACEPRINTF("xl_close");
    LOCK();
    // before threads rendez-vous ...
    spaceGCDone = 0;    //< record the need to GC
    memCommitDone = 0;  //< record the need to commit
    memCloseDone = 0;   //< record the need to close mem

    LEAVE_TRANSACTION();
    do {
        UNLOCK();
        WAIT_DORMANCY();
        LOCK();
    } while (transactionCount > 0);  // spurious wakeup check

    if (!spaceGCDone) {
        weaver_performGC();
        spaceGCDone = 1;
        memCommitDone = 0;
    }

    if (!memCommitDone) {
        mem_commit();
        memCommitDone = 1;
    }

    if (!memCloseDone) {  // only if not done yet and no commit pending
        memCloseDone = 1;
        INFOPRINTF("xl_close closes mem");
        mem_close();
    }

    UNLOCK();
}

void xl_commit() {
    TRACEPRINTF("xl_commit (looseLogSize = %d)", looseLogSize);

    LOCK();
    //  before threads rendez-vous  ...
    spaceGCDone = 0;    //< record the need to GC
    memCommitDone = 0;  //< record the need to commit

    /// temporarily leave the transaction to allow other all threads to rendez-vous
    LEAVE_TRANSACTION();
    do {
        UNLOCK();
        WAIT_DORMANCY();
        LOCK();
    } while (transactionCount > 0);  // spurious wakeup check

    // Commiting doesn't leave a transaction, so we must enter one.
    ENTER_TRANSACTION();

    if (!spaceGCDone) {
        weaver_performGC();
        spaceGCDone = 1;
        memCommitDone = 0;
    }

    if (!memCommitDone) {
        mem_commit();
        memCommitDone = 1;
    }

    UNLOCK();
}

/** xl_yield */
void xl_yield(Address a) {
    (void)a;
    return;  // FIXME je désactive yield tant que je n'ai pas trouvé une façon correcte de préserver des flèches manipulées en dehors de xs_run()
}

/** initialize the Entrelacs system */
int xl_init() {
    static int xl_init_done = 0;
    if (xl_init_done)
        return 0;
    xl_init_done = 1;

    assert(sizeof(CellBody) == sizeof(Cell));

    pthread_cond_init(&apiNowActive, NULL);
    pthread_cond_init(&apiNowDormant, NULL);

    pthread_mutexattr_init(&apiMutexAttr);
    pthread_mutexattr_settype(&apiMutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&apiMutex, &apiMutexAttr);

    int rc = weaver_init();
    if (rc < 0) {  // problem
        return rc;
    }

    rc = mem_init();
    if (rc < 0) {  // problem
        return rc;
    }

    if (rc) {  // very first start
        INFOPRINTF("Very first start");

        rc = mem_open();
        if (rc < 0) {  // problem
            return rc;
        }

        Cell EveCell;
        EveCell.pair.tail = EVE;
        EveCell.pair.head = EVE;
        EveCell.arrow.hash = hash_eve();
        EveCell.arrow.RC0dCn = FLAGS_ROOTED;
        EveCell.arrow.child0 = EVE;
        EveCell.full.type = CELLTYPE_PAIR;
        EveCell.arrow.dr = 0;
        CELL_WRITE(&EveCell, EVE);
        mem_commit();
        mem_close();
    }

    return rc;
}

void xl_destroy() {
    weaver_destroy();
    // TODO complete

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