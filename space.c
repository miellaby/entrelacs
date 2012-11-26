#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <printf.h>
#include <ctype.h>
#include <pthread.h>
#include "entrelacs/entrelacs.h"
#include "mem0.h"
#include "mem.h"
#include "sha1.h"
#define LOG_CURRENT LOG_SPACE
#include "log.h"


static struct s_space_stats {
  int get;
  int root;
  int unroot;
  int new;
  int found;
  int connect;
  int disconnect;
  int atom;
  int pair;
  int forget;
} space_stats_zero = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}, space_stats = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static pthread_mutexattr_t api_mutex_attr;
static pthread_mutex_t api_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() pthread_mutex_lock(&api_mutex)
#define LOCK_END() pthread_mutex_unlock(&api_mutex)
#define LOCK_OUT(X) (pthread_mutex_unlock(&api_mutex), X)


static pthread_cond_t api_now_dormant;   // Signaled when all thread are ready to commit
static pthread_cond_t api_now_active;    // Signaled once a thread has finished commit
static int api_activity = 0; // Activity counter; Incremented/Decremented by xl_begin/xl_over. Decremented when waiting for commit. Incremented again after.
#define ACTIVITY_BEGIN() (LOCK(), (!api_activity++ ? (void)pthread_cond_signal(&api_now_active) : (void)0), LOCK_END())
#define ACTIVITY_OVER() (LOCK(), (api_activity ? (--api_activity ? (void)0 : (void)pthread_cond_signal(&api_now_dormant)) : (void)0), LOCK_END())
#define WAIT_DORMANCY() (LOCK(), (api_activity > 0 ? (void)pthread_cond_wait(&api_now_dormant, &api_mutex) : (void)0))

/*
 * Size limit where data is stored as "blob" rather than "tag"
 */
#define BLOB_MINSIZE 100

/*
 * Eve
 */
#define EVE (0)
const Arrow Eve = EVE;
/*
 * Cell category bits
 */
#define CATBITS_MASK   0x0700000000000000LLU
#define CATBITS_EMPTY  0x0000000000000000LLU
#define CATBITS_PAIR   0x0100000000000000LLU
#define CATBITS_SMALL  0x0200000000000000LLU
#define CATBITS_TAG    0x0300000000000000LLU
#define CATBITS_BLOB   0x0400000000000000LLU
#define CATBITS_CHAIN  0x0500000000000000LLU
#define CATBITS_LAST   0x0600000000000000LLU
#define CATBITS_SYNC   0x0700000000000000LLU

/*
 * Reattachment cell type
 */
#define REATTACHMENT_FIRST  0
#define REATTACHMENT_NEXT   1
#define REATTACHMENT_CHILD0 2

/*
 * ROOTBIT
 */
#define ROOTBIT_ROOTED 0x80
#define ROOTBIT_UNROOTED 0

/*
 *   Memory device
 *
 *   -------+------+-------+------+----------
 *      ... | cell |  cell | cell | ....
 *   -------+------+-------+------+----------
 *
 *
 *   Cell structure (64 bits)
 *
 *   +---+----+----------------------------------------+
 *   | P |  T |                 data                   |
 *   +---+----+----------------------------------------+
 *
 *       P: "Peeble" count aka "More" counter (5 bits)
 *       T: Data type ID (3 bits)
 *    Data: Data (56 bits)
 *
 *   Data structure by type (56 bits)
 *
 *   T = 1: pair
 *   +---------------+--------------+---+---+
 *   |     tail@     |     head@    | R | C |
 *   +---------------+--------------+---+---+
 *
 *   T = 3 : tag
 *   T = 4 : blob signature
 *   +----------------------+-------+---+---+
 *   |     checksum         |   J   | R | C |
 *   +----------------------+-------+---+---+
 *
 *   T = 2 : small
 *   +------------------------------+---+---+
 *   |           small              | R | C |
 *   +------------------------------+---+---+
 *
 *   T = 5 : slice
 *
 *   slice of tuple is
 *   +---------------+--------------+-------+
 *   |     end@      |     end@     |   J   |
 *   +---------------+--------------+-------+
 *
 *   slice of binary chain (tag/blob) is
 *   +------------------------------+-------+
 *   |          data                |   J   |
 *   +------------------------------+-------+
 *
 *   slice of children list
 *   +---------------+--------------+-------+
 *   |     child@    |   child@     |   J   |
 *   +---------------+--------------+-------+
 *
 *   T = 6 : last slice
 *
 *   last slice of tuple is
 *   +---------------+--------------+-------+
 *   |     end@      |     end@     |   S   |
 *   +---------------+--------------+-------+
 *
 *   last slice of binary chain (tag/blob) is
 *   +------------------------------+-------+
 *   |          data                |   S   |
 *   +------------------------------+-------+
 *
 *   last slice of children list
 *   +---------------+--------------+-------+
 *   |     child@    |   child@     |  S=6  |
 *   +---------------+--------------+-------+
 *
 *        R: Root flag (1 bit)
 *        C: first child jump, h-child multiplier (7 bits)
 *        J: next slice jump, h-sequence multiplier (1 byte)
 *        @: Cell address (3 bytes)
 *     data: data or slice of data (6 bytes)
 * checksum: definition checksum (40 bits / 5 bytes)
 *       S : Actual data size in bytes of the last slice (1 byte)
 *            (FIXME: always bytes for consistency )
 *
 *   T = 7 : sequence reattachment
 *   +---------------+--------------+-------+
 *   |    from@      |    next@     | Type  |
 *   +---------------+--------------+-------+
 *
 *   Type: reattached chain type
 *        - Type = 0 : Jump chain reattachment
 *        - Type = 1 : Child0 chain reattachment
 */

#define get_moreBits(CELL) ((CELL) & 0xF800000000000000LLU)


// This remaining part is useless as attributs are always zero when building: | ((rooted) << 7) | (child0)

#define pair_build(CELL, tail, head)  (get_moreBits(CELL) | CATBITS_PAIR | ((Cell)(tail) << 32) | ((Cell)(head) << 8))

#define small_build(CELL, data)        (get_moreBits(CELL) | CATBITS_SMALL | ((Cell)(data) << 8))

#define tag_build(CELL, checksum, J)   (get_moreBits(CELL) | CATBITS_TAG | (((Cell)(checksum)) << 16) | ((J) << 8))

#define blob_build(CELL, checksum, J)  (get_moreBits(CELL) | CATBITS_BLOB | (((Cell)(checksum)) << 16) | ((J) << 8))

#define refs_build(CELL, r0, r1, J)               (get_moreBits(CELL) | CATBITS_CHAIN | ((Cell)(r0) << 32) | ((r1) << 8) | (J))

#define lastRefs_build(CELL, r0, r1)           (get_moreBits(CELL) | CATBITS_LAST | ((Cell)(r0) << 32) | ((r1) << 8) | (6))

#define slice_build(CELL, data, J)                (get_moreBits(CELL) | CATBITS_CHAIN | ((Cell)(data) << 8) | (J))

#define lastSlice_build(CELL, data, S)            (get_moreBits(CELL) | CATBITS_LAST | (((Cell)data) << 8) | (S))

#define sync_build(CELL, from, next, type)        (get_moreBits(CELL) | CATBITS_SYNC | ((Cell)(from) << 32) | ((next) << 8) | (type))

#define cell_setRootBit(cell, ROOTBIT) (((cell) & 0xFFFFFFFFFFFFFF7FLLU) | (ROOTBIT))

#define cell_chain(cell, jump) (((cell) & 0xF8FFFFFFFFFFFF00LLU) | CATBITS_CHAIN | (jump))

#define cell_unchain(cell)     (((cell) & 0xF8FFFFFFFFFFFF00LLU) | CATBITS_LAST)

#define cell_chainChild0(cell, jump) (((cell) & 0xFFFFFFFFFFFFFF80LLU) | (jump))

/** change C cell into a freed cell.
 */
#define cell_free(C) ((C) & 0xF800000000000000LLU)

/* ________________________________________
 * "More" counter management (all cells)
 */

/** tells if more counter is to the max
 */
#define cell_isOvermore(C) (((C) & 0xF800000000000000LLU) == 0xF800000000000000LLU)

/** "more" a C cell.
 * increments its more counter if not to the max
 */
#define cell_more(C) (cell_isOvermore(C) ? (C) : ((C) + 0x0800000000000000LLU))

/** "unmore" a C cell.
 * decrements its more counter if not to the max
 */
#define cell_unmore(C) (cell_isOvermore(C) ? (C) : ((C) - 0x0800000000000000LLU))

/* ________________________________________
 * Cell administrative bits
 */
#define MAX_JUMP   0xFF
#define MAX_CHILD0 0x7F

/* ________________________________________
 * Cell unbuilding macros
 */
#define cell_getCatBits(C) ((C) & CATBITS_MASK)
#define cell_getMore(C) ((C) >> 59)
#define cell_getTail(C) (Arrow)(((C) & 0xFFFFFF00000000LLU) >> 32)
#define cell_getHead(C) (Arrow)(((C) & 0xFFFFFF00LLU) >> 8)
#define cell_getChecksum(C) (((C) & 0xFFFFFFFFFF0000LLU) >> 16)
#define cell_getSlice(C)    (((C) & 0xFFFFFFFFFFFF00LLU) >> 8)
#define cell_getSmall(C)    (((C) & 0xFFFFFFFFFFFF00LLU) >> 8)
#define cell_getRef0(C)    (Arrow)(((C) & 0xFFFFFF00000000LLU) >> 32)
#define cell_getRef1(C)    (Arrow)(((C) & 0xFFFFFF00LLU) >> 8)
#define cell_getChild0(C) (Address)((C) & 0x7FLLU)
#define cell_getRootBit(C) ((C) & 0x80LLU)
#define cell_getJumpFirst(C) (((C) & 0xFF00LLU) >> 8)
#define cell_getJumpNext(C) ((C) & 0xFFLLU)
#define cell_getSize(C) ((C) & 0xFFLLU)
#define cell_getSyncType(C) ((C) & 0xFFLLU)
#define cell_getFrom(C) (Address)cell_getTail(C)
#define cell_getNext(C) (Address)cell_getHead(C)

/* ________________________________________
 * Cell testing macros
 */
#define more(C) ((C) & 0xF800000000000000LLU)
#define cell_isRooted(C) (cell_getRootBit(C))
#define cell_isFree(C) (((C) & 0x07FFFFFFFFFFFFFFLLU) == 0)
#define cell_isPair(C) (cell_getCatBits(C) == CATBITS_PAIR)
// The next tricky macro returns !0 for any valid arrow address
#define cell_isArrow(C) (((C) & 0x07FFFFFFFFFFFFFFLLU) && cell_getCatBits(C) <= CATBITS_CHAIN)
#define cell_isSmall(C) (cell_getCatBits(C) == CATBITS_SMALL)
#define cell_isTag(C) (cell_getCatBits(C) == CATBITS_TAG)
#define cell_isBlob(C) (cell_getCatBits(C) == CATBITS_BLOB)
#define cell_isEntrelacs(C) (cell_isTag(C) || cell_isBlob(C))
#define cell_isSyncFrom(C, address, type) (cell_getCatBits(C) == CATBITS_SYNC && cell_getFrom(C) == (address) && cell_getSyncType(cell) == (type))

// The loose stack.
// This is a stack of all newly created arrows still in loose state.

static struct s_loose {
    Arrow a;
    uint64_t checksum;
} *looseStack = NULL;
static Address looseStackMax = 0;
static Address looseStackSize = 0;

static void show_cell(char operation, Address address, Cell C, int ofSliceChain) {
    Cell catBits = cell_getCatBits(C);
    int catI = catBits >> 56;
    int more = (int) cell_getMore(C);
    Arrow tail = cell_getTail(C);
    Arrow head = cell_getHead(C);
    CellData checksum = (CellData) cell_getChecksum(C);
    CellData slice = (CellData) cell_getSlice(C);
    int small = (int) cell_getSmall(C);
    Arrow ref0 = cell_getRef0(C);
    Arrow ref1 = cell_getRef1(C);
    int child0 = cell_getChild0(C);
    int rooted = cell_getRootBit(C) ? 1 : 0;
    int jumpFirst = cell_getJumpFirst(C);
    int jumpNext = cell_getJumpNext(C);
    int size = cell_getSize(C);
    int syncType = cell_getSyncType(C);
    Address from = cell_getFrom(C);
    Address next = cell_getNext(C);

    static char* cats[] = {"Empty", "Arrow", "Small", "Tag", "Blob", "Chain", "Last", "Sync"};
    char* cat = cats[catI];

    if (cell_isFree(C)) {
        dputs("%c%06x FREE (M=%02x)", operation, address, more);
    } else if (cell_isPair(C)) {
        dputs("%c%06x M=%02x C=%1x tail@=%06x head@=%06x R=%1x C=%02x (%s)", operation, address, more, catI, tail, head, rooted, child0, cat); // an arrow
    } else if (cell_isTag(C) || cell_isBlob(C)) {
        dputs("%c%06x M=%02x C=%1x checksum=%010llx J=%02x R=%1x C=%02x (%s)", operation, address, more, catI, checksum, jumpFirst, rooted, child0, cat); // a tuple/tag/blob
    } else if (cell_isSmall(C)) {
        dputs("%c%06x M=%02x C=%1x small=%12x R=%1x C=%02x (%s)", operation, address, more, catI, small, rooted, child0, cat); // a small
    } else if (catBits == CATBITS_SYNC) {
        dputs("%c%06x M=%02x C=%1x from@=%06x next@=%06x T=%1x (%s)", operation, address, more, catI, from, next, syncType, cat); // Sync cell
    } else if (catBits == CATBITS_CHAIN) {
        static char c;
#define charAt(SLICE, POS) ((c = ((char*)&SLICE)[POS]) ? c : '.')
        if (ofSliceChain) {
            dputs("%c%06x M=%02x C=%1x slice=%012llx <%c%c%c%c%c%c> J=%02x (%s)", operation, address, more, catI, slice,
                    charAt(slice, 0), charAt(slice, 1), charAt(slice, 2), charAt(slice, 3), charAt(slice, 4), charAt(slice, 5), jumpNext, cat); // Chained cell with slice
        } else {
            dputs("%c%06x M=%02x C=%1x ref0@=%06x ref1@=%06x J=%02x (%s)", operation, address, more, catI, ref0, ref1, jumpNext, cat); // Chained cell with refs
        }
    } else if (catBits == CATBITS_LAST) {
        static char c;
        if (ofSliceChain) {
            dputs("%c%06x M=%02x C=%1x slice=%012llx <%c%c%c%c%c%c> S=%02x (%s)", operation, address, more, catI, slice,
                    charAt(slice, 0), charAt(slice, 1), charAt(slice, 2), charAt(slice, 3), charAt(slice, 4), charAt(slice, 5),
                    size, cat); // Last chained cell with slice
        } else {
            dputs("%c%06x M=%02x C=%1x ref0@=%06x ref1@=%06x (%s)", operation, address, more, catI, ref0, ref1, cat); // Last chained cell with refs
        }
    } else {
        assert(0 == 1);
    }
}

static void looseStackAdd(Address a) {
    geoalloc((char**) &looseStack, &looseStackMax, &looseStackSize, sizeof (struct s_loose), looseStackSize + 1);
    looseStack[looseStackSize - 1].a = a;
    looseStack[looseStackSize - 1].checksum = xl_checksumOf(a);
}

static void looseStackRemove(Address a) {
    assert(looseStackSize);
    if (looseStack[looseStackSize - 1].a == a)
        looseStackSize--;
    else if (looseStackSize > 1 && looseStack[looseStackSize - 2].a == a) {
        looseStackSize--;
        looseStack[looseStackSize - 1] = looseStack[looseStackSize];
    }
}


/* ________________________________________
 * Hash functions
 * One computes 3 kinds of hashcode.
 * * hashLocation: default cell address of an arrow.
 *     hashLocation = hashArrow(tail, head) % PRIM0 for a regular arrow
 *     hashLocation = hashString(string) for a tag arrow
 * * hashProbe: probing offset when looking for a free cell nearby a conflict.
 *     hProbe = hashArrow(tail, head) % PRIM1 for a regular pair
 * * hashChain: offset to separate chained cells
 *       chained data slices (and tuple ends) are separated by JUMP * h3 cells
 * * hashChild : offset to separate children cells
 *       first child in parent children list is at @parent + cell.child0 * hChild
 *       chained children cells are separated by JUMP * hChild cells
 */

/* hash function to get H1 from a regular pair definition */
uint64_t hashPair(uint64_t tail, uint64_t head) {
    return (tail << 20) ^ (tail >> 4) ^ head;
}

/* hash function to get H1 from a tag arrow definition */
uint64_t hashStringAndGetSize(char *str, uint32_t* length) { // simple string hash
    uint64_t hash = 5381;
    char *s = str;
    int c;
    uint32_t l = 0;
    while (c = *s++) { // for all bytes up to the null terminator
        // 5 bits shift only because ASCII varies within 5 bits
        hash = ((hash << 5) + hash) + c;
        l++;
    }
    //  // The null-terminator is counted and hashed for consistancy with hashRaw
    //  hash = ((hash << 5) + hash) + c;
    //  l++;

    TRACEPRINTF("hashStringAndGetSize(\"%s\") = [ %016llx ; %d] ", str, hash, l);

    *length = l;
    return hash;
}

/* hash function to get H1 from a binary tag arrow definition */
uint64_t hashRaw(char *buffer, uint32_t length) { // simple string hash
    uint64_t hash = 5381;
    int c;
    char *p = buffer;
    uint32_t l = length;
    while (l--) { // for all bytes (even last one)
        c = *p++;
        // 5 bits shift only because ASCII generally works within 5 bits
        hash = ((hash << 5) + hash) + c;
    }

    TRACEPRINTF("hashRaw(%d, \"%.*s\") = %016llx", length, length, buffer, hash);
    return hash;
}

/* hash function to get hashChain from a cell caracteristics */
uint32_t hashChain(Address address, CellData cell) { // Data chain hash offset
    // This hash mixes the cell address and its content
    return ((cell & 0x00FFFFFFFFFF0000LLU) << 8) ^ ((cell & 0x00FFFFFFFFFF0000LLU) >> 16) ^ address;
}

/* hash function to get hashChild from a cell caracteristics */
uint32_t hashChild(Address address, CellData cell) { // Data chain hash offset
    return -hashChain(address, cell);
}

/* ________________________________________
 * The blob cryptographic hash computing fonction.
 * This function returns a string used to define a tag arrow.
 *
 */
#define CRYPTO_SIZE 40

char* crypto(uint32_t size, char* data, char output[CRYPTO_SIZE + 1]) {
    char h[20];
    sha1(data, size, h);
    sprintf(output, "%08x%08x%08x%08x%08x", *(uint32_t *) h, *(uint32_t *) (h + 4), *(uint32_t *) (h + 8),
            *(uint32_t *) (h + 12), *(uint32_t *) (h + 16));
    return output;
}


/* ________________________________________
 * Address shifting and jumping macros
 * N : new location
 * H : current location
 * O : offset
 * S : start location
 * J : Jump multiplier
 * C : Cell
 */
#define shift(LOCATION, NEW, OFFSET) (NEW = (((LOCATION) + (OFFSET)) % (SPACE_SIZE)))

#define growingShift(LOCATION, NEW, OFFSET, START) ((shift(LOCATION, NEW, OFFSET) == (START)) ? shift(LOCATION, NEW,( ++OFFSET ? OFFSET :( OFFSET = 1 ))) : NEW)

#define jump(LOCATION, NEXT, JUMP, OFFSET, START) (NEXT = ((LOCATION + OFFSET * ( 1 + JUMP)) % SPACE_SIZE), assert(NEW != START))

static Address jumpToFirst(Cell cell, Address address, Address offset, Address start) {
    Address next = address;
    assert(cell_isArrow(cell));
    Cell jump = cell_getJumpFirst(cell);
    // Note jump=1 means 2 shifts!
    for (int i = 0; i <= jump; i++)
        growingShift(next, next, offset, address);

    if (jump == MAX_JUMP) { // one needs to look for a reattachment (sync) cell
        cell = mem_get(next);
        ONDEBUG((show_cell('R', next, cell, 0)));
        int loopDetect = 10000;
        while (--loopDetect && !cell_isSyncFrom(cell, address, REATTACHMENT_FIRST)) {
            growingShift(next, next, offset, address);
            cell = mem_get(next);
            ONDEBUG((show_cell('R', next, cell, 0)));
        }
        assert(loopDetect);
        next = cell_getNext(cell);
    }
    return next;
}

static Address jumpToNext(Cell cell, Address address, Address offset, Address start) {
    Address next = address;
    assert(cell_getCatBits(cell) == CATBITS_CHAIN);
    Cell jump = cell_getJumpNext(cell);
    // Note jump=1 means 2 shifts!
    for (int i = 0; i <= jump; i++)
        growingShift(next, next, offset, address);

    if (jump == MAX_JUMP) { // one needs to look for a sync cell
        cell = mem_get(next);
        ONDEBUG((show_cell('R', next, cell, 0)));
        int loopDetect = 10000;
        while (--loopDetect && !cell_isSyncFrom(cell, address, REATTACHMENT_NEXT)) {
            growingShift(next, next, offset, address);
            cell = mem_get(next);
            ONDEBUG((show_cell('R', next, cell, 0)));
        }
        assert(loopDetect);
        next = cell_getNext(cell);
    }
    return next;
}

static Address jumpToChild0(Cell cell, Address address, Address offset, Address start) {
    Address next = address;
    assert(cell_isArrow(cell));
    Cell jump = cell_getChild0(cell);
    if (!jump) return (Address) 0; // no child when jump=0

    // Note jump=1 means 1 shift!
    for (int i = 0; i < jump; i++)
        growingShift(next, next, offset, address);

    if (jump == MAX_CHILD0) { // one needs to look for a sync cell
        cell = mem_get(next);
        ONDEBUG((show_cell('R', next, cell, 0)));
        int loopDetect = 10000;
        while (--loopDetect && !cell_isSyncFrom(cell, address, REATTACHMENT_CHILD0)) {
            growingShift(next, next, offset, address);
            cell = mem_get(next);
            ONDEBUG((show_cell('R', next, cell, 0)));
        }
        assert(loopDetect);
        next = cell_getNext(cell);
    }
    return next;
}

/** return Eve */
Arrow xl_Eve() {
    return EVE;
}

static char* payloadOf(Cell catBits, Arrow a, uint32_t* lengthP);

/** retrieve the arrow corresponding to data at $str with size $length.
 * Will create the singleton if missing except if $ifExist is set.
 * data might be a blob signature or a tag content.
 */
Arrow payload(Cell catBits, int length, char* str, uint64_t hash, int ifExist) {
    Address hashLocation, hashProbe, hChain;
    uint32_t l;
    CellData checksum;
    Address probeAddress, firstFreeCell, newArrow, current, next;
    Cell cell, nextCell, content;
    unsigned i, jump;
    char c, *p;

    space_stats.atom++;

    if (!length) {
        return EVE;
    }

    hashLocation = hash % PRIM0;
    hashProbe = hash % PRIM1;
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    checksum = hash & 0xFFFFFFFFFFLLU;

    // Search for an existing singleton
    probeAddress = hashLocation;
    firstFreeCell = EVE;
    while (1) {
        cell = mem_get(probeAddress);
        ONDEBUG((show_cell('R', probeAddress, cell, 1)));
        if (cell_isFree(cell))
            firstFreeCell = probeAddress;
        else if (cell_getCatBits(cell) == catBits
                && cell_getChecksum(cell) == checksum) {
            // chance we found it
            // now comparing the whole string
            hChain = hashChain(probeAddress, cell) % PRIM1;
            if (!hChain) hChain = 1; // offset can't be 0
            next = jumpToFirst(cell, probeAddress, hChain, probeAddress);
            p = str;
            l = length;
            i = 1;
            cell = mem_get(next);
            ONDEBUG((show_cell('R', next, cell, 1)));
            if (l) {
                while ((c = *p++) == ((char*) &cell)[i] && --l) {
                    if (i == 6) {
                        if (cell_getCatBits(cell) == CATBITS_LAST) {
                            break; // the chain is over
                        }
                        next = jumpToNext(cell, next, hChain, probeAddress);
                        cell = mem_get(next);
                        ONDEBUG((show_cell('R', next, cell, 1)));
                        i = 1;
                    } else
                        i++;
                }
            }
            if (!l && cell_getCatBits(cell) == CATBITS_LAST && cell_getSize(cell) == i) {
              space_stats.found++;
                return probeAddress; // found arrow
            }
        }
        // Not the singleton

        if (!more(cell)) {
            break; // Miss
        }

        growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
    }

    if (ifExist)
        return EVE;
    space_stats.new++;
    space_stats.atom++;

    if (firstFreeCell == EVE) {
        while (1) {
            growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
            cell = mem_get(probeAddress);
            ONDEBUG((show_cell('R', probeAddress, cell, 0)));
            if (cell_isFree(cell)) {
                firstFreeCell = probeAddress;
                break;
            }
        }
    }

    /* Create a missing singleton */
    newArrow = firstFreeCell;
    cell = mem_get(newArrow);
    ONDEBUG((show_cell('R', newArrow, cell, 1)));
    cell = (catBits == CATBITS_TAG
            ? tag_build(cell, checksum, 0 /* jump */)
            : blob_build(cell, checksum, 0 /* jump */));

    hChain = hashChain(newArrow, cell) % PRIM1;
    if (!hChain) hChain = 1; // offset can't be 0

    Address offset = hChain;
    growingShift(newArrow, next, offset, newArrow);
    nextCell = mem_get(next);
    ONDEBUG((show_cell('R', next, nextCell, 1)));
    jump = 0; // Note how jump=0 means 1 shift.
    while (!cell_isFree(nextCell)) {
        jump++;
        growingShift(next, next, offset, newArrow);
        nextCell = mem_get(next);
        ONDEBUG((show_cell('R', next, nextCell, 1)));
    }

    if (jump >= MAX_JUMP) {
        Address sync = next;
        Cell syncCell = nextCell;
        syncCell = sync_build(syncCell, newArrow, 0, REATTACHMENT_FIRST);
        mem_set(sync, syncCell);
        ONDEBUG((show_cell('W', sync, syncCell, 0)));
        offset = hChain;
        growingShift(next, next, offset, sync);
        nextCell = mem_get(next);
        ONDEBUG((show_cell('R', next, nextCell, 1)));
        while (!cell_isFree(nextCell)) {
            growingShift(next, next, offset, sync);
            nextCell = mem_get(next);
            ONDEBUG((show_cell('R', next, nextCell, 1)));
        }
        syncCell = sync_build(syncCell, newArrow, next, REATTACHMENT_FIRST);
        mem_set(sync, syncCell);
        ONDEBUG((show_cell('W', sync, syncCell, 0)));

        jump = MAX_JUMP;
    }
    cell = (catBits == CATBITS_TAG
            ? tag_build(cell, checksum, jump)
            : blob_build(cell, checksum, jump));
    mem_set(newArrow, cell);
    ONDEBUG((show_cell('W', newArrow, cell, 0)));

    current = next;
    p = str;
    l = length;
    i = 0;
    content = 0;
    c = 0;
    if (l) {
        while (1) {
            c = *p++;
            ((char*) &content)[i] = c;
            if (!--l) break;
            if (++i == 6) {

                Address offset = hChain;
                growingShift(current, next, offset, current);
                jump = 0; // Note how jump=0 means 1 shift.
                nextCell = mem_get(next);
                ONDEBUG((show_cell('R', next, nextCell, 1)));
                while (!cell_isFree(nextCell)) {
                    jump++;
                    growingShift(next, next, offset, current);
                    nextCell = mem_get(next);
                    ONDEBUG((show_cell('R', next, nextCell, 1)));
                }
                if (jump >= MAX_JUMP) {
                    Address sync = next;
                    Cell syncCell = nextCell;
                    syncCell = sync_build(syncCell, current, 0, REATTACHMENT_NEXT); // 0 = jump sync
                    mem_set(sync, syncCell);
                    ONDEBUG((show_cell('W', sync, syncCell, 0)));

                    offset = hChain;
                    growingShift(next, next, offset, sync);
                    nextCell = mem_get(next);
                    ONDEBUG((show_cell('R', next, nextCell, 1)));
                    while (!cell_isFree(nextCell)) {
                        growingShift(next, next, offset, sync);
                        nextCell = mem_get(next);
                        ONDEBUG((show_cell('R', next, nextCell, 1)));
                    }
                    syncCell = sync_build(syncCell, current, next, REATTACHMENT_NEXT); // 0 = jump sync
                    mem_set(sync, syncCell);
                    ONDEBUG((show_cell('W', sync, syncCell, 0)));

                    jump = MAX_JUMP;
                }
                cell = mem_get(current);
                ONDEBUG((show_cell('R', current, cell, 1)));
                cell = slice_build(cell, content, jump);
                mem_set(current, cell);
                ONDEBUG((show_cell('W', current, cell, 1)));
                i = 0;
                content = 0;
                current = next;
            }

        }
    }
    cell = mem_get(current);
    ONDEBUG((show_cell('R', current, cell, 1)));
    cell = lastSlice_build(cell, content, 1 + i /* size */);
    mem_set(current, cell);
    ONDEBUG((show_cell('W', current, cell, 1)));


    /* Now incremeting "more" counters in the probing path up to the new singleton
     */
    probeAddress = hashLocation;
    hashProbe = hash % PRIM1;
    while (probeAddress != newArrow) {
        cell = mem_get(probeAddress);
        ONDEBUG((show_cell('R', probeAddress, cell, 0)));
        cell = cell_more(cell);
        mem_set(probeAddress, cell);
        ONDEBUG((show_cell('W', probeAddress, cell, 0)));
        growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
    }

    // log loose arrow
    looseStackAdd(newArrow);
    return newArrow;
}


#define CHECKSUM_CACHE_SIZE 1024

static struct s_checksumCacheEntry {
    uint64_t checksum;
    Arrow a;
} *checksumCache = NULL;

uint64_t xl_checksumOf(Arrow a) {
    if (a == XL_EVE)
        return 0x8200000000LU; // Eve checksum is not zero!

    if (a >= SPACE_SIZE) return 0;
    LOCK();
    
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell))
        return LOCK_OUT(0);


    // General case
    if (!checksumCache) {
        checksumCache = (struct s_checksumCacheEntry *) malloc(sizeof (struct s_checksumCacheEntry) * CHECKSUM_CACHE_SIZE);
        memset(checksumCache, 0, sizeof (struct s_checksumCacheEntry) * CHECKSUM_CACHE_SIZE);
    } else if (checksumCache[a % CHECKSUM_CACHE_SIZE].a == a) { // cache hit!
        uint64_t checksum = checksumCache[a % CHECKSUM_CACHE_SIZE].checksum;
        // DEBUGPRINTF("Cache('checksum(%06x)) = %016llx", a, checksum);
        return LOCK_OUT(checksum);
    }

    if (cell_isPair(cell)) {
        Arrow tail = cell_getTail(cell);
        Arrow head = cell_getHead(cell);
        uint64_t c1 = xl_checksumOf(tail);
        uint64_t c2 = xl_checksumOf(head);
        uint64_t checksum = hashPair(c1, c2);
        checksumCache[a % CHECKSUM_CACHE_SIZE].a = a;
        checksumCache[a % CHECKSUM_CACHE_SIZE].checksum = checksum;
        // DEBUGPRINTF("Cache(checksum(%06x), %016llx))", a, checksum);
        return LOCK_OUT(checksum);

    } else if (cell_isTag(cell)) {
        uint32_t tagLength;
        char *tag = payloadOf(CATBITS_TAG, a, &tagLength);
        uint64_t checksum = hashRaw(tag, tagLength);
        free(tag);
        checksumCache[a % CHECKSUM_CACHE_SIZE].a = a;
        checksumCache[a % CHECKSUM_CACHE_SIZE].checksum = checksum;
        // DEBUGPRINTF("Cache(checksum(%06x), %016llx))", a, checksum);
        return LOCK_OUT(checksum);
        
    } else if (cell_isBlob(cell)) {
        uint32_t blobHashLength;
        char *blobHash = payloadOf(CATBITS_BLOB, a, &blobHashLength);
        uint64_t checksum = hashRaw(blobHash, blobHashLength);
        free(blobHash);
        checksumCache[a % CHECKSUM_CACHE_SIZE].a = a;
        checksumCache[a % CHECKSUM_CACHE_SIZE].checksum = checksum;
        // DEBUGPRINTF("Cache(checksum(%06x), %016llx))", a, checksum);
        return LOCK_OUT(checksum);

    } else {
        assert(0);
        checksumCache[a % CHECKSUM_CACHE_SIZE].a = a;
        checksumCache[a % CHECKSUM_CACHE_SIZE].checksum = 0;
        // DEBUGPRINTF("Cache(checksum(%06x, %016llx))", a, 0);
        return LOCK_OUT(0);
    }
}

/** retrieve tail->head pair
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
static Arrow pair(Arrow tail, Arrow head, int ifExist) {
    uint64_t hash;
    Address hashLocation, hashProbe;
    Address probeAddress, firstFreeCell, newArrow;
    Cell cell;

    // NOPE:Eve special case
    // It's a mistake
    //if (tail == EVE && head == EVE) {
    //   return EVE;
    //  }
    if (tail == NIL || head == NIL)
        return NIL;
 
    space_stats.get++;

    // Compute hashs
    hash = hashPair(xl_checksumOf(tail), xl_checksumOf(head));
    hashLocation = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0
    
    // Probe for an existing singleton
    probeAddress = hashLocation;
    firstFreeCell = EVE;
    while (1) { // TODO: thinking about a probing limit
        cell = mem_get(probeAddress);
        ONDEBUG((show_cell('R', probeAddress, cell, 0)));
        if (cell_isFree(cell))
            firstFreeCell = probeAddress;
        else if (cell_isPair(cell) // TODO test in a single binary word comparison
                && cell_getTail(cell) == tail
                && cell_getHead(cell) == head
                && probeAddress /* ADAM case, can't be put at EVE place! TODO: optimize */) {
            space_stats.found++;
            return probeAddress; // OK: arrow found!
        }
        // Not the singleton

        if (!more(cell)) {
            break; // Probing over. It's a miss.
        }

        growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
    }
    // Miss

    if (ifExist) // one only want to test for singleton existence in the arrows space
        return EVE; // Eve means not found

    space_stats.new++;
    space_stats.pair++;

    if (firstFreeCell == EVE) {
        while (1) {
            growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
            cell = mem_get(probeAddress);
            ONDEBUG((show_cell('R', probeAddress, cell, 0)));
            if (cell_isFree(cell)) {
                firstFreeCell = probeAddress;
                break;
            }
        }
    }

    /* Create a singleton */
    newArrow = firstFreeCell;
    cell = mem_get(newArrow);
    ONDEBUG((show_cell('R', newArrow, cell, 0)));
    cell = pair_build(cell, tail, head);
    mem_set(newArrow, cell);
    ONDEBUG((show_cell('W', newArrow, cell, 0)));

    /* Now incremeting "more" counters in the probing path up to the new singleton
     */
    probeAddress = hashLocation;
    hashProbe = hash % PRIM1; // reset hashProbe to default
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    while (probeAddress != newArrow) {
        cell = mem_get(probeAddress);
        ONDEBUG((show_cell('R', probeAddress, cell, 0)));
        cell = cell_more(cell);
        mem_set(probeAddress, cell);
        ONDEBUG((show_cell('W', probeAddress, cell, 0)));
        growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
    }

    // record new arrow as loose arrow
    looseStackAdd(newArrow);
    return newArrow;
}

Arrow xl_pair(Arrow tail, Arrow head) {
    LOCK();
    Arrow a = pair(tail, head, 0);
    return LOCK_OUT(a);
}

Arrow xl_pairMaybe(Arrow tail, Arrow head) {
    LOCK();
    Arrow a= pair(tail, head, 1);
    return LOCK_OUT(a);
}

/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
static Arrow blob(uint32_t size, char* data, int ifExist) {
    char signature[CRYPTO_SIZE + 1];
    crypto(size, data, signature);
    uint32_t signature_size;
    uint64_t hash = hashStringAndGetSize(signature, &signature_size);

    // A BLOB consists in:
    // - A signature, stored as a specialy typed tag in the arrows space.
    // - data stored separatly in some traditional filer outside the arrows space.
    mem0_saveData(signature, size, data);
    // TODO: remove data when cell at h is recycled.

    return payload(CATBITS_BLOB, signature_size, signature, hash, ifExist);
}

Arrow xl_blob(uint32_t size, char* data) {
    LOCK();
    Arrow a = blob(size, data, 0);
    return LOCK_OUT(a);
}

Arrow xl_blobMaybe(uint32_t size, char* data) {
    LOCK();
    Arrow a = blob(size, data, 1);
    return LOCK_OUT(a);
}

Arrow xl_atom(char* str) {
    uint32_t size;
    uint64_t hash = hashStringAndGetSize(str, &size);
    LOCK();
    Arrow a =  (size >= BLOB_MINSIZE
       ? blob(size, str, 0)
       : payload(CATBITS_TAG, size, str, hash, 0));
    return LOCK_OUT(a);
}

Arrow xl_atomMaybe(char* str) {
    uint32_t size;
    uint64_t hash = hashStringAndGetSize(str, &size);
    LOCK();
    Arrow a = (size >= BLOB_MINSIZE
        ? blob(size, str, 1)
        : payload(CATBITS_TAG, size, str, hash, 1));
    return LOCK_OUT(a);
}

Arrow xl_atomn(uint32_t size, char* mem) {
    Arrow a;
    LOCK();
    if (size >= BLOB_MINSIZE)
        a = blob(size, mem, 0);
    else {
        uint64_t hash = hashRaw(mem, size);
        a = payload(CATBITS_TAG, size, mem, hash, 0);
    }
    return LOCK_OUT(a);
}

Arrow xl_atomnMaybe(uint32_t size, char* mem) {
    Arrow a;
    LOCK();
    if (size >= BLOB_MINSIZE)
        a = blob(size, mem, 1);
    else {
        uint64_t hash = hashRaw(mem, size);
        a = payload(CATBITS_TAG, size, mem, hash, 1);
    }
    return LOCK_OUT(a);
}

Arrow xl_headOf(Arrow a) {
    if (a == EVE)
        return EVE;
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    assert(cell_isArrow(cell));
    if (!cell_isArrow(cell))
        return LOCK_OUT(-1); // Invalid id

    Cell catBits = cell_getCatBits(cell);
    if (catBits != CATBITS_PAIR)
        return LOCK_OUT(a); //TODO: Tuple

    Arrow head = (Arrow) cell_getHead(cell);
    return LOCK_OUT(head);
}

Arrow xl_tailOf(Arrow a) {
    if (a == EVE)
        return EVE;
    LOCK();
    Cell cell = mem_get(a);
    LOCK_END();
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell))
        return -1; // Invalid id

    Cell catBits = cell_getCatBits(cell);

    if (catBits != CATBITS_PAIR)
        return a; // TODO: Tuple

    return (Arrow) cell_getTail(cell);
}

static char* payloadOf(Cell catBits, Arrow a, uint32_t* lengthP) {
    Address current = a;
    if (a == EVE) {
        lengthP = 0;
        return (char *) 0; // EVE => NUL
    }

    Cell cell = mem_get(current);
    ONDEBUG((show_cell('R', current, cell, 1)));
    Cell _catBits = cell_getCatBits(cell);
    if (_catBits != catBits) {
        lengthP = 0;
        return (char *) 0; // Invalid Id : FIXME : -1
    }

    uint32_t hChain = hashChain(a, cell) % PRIM1;
    if (!hChain) hChain = 1; // offset can't be 0

    current = jumpToFirst(cell, current, hChain, a);
    cell = mem_get(current);
    ONDEBUG((show_cell('R', current, cell, 1)));

    unsigned i = 1;
    char* tag = NULL;
    uint32_t size = 0;
    uint32_t max = 0;
    //TODO:QUICKER COPY
    while (1) {
        geoalloc((char**) &tag, &max, &size, sizeof (char), size + 1);
        tag[size - 1] = ((char*) &cell)[i];
        i++;
        if (i == 7) {
            if (cell_getCatBits(cell) == CATBITS_LAST) {
                break; // the chain is over
            }
            current = jumpToNext(cell, current, hChain, a);
            cell = mem_get(current);
            ONDEBUG((show_cell('R', current, cell, 1)));
            i = 1;
        }
    }
    size -= (6 - cell_getSize(cell));
    // 2 next lines: add a trailing 0
    geoalloc((char**) &tag, &max, &size, sizeof (char), size + 1);
    tag[size - 1] = 0;
    *lengthP = size - 1;
    return tag;
}

char* xl_memOf(Arrow a, uint32_t* lengthP) {
    if (a == EVE) {
        *lengthP = 0;
        return (char *) 0;
    }
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (cell_isFree(cell)) {
        *lengthP = 0;
        return (char *) LOCK_OUT(0); // FIXME -1
    }

    if (cell_isBlob(cell)) {
        uint32_t blobHashLength;
        char* blobHash = payloadOf(CATBITS_BLOB, a, &blobHashLength);
        if (!blobHash) {
            *lengthP = 0;
            return (char *) LOCK_OUT(0);
        }
        char* data = mem0_loadData(blobHash, lengthP);
        free(blobHash);
        return LOCK_OUT(data);
    } else {
        char* data = payloadOf(CATBITS_TAG, a, lengthP);
        return LOCK_OUT(data);
    }
}

char* xl_strOf(Arrow a) {
    uint32_t lengthP;
    char* p = xl_memOf(a, &lengthP);
    return p;
}


// URL-encode input buffer into destination buffer.
// 0-terminate the destination buffer.

static void percent_encode(const char *src, size_t src_len, char *dst, size_t* dst_len_p) {
    static const char *dont_escape = "_-,;~()";
    static const char *hex = "0123456789abcdef";
    static size_t i, j;
    for (i = j = 0; i < src_len; i++, src++, dst++, j++) {
        if (*src && (isalnum(*(const unsigned char *) src) ||
                strchr(dont_escape, * (const unsigned char *) src) != NULL)) {
            *dst = *src;
        } else {
            dst[0] = '%';
            dst[1] = hex[(* (const unsigned char *) src) >> 4];
            dst[2] = hex[(* (const unsigned char *) src) & 0xf];
            dst += 2;
            j += 2;
        }
    }

    *dst = '\0';
    *dst_len_p = j;
}


// URL-decode input buffer into destination buffer.
// 0-terminate the destination buffer.

static void percent_decode(const char *src, size_t src_len, char *dst, size_t* dst_len_p) {
    static size_t i, j;
    int a, b;
#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

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

static char* toURI(Arrow a, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (a == XL_EVE) {
        char *s = (char*) malloc(1);
        assert(s);
        s[0] = 0;
        if (l) *l = 0;
        return s;
    }

    if (a >= SPACE_SIZE)
        return NULL;

    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (cell_isFree(cell))
        return NULL;

    Cell catBits = cell_getCatBits(cell);

    switch (catBits) {
        case CATBITS_BLOB:
        {
            return xl_digestOf(a, l);
        }
        case CATBITS_TAG: case CATBITS_SMALL:
        {
            uint32_t tagLength, encodedDataLength;
            char* tag = xl_memOf(a, &tagLength);
            char *uri = malloc(3 * tagLength + 1);
            assert(uri);
            percent_encode(tag, tagLength, uri, &encodedDataLength);
            free(tag);
            if (l) *l = encodedDataLength;
            uri = realloc(uri, 1 + encodedDataLength);
            return uri;
        }
        case CATBITS_PAIR:
        {
            uint32_t l1, l2;
            char *tailUri = toURI(xl_tailOf(a), &l1);
            if (!tailUri) return NULL;
            char *headUri = toURI(xl_headOf(a), &l2);
            if (!headUri) {
                free(tailUri);
                return NULL;
            }

            char *uri = malloc(2 + l1 + l2 + 1);
            assert(uri);
            sprintf(uri, "/%s+%s", tailUri, headUri);
            free(tailUri);
            free(headUri);
            if (l) *l = 2 + l1 + l2;
            return uri;
        }
        default:
            assert(0);
            return NULL;
    }
}

#define DIGEST_CHECKSUM_SIZE 16
#define DIGEST_SIZE (2 + CRYPTO_SIZE + DIGEST_CHECKSUM_SIZE)

char* xl_digestOf(Arrow a, uint32_t *l) {
    TRACEPRINTF("BEGIN xl_digestOf(%06x)", a);
    char *digest;
    char* hashStr;
    uint32_t hashLength;
    uint64_t checksum;
    if (a == XL_EVE) {
        digest = (char*) malloc(1);
        assert(digest);
        digest[0] = 0;
        *l = 0;
        return digest;
    }

    if (a >= SPACE_SIZE)
        return NULL;

    LOCK();

    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell))
        return LOCK_OUT(NULL);

    Cell catBits = cell_getCatBits(cell);

    switch (catBits) {
        case CATBITS_PAIR:
        {
            uint32_t uriLength;
            char* uri = toURI(a, &uriLength);
            hashStr = (char *) malloc(CRYPTO_SIZE + 1);
            assert(hashStr);
            crypto(uriLength, uri, hashStr);
            free(uri);
            hashLength = strlen(hashStr);
            checksum = xl_checksumOf(a);
            break;
        }
        case CATBITS_BLOB:
        {
            hashStr = payloadOf(CATBITS_BLOB, a, &hashLength);
            checksum = hashRaw(hashStr, hashLength);
            break;
        }
        case CATBITS_TAG:
        {
            uint32_t dataSize;
            char* data = xl_memOf(a, &dataSize);
            hashStr = (char *) malloc(CRYPTO_SIZE + 1);
            assert(hashStr);
            crypto(dataSize, data, hashStr);
            free(data);
            hashLength = strlen(hashStr);
            checksum = hashRaw(data, dataSize);

            break;
        }
        default:
        {
            assert(0);
        }
    }
    uint32_t encodedChecksumLength, digestLength;
    digest = (char *) malloc(DIGEST_SIZE + 1);
    assert(digest);
    digest[0] = '$';
    digest[1] = 'H';
    digestLength = 2;

    // TODO: more efficient
    static const char *hex = "0123456789abcdef";
    char checksumMap[DIGEST_CHECKSUM_SIZE] = {
        hex[(checksum >> 60) & 0xF],
        hex[(checksum >> 56) & 0xF],
        hex[(checksum >> 52) & 0xF],
        hex[(checksum >> 48) & 0xF],
        hex[(checksum >> 44) & 0xF],
        hex[(checksum >> 40) & 0xF],
        hex[(checksum >> 36) & 0xF],
        hex[(checksum >> 32) & 0xF],
        hex[(checksum >> 28) & 0xF],
        hex[(checksum >> 24) & 0xF],
        hex[(checksum >> 20) & 0xF],
        hex[(checksum >> 16) & 0xF],
        hex[(checksum >> 12) & 0xF],
        hex[(checksum >> 8)  & 0xF],
        hex[(checksum >> 4)  & 0xF],
        hex[checksum & 0xF]
    };
    strncpy(digest + digestLength, checksumMap, DIGEST_CHECKSUM_SIZE);
    digestLength += DIGEST_CHECKSUM_SIZE;

    strncpy(digest + digestLength, hashStr, hashLength);
    free(hashStr);
    digestLength += hashLength;
    digest[digestLength] = '\0';
    digest = realloc(digest, 1 + digestLength);
    if (l) *l = digestLength;

    return LOCK_OUT(digest);
}

char* xl_uriOf(Arrow a, uint32_t *l) {
    LOCK();
    char *str = toURI(a, l);
    return LOCK_OUT(str);
}

Arrow xl_digestMaybe(char* digest) {
    TRACEPRINTF("BEGIN xl_digestMaybe(%.56s)", digest);

    int i = 2; // $H
    uint64_t hash = HEXTOI(digest[i]);
    while (++i <= 17) {
        hash = (hash << 4) | (uint64_t)HEXTOI(digest[i]);
    }
    
    Address hashLocation, hashProbe;
    Address probeAddress;
    Cell cell;

    hashLocation = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    // Probe for an existing singleton
    DEBUGPRINTF("hash is %llX so probe from %06x", hash, hashLocation);

    LOCK();

    probeAddress = hashLocation;
    while (1) { // TODO: thinking about a probing limit
        cell = mem_get(probeAddress);
        ONDEBUG((show_cell('R', probeAddress, cell, 0)));
        if (cell_isArrow(cell) && xl_checksumOf(probeAddress) == hash) {
            char* otherDigest = xl_digestOf(probeAddress, NULL);
            if (!strcmp(otherDigest, digest)) {
                free(otherDigest);
                return LOCK_OUT(probeAddress); // arrow found!
            }
            free(otherDigest);
        }

        if (!more(cell)) {
            return LOCK_OUT(EVE); // Probing over. It's a miss.
        }

        growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
    }
}

static Arrow fromUri(uint32_t size, unsigned char* uri, uint32_t* uriLength_p, int ifExist) {
    TRACEPRINTF("BEGIN fromUri(%s)", uri);
    Arrow a = NIL;
#define NAN (uint32_t)(-1)
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
                    TRACEPRINTF("fromUri - not long enough for a digest: %d", size);
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
                    TRACEPRINTF("fromUri - wrong digest size %d", DIGEST_SIZE);
                    break;
                }

                a = xl_digestMaybe(uri);
                
                if (a == NIL || a == EVE) // Non assimilated blob
                    uriLength = NAN;
                
                break;
            }
            case '/':
            { // Pair
                uint32_t tailUriLength, headUriLength;
                Arrow tail, head;
                
                if (size != NAN) size--;
                
                tail = fromUri(size, uri + 1, &tailUriLength, ifExist);
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
                
                head = fromUri(size, headUriStart, &headUriLength, ifExist);
                if (headUriLength == NAN) { // Non assimilated head
                    a = head; // NIL or EVE
                    uriLength = NAN;
                    break;
                }

                a = pair(tail, head, ifExist);
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

                uriLength = 0;
                while ((size == NAN || uriLength < size)
                        && (c = uri[uriLength]) > 32 && c != '+' && c != '/')
                    uriLength++;
                assert(uriLength);

                char *atomStr = malloc(uriLength + 1);
                percent_decode(uri, uriLength, atomStr, &atomLength);
                if (atomLength >= BLOB_MINSIZE)
                    a = blob(atomLength, atomStr, ifExist);
                else {
                    uint64_t hash = hashRaw(atomStr, atomLength);
                    a = payload(CATBITS_TAG, atomLength, atomStr, hash, ifExist);
                }
                free(atomStr);
                
                if (a == NIL || a == EVE) { // Non assimilated
                    uriLength = NAN;
                }
            }
        }

    DEBUGPRINTF("END fromUri(%s) = %O (length = %d)", uri, a, uriLength);
    *uriLength_p = uriLength;
    return a;
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

static Arrow uri(uint32_t size, char *uri, int ifExist) { // TODO: document actual design
    char c;
    uint32_t uriLength, gap;
    Arrow a = fromUri(size, uri, &uriLength, ifExist);
    if (uriLength == NAN)
        return a; // return NIL (wrong URI) or EVE (not assimilated)
    
    if (size != NAN) size -= uriLength;
    gap = skeepSpacesAndOnePlus(size, uri + uriLength);
    if (size != NAN) size -= gap;
    uriLength += gap;
    while ((size == NAN || size--) && (c = uri[uriLength])) {
        DEBUGPRINTF("nextUri = >%s<", uri + uriLength);
        uint32_t nextUriLength;
        Arrow b = fromUri(size, uri + uriLength, &nextUriLength, ifExist);
        if (nextUriLength == NAN)
            return b; // return NIL (wrong URI) or EVE (not assimilated)
        uriLength += nextUriLength;
        if (size != NAN) size -= nextUriLength;
        gap = skeepSpacesAndOnePlus(size, uri + uriLength);
        if (size != NAN) size -= gap;
        uriLength += gap;
    
        a = pair(a, b, ifExist);
        if (a == EVE) {
          return EVE; // not assimilated pair
        }
    }

    return a;
}

Arrow xl_uri(char* aUri) {
    LOCK();
    Arrow a = uri(NAN, aUri, 0);
    return LOCK_OUT(a);
}

Arrow xl_uriMaybe(char* aUri) {
    LOCK();
    Arrow a = uri(NAN, aUri, 1);
    return LOCK_OUT(a);
}

Arrow xl_urin(uint32_t aSize, char* aUri) {
    LOCK();
    Arrow a = uri(aSize, aUri, 0);
    return LOCK_OUT(a);
}

Arrow xl_urinMaybe(uint32_t aSize, char* aUri) {
    LOCK();
    Arrow a = uri(aSize, aUri, 1);
    return LOCK_OUT(a);
}

Arrow xl_anonymous() {
    char anonymous[CRYPTO_SIZE + 1];
    do {
        char random[80];
        snprintf(random, sizeof(random), "an0nymouse:)%lx", (long)rand() ^ (long)time(NULL));
        crypto(80, random,  anonymous); // Access to unitialized data is wanted
    } while (xl_atomMaybe(anonymous));
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
    if (a == EVE) return EVE;
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    LOCK_END();
    if (cell_isFree(cell))
        return NIL;

    return (cell_isPair(cell) ? a : EVE);
}

Arrow xl_isAtom(Arrow a) {
    if (a == EVE) return EVE;

    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    LOCK_END();

    if (cell_isFree(cell))
        return NIL;

    return (cell_isEntrelacs(cell) ? a : EVE);
}

enum e_xlType xl_typeOf(Arrow a) {
    if (a == EVE) return XL_EVE;
    if (a >= SPACE_SIZE) return XL_UNDEF;
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    LOCK_END();

    if (cell_isFree(cell))
        return XL_UNDEF;

    static enum e_xlType map[] = {XL_UNDEF, XL_PAIR, XL_ATOM, XL_ATOM, XL_ATOM, XL_UNDEF, XL_UNDEF, XL_UNDEF};

    Cell catBits = cell_getCatBits(cell);
    int catI = catBits >> 56;
    return map[catI];
}

/** connect a child pair to its parent arrow "a".
 * it consists in adding the child to the children list of "a".
 * An pair gets connected to its both parents as soon as it or one of its descendants gets rooted.
 *
 * Steps:
 * 1) if it turns out the parent arrow was loose (i.e. unconnected + unrooted), then
 *   - one removes it from the "loose" log
 *   - one connects the parent arrow to its own parents (recursive connect() calls)
 * 2) Compute hashChild
 * 3) Scan the existing children list for a free bucket,
 *    starting from child0 up to the last linked cell.
 *   - If a free bucket is found, back-ref is put here. End.
 * 4) If previous scan failed.
 *   - search for a free cell and compute jumps count (offset multiplier)
 *     relatively to the last cell in chain (or "a" if no list yet).
 * 5) If it turns out one's adding the very first cell to an empty children list.
 *   5.1) If jumps count over limit ... The Pain!
 *     - Put a "reattachement" into found cell
 *     - Search for a second free cell
 *     - Hook the "reattachement" to the second free cell
 *     - Set child0 to MAX in parent definition
 *   5.2) Normal case: Put the back-ref into free cell and update child0 inside parent def.
 * 6) Otherelse, one's adding a new "last" cell after the current "last" of the children list.
 *   5.1) jumps count over limit
 *    - set jump to MAX in the current last
 *    - replace the 2d ref of the current last with the ref to the free cell
 *    - converts the free cell into a "last" cell, pointing the child ref
 *    - the replaced 2d ref is put back nearby the child ref
 *   5.2) The most regular case. jumps count not too high.
 *    - transform the free cell into a new cell.
 *    - chain it to the previous last by converting it into
 *      a "chain cell" and setting up its jump field.
 * TODO : when adding a new cell, try to reduce jumpers from time to time
 */
static void connect(Arrow a, Arrow child) {
    TRACEPRINTF("connect child=%06x to a=%06x", child, a);
    space_stats.connect++;
    if (a == EVE) return; // One doesn't store Eve connectivity. 18/8/11 Why not?
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    assert(cell_isArrow(cell));
    Cell catBits = cell_getCatBits(cell);
    int loose = (cell_getChild0(cell) == 0);

    if (loose) {
        // (try to) remove 'a' from the loose log
        looseStackRemove(a);
        // One recursively connects the parent arrow to its ancestors.
        if (catBits == CATBITS_PAIR) {
            connect(cell_getTail(cell), a);
            connect(cell_getHead(cell), a);
        }
    }
    CellData data = (catBits == CATBITS_PAIR || catBits == CATBITS_SMALL)
            ? cell_getSmall(cell)
            : cell_getChecksum(cell);

    uint32_t hChild = hashChild(a, data) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    Address current = jumpToChild0(cell, a, hChild, a);
    if (current) {
        cell = mem_get(current);
        ONDEBUG((show_cell('R', current, cell, 0)));
        while ((cell & 0xFFFFFF00000000LLU) &&
                (cell & 0x000000FFFFFF00LLU) &&
                cell_getCatBits(cell) != CATBITS_LAST) {
            if (cell_getJumpNext(cell) == MAX_JUMP) {
                // jump == MAX ==> "deep link": ref1 points to next
                current = cell_getRef1(cell);
            } else {
                current = jumpToNext(cell, current, hChild, a);
            }
            cell = mem_get(current);
            ONDEBUG((show_cell('R', current, cell, 0)));
        }
        if (!(cell & 0xFFFFFF00000000LLU)) {
            // first ref bucket is empty
            cell |= ((Cell) child << 32);
            mem_set(current, cell);
            ONDEBUG((show_cell('W', current, cell, 0)));
            DEBUGPRINTF("Connection using first ref bucket of %06x", current);
            return;
        } else if (!((Cell) cell & 0xFFFFFF00LLU)) {
            // second ref bucket is empty
            cell |= ((Cell) child << 8);
            mem_set(current, cell);
            ONDEBUG((show_cell('W', current, cell, 0)));
            DEBUGPRINTF("Connection using second ref bucket of %06x", current);
            return;
        }
    } else {
        current = a;
    }

    // if no free cell in list
    // free cell search and jump computing
    Address next;
    Cell nextCell;
    Address offset = hChild;
    unsigned jump = 1;
    growingShift(current, next, offset, current);
    nextCell = mem_get(next);
    ONDEBUG((show_cell('R', next, nextCell, 0)));
    while (!cell_isFree(nextCell)) {
        jump++;
        growingShift(next, next, offset, current);
        nextCell = mem_get(next);
        ONDEBUG((show_cell('R', next, nextCell, 0)));
    }

    if (current == a) { // No child yet
        DEBUGPRINTF("new child0: %06x ", next);
        if (jump >= MAX_CHILD0) { // ohhh the pain
            TRACEPRINTF("MAX CHILD0!");
            Address sync = next;
            Cell syncCell = nextCell;
            syncCell = sync_build(syncCell, a, 0, REATTACHMENT_CHILD0);
            mem_set(sync, syncCell);
            ONDEBUG((show_cell('W', sync, syncCell, 0)));

            offset = hChild;
            growingShift(next, next, offset, current);
            nextCell = mem_get(next);
            ONDEBUG((show_cell('R', next, nextCell, 0)));
            while (!cell_isFree(nextCell)) {
                growingShift(next, next, offset, current);
                nextCell = mem_get(next);
                ONDEBUG((show_cell('R', next, nextCell, 0)));
            }
            syncCell = sync_build(syncCell, a, next, REATTACHMENT_CHILD0);
            mem_set(sync, syncCell);
            ONDEBUG((show_cell('W', sync, syncCell, 0)));

            nextCell = lastRefs_build(nextCell, child, 0);

            jump = MAX_CHILD0;
        } else { // Easy one
            DEBUGPRINTF("child0=%d", jump);
            nextCell = lastRefs_build(nextCell, child, 0);
        }
        mem_set(next, nextCell);
        ONDEBUG((show_cell('W', next, nextCell, 0)));
        cell = cell_chainChild0(cell, jump);
        mem_set(a, cell);
        ONDEBUG((show_cell('W', a, cell, 0)));
    } else {
        TRACEPRINTF("new chain: %06x ", next);
        if (jump >= MAX_JUMP) {
            TRACEPRINTF(" MAX JUMP!");
            nextCell = lastRefs_build(nextCell, cell_getRef1(cell), child);
            mem_set(next, nextCell);
            ONDEBUG((show_cell('W', next, nextCell, 0)));

            // When jump = MAX_JUMP in a children cell, ref1 actually points to the next chained cell
            cell = refs_build(cell, cell_getRef0(cell), next, MAX_JUMP);
            mem_set(current, cell);
            ONDEBUG((show_cell('W', current, cell, 0)));
        } else {
            DEBUGPRINTF(" jump=%d", jump);
            // One add a new cell (a "last" chained cell)
            nextCell = lastRefs_build(nextCell, child, 0);
            mem_set(next, nextCell);
            ONDEBUG((show_cell('W', next, nextCell, 0)));
            // One chain the previous last cell with this last celll
            cell = cell_chain(cell, jump - 1);
            mem_set(current, cell);
            ONDEBUG((show_cell('W', current, cell, 0)));
        }
    }
}

/** disconnect a child pair from its parent arrow "a".
 * - it consists in removing the child from the children list of "a".
 * - an pair gets disconnected from its both parents as soon as it gets both unrooted and child-less.
 *
 * Steps:
 * 1) compute hashChild
 * 2) find the first cell of the children list. Note jumpToChild0 can be complex when a.JUMP == MAX
 * 3) Scan the children list for the child ref.
 * 4) Erase the back ref from the found chain cell.
 * 5) If still data in the chain cell (except case where JUMP==MAX), one leaves it as is. Work is done.
 *    Otherwise, one undertakes to remove the empty chell chain.
 * 6) If the empty cell is the last.
 *   6.1) Free the cell.
 *   6.2) If not the first chain cell. Unchain the previous cell by making it the last chained cell.
 *   6.3) If one's removing the unique cell of the list.
 *    - Reset child0 in "a"
 *    - If "a" is not rooted, then it gets loose.
 *      * So one adds it to the "loose log"
 *      * one disconnects "a" from its both parents.
 * 7) If the empty cell is a chain cell (not the last)
 *   7.1) Free the empty cell.
 *   7.2) Find the next cell in the chain.
 *   7.3) If there's a previous cell. One makes it point to the found cell.
 *    - If the previous chain cell was already a deep link, one simply makes it pointing to next. Done.
 *    - Else one computes the new jumps count.
 *    - If jumps count < MAX_JUMP, one simply updates the previous cell. Done.
 *    - If jumps count == MAX, one puts a deep link into the previous cell.
 *      If one overwrittes an other child back-reference in the process,
 *      then one connects this child in turn.
 *   7.5) If there's no previous cell,
 *    - one adds the jump amount to a.child0 (don't forget + 1 to jump over freed cell)
 *    - if jumps count still < MAX, then it's easy.
 *    - but if jumps == MAX, then one restores the freed cell and convert it into a "deep link" cell.
 *      Note "a" definition is untouched since it already points to the modified cell.
 *
 * Reminder: If the parent arrow gets unreferred (child-less and unrooted), then it gets disconnected
 *    in turn from its head and tail (recursive calls) and it gets recorded as a loose arrow (step 6.3).
 */
static void disconnect(Arrow a, Arrow child) {
    TRACEPRINTF("disconnect child=%06x from a=%06x", child, a);
    space_stats.disconnect++;
    if (a == EVE) return; // One doesn't store Eve connectivity.

    // get parent arrow definition
    Cell parent = mem_get(a);
    ONDEBUG((show_cell('R', a, parent, 0)));
    // check it's a valid arrow
    assert(cell_isArrow(parent));

    // get its category
    Cell catBits = cell_getCatBits(parent);

    // get data used to compute hashChild. data depends on arrow category
    CellData data = (catBits == CATBITS_PAIR || catBits == CATBITS_SMALL)
            ? cell_getSmall(parent)
            : cell_getChecksum(parent);

    // compute parent arrow hChild (offset for child list)
    uint32_t hChild = hashChild(a, data) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    // find the beginning address of the parent arrow child chain
    Address current = jumpToChild0(parent, a, hChild, a);
    assert(current);
    Address previous = 0;
    // childToRef0 and childToRef1 values make comparisons quicker
    Cell childToRef0 = (Cell) child << 32;
    Cell childToRef1 = (Cell) child << 8;
    // get the first child refs storing cell
    Cell chain = mem_get(current);
    ONDEBUG((show_cell('R', current, chain, 0)));
    while ((chain & 0xFFFFFF00000000LLU) != childToRef0 &&
            (chain & 0xFFFFFF00LLU) != childToRef1 &&
            cell_getCatBits(chain) != CATBITS_LAST) { // while not found

        previous = current;
        if (cell_getJumpNext(chain) == MAX_JUMP) {
            // jump == MAX ==> "deep link": ref1 points to next
            current = cell_getRef1(chain);
        } else {
            current = jumpToNext(chain, current, hChild, a);
        }
        chain = mem_get(current);
        ONDEBUG((show_cell('R', current, chain, 0)));
    }

    // at this step, we're supposed to have found the child back-ref
    // ones erases this back-ref
    if (childToRef0 == (chain & 0xFFFFFF00000000LLU)) {
        // empty out first ref bucket
        chain &= 0xFF000000FFFFFFFFLLU;
    } else {
        // empty out second ref
        assert(childToRef1 == (chain & 0xFFFFFF00LLU));
        chain &= 0xFFFFFFFF000000FFLLU;
    }

    if (cell_getCatBits(chain) == CATBITS_CHAIN && cell_getJumpNext(chain) == MAX_JUMP) { // "deep link" case
        assert(!(chain & 0xFFFFFF00000000LLU));
        // one removes a "deep link" child chain cell
    } else if (chain & 0xFFFFFFFFFFFF00LLU) {
        // still an other back-ref in this child chain cell
        mem_set(current, chain);
        ONDEBUG((show_cell('W', current, chain, 0)));
        // nothing more to do
        return;
    }

    // The modified cell contains no more back-reference
    // One removes the cell from the children list
    if (cell_getCatBits(chain) == CATBITS_LAST) {
        // this is the last cell

        // freeing the whole cell
        chain = cell_free(chain);
        mem_set(current, chain);
        ONDEBUG((show_cell('W', current, chain, 0)));

        // FIXME detect a chain of deep links up to this last cell
        if (previous) {
            // Unchain previous cell (make it the last chained cell)
            Cell previousCell = mem_get(previous);
            ONDEBUG((show_cell('R', previous, previousCell, 0)));
            if (cell_getCatBits(previousCell) == CATBITS_CHAIN && cell_getJumpNext(previousCell) == MAX_JUMP)
                // if the previous cell is a "deep link" cell, one transforms it into a last chain cell
                previousCell = lastRefs_build(previousCell, cell_getRef0(previousCell), 0);
            else
                // if the previous cell is a regular chain cell, one converts it into a last chain cell
                previousCell = cell_unchain(previousCell);
            mem_set(previous, previousCell);
            ONDEBUG((show_cell('W', previous, previousCell, 0)));
        } else {
            // this is both the first and last cell of its chain!
            // so we've removed the last child from the list. The list is empty.
            parent = cell_chainChild0(parent, 0);
            mem_set(a, parent);
            ONDEBUG((show_cell('W', a, parent, 0)));
            // FIXME: if cell.jump == MAX_CHILD0 there is a child0 reattachment cell to delete!
            if (!cell_isRooted(parent)) {
                // The parent arrow is totally unreferred
                // Add it to the loose log
                looseStackAdd(a);
                // One disconnects the pair from its parents
                // 2 recursive calls
                if (cell_isPair(parent)) {
                    disconnect(cell_getTail(parent), a);
                    disconnect(cell_getHead(parent), a);
                } // TODO tuple
            }
        }
    } else { // That's not the last cell

        // Find the next cell
        unsigned jump = cell_getJumpNext(chain);
        Address next;
        if (jump == MAX_JUMP)
            next = cell_getRef1(chain);
        else
            next = jumpToNext(chain, current, hChild, a);

        // Free the whole cell
        chain = cell_free(chain);
        mem_set(current, chain);
        ONDEBUG((show_cell('W', current, chain, 0)));

        if (previous) {
            // Update previous chain jumper
            Cell previousCell = mem_get(previous);
            ONDEBUG((show_cell('R', previous, previousCell, 0)));
            unsigned previousJump = cell_getJumpNext(previousCell);
            if (previousJump == MAX_JUMP) {
                // the previous chain cell was already a deep link, one simply makes it pointing to next
                previousCell = refs_build(previousCell, cell_getRef0(previousCell), next, MAX_JUMP);
                mem_set(previous, previousCell);
                ONDEBUG((show_cell('W', previous, previousCell, 0)));
            } else {
                // one adds the jump amount of the removed cell to the previous cell jumper
                // note: don't forget to jump over the removed cell itself (+1)
                previousJump += 1 + jump; // note "jump" might be at max here
                if (previousJump < MAX_JUMP) {
                    // an easy one at last
                    previousCell = cell_chain(previousCell, previousJump);
                    mem_set(previous, previousCell);
                    ONDEBUG((show_cell('W', previous, previousCell, 0)));
                } else {
                    // It starts to be tricky, one has to put a deep link into the previous cell.
                    // And as one overwrites a child back-reference, one connects this child again. :(
                    Arrow child1 = cell_getRef1(previousCell);
                    previousCell = refs_build(previousCell, cell_getRef0(previousCell), next, MAX_JUMP);
                    mem_set(previous, previousCell);
                    ONDEBUG((show_cell('W', previous, previousCell, 0)));
                    if (child1)
                        connect(a, child1); // Q:may it connect 'a' to its parents twice? A: no. It's ok.
                }
            }
        } else {
            // This is the first cell of the children cells chain
            // one adds the jump amount to a.child0
            unsigned child0 = cell_getChild0(parent) + jump + 1; // don't forget + 1 to jump over freed cell
            if (child0 < MAX_CHILD0) { // an easy one: one updates child0
                parent = cell_chainChild0(parent, child0);
                mem_set(a, parent);
                ONDEBUG((show_cell('W', a, parent, 0)));
            } else {
                // One reuses the freed cell and convert it into a "deep link" cell
                chain = refs_build(chain, 0, next, MAX_JUMP);
                mem_set(current, chain);
                ONDEBUG((show_cell('W', current, chain, 0)));
                // Note: no need to touch "a" def. Already pointing the cell.
            }
        }
    }
}

void xl_childrenOfCB(Arrow a, XLCallBack cb, Arrow context) {
    TRACEPRINTF("xl_childrenOf a=%06x", a);

    if (a == EVE) {
        return; // Eve connectivity not traced
    }

    Arrow child;
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell)) return; // invalid ID
    // TODO one might call cb with a

    Cell catBits = cell_getCatBits(cell);

    // compute hashChild
    CellData data = (catBits == CATBITS_PAIR || catBits == CATBITS_SMALL)
            ? cell_getSmall(cell)
            : cell_getChecksum(cell);

    uint32_t hChild = hashChild(a, data) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    Address current = jumpToChild0(cell, a, hChild, a);
    if (!current) { LOCK_END(); return; } // no child
    cell = mem_get(current);
    ONDEBUG((show_cell('R', current, cell, 0)));
    // For every cell but last
    while (cell_getCatBits(cell) != CATBITS_LAST) {
        child = cell_getRef0(cell);
        LOCK_END();
        if (child && cb(child, context)) {
            return;
        }

        if (cell_getJumpNext(cell) == MAX_JUMP) {
            // jump == MAX ==> ref1 points to next
            current = cell_getRef1(cell);
        } else {
            child = cell_getRef1(cell);
            if (child && cb(child, context)) {
               return;
            }
            current = jumpToNext(cell, current, hChild, a);
        }
        LOCK();
        cell = mem_get(current);
        ONDEBUG((show_cell('R', current, cell, 0)));
    }
    LOCK_END();
    // Last cell
    child = cell_getRef0(cell);
    if (child && cb(child, context)) return;
    child = cell_getRef1(cell);
    if (child && cb(child, context)) return;
}

typedef struct iterator_s {
    int type;
    uint32_t hash;
    Arrow parent;
    Arrow current;
    Address pos;
    int rank;
    uint16_t stamp;
} iterator_t;

static int xl_enumNextChildOf(XLEnum e) {
    iterator_t *iteratorp = e;

    // iterate from current location stored in childrenOf iterator
    Arrow a = iteratorp->parent;
    uint32_t hChild = iteratorp->hash;
    Address pos = iteratorp->pos;
    int rank = iteratorp->rank;
    uint16_t stamp = iteratorp->stamp;
    uint16_t stamp2;
    Cell cell;
    Arrow child;
    LOCK();

    // TODO it's possible to avoir this stamp thing in a simple way:
    // just xls_set(a(atom("iterator"),natom(sizeof(void*),iterator)),current);
    // in case of commit(), this will prevent the current child to be forgotten (as it's connected via the iterator)
    // and its parent as well, obviously. So the current cell can't be altered. 
    // - xls_unset() at iterator free
    // - house-cleaning at server reboot ?
    
    if (pos == EVE) { // First call to "next" for this enumeration
        cell = mem_get_advanced(a, &stamp2);
        ONDEBUG((show_cell('R', a, cell, 0)));
        if (stamp2 != stamp) {
            return LOCK_OUT(0); // enum broken
        }

        pos = jumpToChild0(cell, a, hChild, a);
        if (!pos) {
            return LOCK_OUT(0); // no child
        }
    } else {
        cell = mem_get_advanced(pos, &stamp2);
        ONDEBUG((show_cell('R', pos, cell, 0)));
        if (stamp2 != stamp) { // enum broken
            iteratorp->current = EVE;
            return LOCK_OUT(0); // no child
        }

        if (!rank) {
            if (cell_getCatBits(cell) != CATBITS_LAST && cell_getJumpNext(cell) == MAX_JUMP) {
                // jump == MAX ==> ref1 points to next
                pos = cell_getRef1(cell);
            } else {
                Arrow child = cell_getRef1(cell);
                if (child) {
                    iteratorp->rank = 1;
                    iteratorp->current = child;
                    return LOCK_OUT(!0);
                }
                if (cell_getCatBits(cell) == CATBITS_LAST) {
                    return LOCK_OUT(0); // No more child
                }
                pos = jumpToNext(cell, pos, hChild, a);
            }
        } else {
            if (cell_getCatBits(cell) == CATBITS_LAST) {
                return LOCK_OUT(0); // No more child
            }
            pos = jumpToNext(cell, pos, hChild, a);
            rank = 0;
        }
    }
    cell = mem_get_advanced(pos, &stamp2);
    ONDEBUG((show_cell('R', pos, cell, 0)));

    // For every cell but last
    while (cell_getCatBits(cell) != CATBITS_LAST) {
        child = cell_getRef0(cell);
        if (child) {
            iteratorp->pos = pos;
            iteratorp->rank = 0;
            iteratorp->stamp = stamp2;
            iteratorp->current = child;
            return LOCK_OUT(!0);
        }
        if (cell_getJumpNext(cell) == MAX_JUMP) {
            // jump == MAX ==> ref1 points to next
            pos = cell_getRef1(cell);
        } else {
            child = cell_getRef1(cell);
            if (child) {
                iteratorp->pos = pos;
                iteratorp->rank = 1;
                iteratorp->stamp = stamp2;
                iteratorp->current = child;
                return LOCK_OUT(!0);
            }
            pos = jumpToNext(cell, pos, hChild, a);
        }
        cell = mem_get_advanced(pos, &stamp2);
        ONDEBUG((show_cell('R', pos, cell, 0)));
    }

    // Last cell
    iteratorp->pos = pos;
    iteratorp->stamp = stamp2;
    child = cell_getRef0(cell);

    if (child) {
        iteratorp->rank = 0;
        iteratorp->current = child;
        return LOCK_OUT(!0);
    }

    iteratorp->rank = 1;
    child = cell_getRef1(cell);
    if (child) {
        iteratorp->current = child;
        return LOCK_OUT(!0);
    } else {
        iteratorp->current = EVE;
        return LOCK_OUT(0); // last iteration
    }
}

int xl_enumNext(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    if (iteratorp->type == 0) {
        return xl_enumNextChildOf(e);
    }
}

Arrow xl_enumGet(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    if (iteratorp->type == 0) {
        return iteratorp->current;
    }
}

void xl_freeEnum(XLEnum e) {
    free(e);
}

XLEnum xl_childrenOf(Arrow a) {
    TRACEPRINTF("xl_childrenOf a=%06x", a);

    if (a == EVE) {
        return NULL; // Eve connectivity not traced
    }
    uint16_t stamp;

    LOCK();
    Cell cell = mem_get_advanced(a, &stamp);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell)) return NULL; // invalid ID
    LOCK_END();
    Cell catBits = cell_getCatBits(cell);

    // compute hashChild
    CellData data = (catBits == CATBITS_PAIR || catBits == CATBITS_SMALL)
            ? cell_getSmall(cell)
            : cell_getChecksum(cell);

    uint32_t hChild = hashChild(a, data) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    iterator_t *iteratorp = (iterator_t *) malloc(sizeof (iterator_t));
    assert(iteratorp);
    iteratorp->type = 0; // iterator type = childrenOf
    iteratorp->hash = hChild; // hChild
    iteratorp->parent = a; // parent arrow
    iteratorp->current = EVE; // current child
    iteratorp->pos = EVE; // current cell holding child back-ref
    iteratorp->rank = 0; // position of child back-ref in cell : 0/1
    iteratorp->stamp = stamp;
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
    space_stats.root++;
    if (a == EVE) {
        return EVE; // no
    }
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell)) return LOCK_OUT(EVE);
    if (cell_isRooted(cell)) return LOCK_OUT(a);

    // If this arrow had no child before, it was loose
    int loose = (cell_getChild0(cell) ? 0 : !0);

    // change the arrow to ROOTED state
    mem_set(a, cell_setRootBit(cell, ROOTBIT_ROOTED));
    ONDEBUG((show_cell('R', a, cell_setRootBit(cell, ROOTBIT_ROOTED), 0)));

    if (loose) { // if the arrow was loose before, one now connects it to its parents
        if (cell_isPair(cell)) {
            connect(cell_getTail(cell), a);
            connect(cell_getHead(cell), a);
        }
        // TODO tuple case
        // (try to) remove 'a' from the loose log
        looseStackRemove(a);
    }
    return LOCK_OUT(a);
}

/** unroot a rooted arrow */
Arrow xl_unroot(Arrow a) {
    space_stats.unroot++;

    if (a == EVE)
        return EVE; // stop dreaming!

    LOCK();

    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    if (!cell_isArrow(cell))
        return LOCK_OUT(EVE);
    if (!cell_isRooted(cell))
        return LOCK_OUT(a);

    // change the arrow to UNROOTED state
    cell = cell_setRootBit(cell, ROOTBIT_UNROOTED);
    mem_set(a, cell);
    ONDEBUG((show_cell('W', a, cell, 0)));

    // If this arrow has no child, it's now loose
    int loose = (cell_getChild0(cell) == 0);
    if (loose) {
        // loose log
        looseStackAdd(a);

        // If it's loose, one disconnects it from its parents.
        if (cell_isPair(cell)) {
            disconnect(cell_getTail(cell), a);
            disconnect(cell_getHead(cell), a);
        } // TODO Tuple case
    }
    return LOCK_OUT(a);
}

/** return the root status */
int xl_isRooted(Arrow a) {
    if (a == EVE) return EVE;

    LOCK();

    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    LOCK_END();
    
    assert(cell_isArrow(cell));
    if (!cell_isArrow(cell)) return -1;
    return (cell_isRooted(cell) ? a : EVE);
}

/** check if an arrow is loose */
int xl_isLoose(Arrow a) {
    if (a == EVE) return 0;
    LOCK();
    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    LOCK_END();
    return (cell_isArrow(cell) && !cell_isRooted(cell) && cell_getChild0(cell) == 0);
}

int xl_equal(Arrow a, Arrow b) {
    return (a == b);
}

/** forget a loose arrow, that is actually remove it from the main memory */
static void forget(Arrow a, uint64_t hash) {
    TRACEPRINTF("forget loose arrow %X hash %llX", a, hash);
    space_stats.forget++;

    Cell cell = mem_get(a);
    ONDEBUG((show_cell('R', a, cell, 0)));
    assert(cell_isArrow(cell));
    Cell catBits = cell_getCatBits(cell);
    Address hashLocation; // base address
    Address hashProbe; // probe offset
    
    if (catBits == CATBITS_TAG || catBits == CATBITS_BLOB) {
        // Free chain
        // FIXME this code doesn't erase reattachment cells :(
        Cell hChain = hashChain(a, cell) % PRIM1;
        if (!hChain) hChain = 1; // offset can't be 0
        Address next = jumpToFirst(cell, a, hChain, a);
        Cell chained_cell = mem_get(next);
        ONDEBUG((show_cell('R', next, chained_cell, 0)));
        while (cell_getCatBits(chained_cell) == CATBITS_CHAIN) {
            mem_set(next, cell_free(chained_cell));
            ONDEBUG((show_cell('W', next, cell_free(chained_cell), 0)));
            next = jumpToNext(chained_cell, next, hChain, a);
            chained_cell = mem_get(next);
            ONDEBUG((show_cell('R', next, chained_cell, 0)));
        }
        assert(cell_getCatBits(chained_cell) == CATBITS_LAST);
        mem_set(next, cell_free(chained_cell));
        ONDEBUG((show_cell('W', next, cell_free(chained_cell), 0)));
    }

    // Free definition start cell
    mem_set(a, cell_free(cell));
    ONDEBUG((show_cell('W', a, cell_free(cell), 0)));

    hashLocation = hash % PRIM0; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    /* Now decremeting "more" counters in the probing path up to the forgotten singleton
     */
    Address probeAddress = hashLocation;
    // ONDEBUG(if (probeAddress != a) DEBUGPRINTF("real location %X != open Address %X", a, probeAddress));
    while (probeAddress != a) {
        cell = mem_get(probeAddress);
        ONDEBUG((show_cell('R', probeAddress, cell, 0)));
        cell = cell_unmore(cell);
        mem_set(probeAddress, cell);
        ONDEBUG((show_cell('W', probeAddress, cell, 0)));
        growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
    }

    if (checksumCache && checksumCache[a % CHECKSUM_CACHE_SIZE].a == a) {
        checksumCache[a % CHECKSUM_CACHE_SIZE].a = 0;
    }
}

void xl_begin() {
    ACTIVITY_BEGIN();
}

void xl_commit() {
    TRACEPRINTF("xl_commit (looseStackSize = %d)", looseStackSize);
    unsigned i;
    ACTIVITY_OVER();
    WAIT_DORMANCY();
    for (i = looseStackSize; i > 0; i--) { // loose stack scanning
        Arrow a = looseStack[i - 1].a;
        if (xl_isLoose(a)) { // a loose arrow is removed NOW
            forget(a, looseStack[i - 1].checksum);
        }
    }
    mem_commit();

    LOGPRINTF(LOG_WARN, "xl_commit done, looseStackSize=%d get=%d root=%d unroot=%d new=%d (pair=%d atom=%d) found=%d connect=%d, disconnect=%d forget=%d",
        looseStackSize, space_stats.get, space_stats.root,
        space_stats.unroot, space_stats.new, 
        space_stats.pair, space_stats.atom, space_stats.found,
        space_stats.connect, space_stats.disconnect, space_stats.forget);
    space_stats = space_stats_zero;

    zeroalloc((char**) &looseStack, &looseStackMax, &looseStackSize);

    LOCK_END();
    ACTIVITY_BEGIN();
}

void xl_over() {
    ACTIVITY_OVER();
}

/** printf extension for arrow (%O specifier) */
static int printf_arrow_extension(FILE *stream,
        const struct printf_info *info,
        const void *const *args) {
    static char* nilFakeURI = "(NIL)";
    Arrow arrow = *((Arrow*) (args[0]));
    char* uri = arrow != NIL ? xl_uriOf(arrow, NULL) : nilFakeURI;
    if (uri == NULL)
        uri = nilFakeURI;
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

    pthread_cond_init(&api_now_active, NULL);
    pthread_cond_init(&api_now_dormant, NULL);

    pthread_mutexattr_init(&api_mutex_attr);
    pthread_mutexattr_settype(&api_mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&api_mutex, &api_mutex_attr);

    int rc = mem_init();
    if (rc < 0) { // problem
       return rc;
    }

    // register a printf extension for arrow (glibc only!)
    register_printf_specifier('O', printf_arrow_extension, printf_arrow_arginfo_size);
    
    if (rc) { // very first start
        // Eve
        Cell cellEve = pair_build(0, EVE, EVE);
        cellEve = cell_setRootBit(cellEve, ROOTBIT_ROOTED);
        mem_set(EVE, cellEve);
        ONDEBUG((show_cell('W', EVE, cellEve, 0)));
        mem_commit();
    }
    
    geoalloc((char**) &looseStack, &looseStackMax, &looseStackSize, sizeof (struct s_loose), 0);
    return rc;
}

void xl_destroy() {
    // TODO complete
    free(looseStack);
    
    pthread_cond_destroy(&api_now_active);
    pthread_cond_destroy(&api_now_dormant);
    pthread_mutexattr_destroy(&api_mutex_attr);
    pthread_mutex_destroy(&api_mutex);

    mem_destroy();
}
// GLOBAL TODO: SMALL
