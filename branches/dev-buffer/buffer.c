/* TODO List
* buffer.h
 * any new arrow: set spaceId = 0
 * one "resolves"  a buffered arrow (bind it to its server counterpart) on demand. That is,
   * at first call of buffer_isRooted()
   * or at first call of buffer_childrenOf()
 * detect the first call of buffer_childrenOf()
   * call_xlChildrenOf()
   * "pull" each child on buffer_next
   * buffer_pull(serverId)
 * pull ends when new cell types : PLACEHOLDER; INCOMING_PLACEHOLDER; OUTGOING_...
 * Placeholders are created for every unknown pair pulled from space
   * typically when retrieving the spatial children list
   * or browsing the ancestors or retrieved arrows.
 * 1st call to buffer_children retrieve the server children list (xl_children)
   Subsequent calls compare the children list revision between buffer and server. 
   If different revisions, fetch again the server list.
   the children iterator not being atomic, one may have new children fetched before
   the iterator ended. It's important to store the children revision number before iterating
 * when pulling a server children list, one call "pull(spaceId)" for each retrieved arrow 
 * buffer can be reserved/freed as a whole
 * buffer is zeroed after usage.
 * buffer_sync: ~~resolve + fetch~~ reset server root flag and children list for one arrow/every arrow =~ empty cache / force cache refresh
 * blob in separate directories (/var/tmp)
 * blob are lazy loaded!!!!!!!
 * space children retrieval algorithm if based on a buffer_pull() fonction
 * pulling = loading a space arrow known by its spaceId into the buffer
 * buffer_pull(spaceId) =
   * read hash=xl_hashOf(spaceId)
   * look for hash-indexed arrows in the buffer bank
   * if the hash matches to local unresolved arrows,
   * resolve these local arrow(s) first,
   * so to compare the spaceId and avoid duping with local copies,
   * if no local copy then eventually retrieve the pulled arrow definition
   * if the arrow is a pair,
     * create a placeholder containing a spaceId as one of its ends.
     * Outgoing placeholders got buffer Id for tail and space Id for heads 
     * Incoming placeholders got buffer Id for heads, space Id for tails
   * if the arrow is a small/tag atom, retrieve its full definition
   * if the space arrow is a blob, only retrieve its footprint,
   * raw data is pulled on buffer on demand
   * buffer_tailOf (resp. buffer_headOf) will convert a placeholder into a regular pair
      * if cell.arrow.type == OUTGOING_PLACEHOLDER || ...PLACEHOLDER
      * cell.arrow.tailId  = buffer_pull(cell.arrow.tailId)
      * if ...PLACEHOLDER { cell.arrow.headId = .... }
      * cell.arrow.type = PAIR
   * when assimlating a buffered pair, one probes for an existing singleton as usual,
   * but ...
     * if the buffered pair spaceId is not nil (happens if one end is nil)
     * and if one finds a checksum-matching placeholder pair while probing,
     * then one resolves the bufferer pair (call spaceId = xl_pair(buffer_resolve(tail), buffer_resolve(head))
     * if both spaceId match, its a probing hit 
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
#include "ram.h"
#include "sha1.h"
#define LOG_CURRENT LOG_BUFFER
#include "log.h"


/** The change log.
 This is a log of all changes made in buffered arrows space. It grows geometrically (see geoalloc() function).
 */
static Arrow* changeLog = NULL; ///< dynamically allocated
static size_t   changeLogMax = 0; ///< heap allocated changeLog size
static size_t   changeLogSize = 0; ///< significative changeLog size

static int changeLogAdd(Arrow a) {
  geoalloc((char **)&changeLog, &changeLogMax, &changeLogSize, sizeof(Arrow), changeLogSize + 1);
  changeLog[changeLogSize - 1] = a; 
}

static Arrow resolve(Arrow a, int ifAny) {
  Cell cell;
  if (a == EVE) return EVE;

  ram_get(a, (RamCell *)&cell);
  ONDEBUG((SHOWCELL('R', a, &cell)));
  Arrow spaceId = cell.arrow.spaceId;
  if (spaceId && (spaceId != NIL || ifAny)) {
      return spaceId;
  }

  if (cell.full.type == CELLTYPE_PAIR) {
      Arrow t = resolve(cell.pair.tail, ifAny);
      Arrow h = resolve(cell.pair.head, ifAny);
      if (t != NIL && h != NIL) {
          spaceId = ifAny ? xl_pairIfAny(t, h) : xl_pair(t, h);
      } else {
          spaceId = NIL;
      }
  } else {
    uint32_t lengthP;
    char* p = buffer_memOf(a, &lengthP);
    if (p) {
        spaceId = ifAny ? xl_atomnMaybe(lengthP, p) : xl_atomn(lengthP, p);
    } else {
      spaceId = NIL;
    }
  }
  cell.arrow.spaceId = spaceId;
  ram_set(a, (RamCell *)&cell);
  ONDEBUG((SHOWCELL('W', a, &cell)));
  return spaceId;
}

static int changeLogFlush() {
  int i;
    // TODO: optimize
  for (i = 0; i < changeLogSize; i++) {
    Arrow a = changeLog[i];
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      continue;
  
    if (cell.arrow.mutables & FLAGS_ROOTSETTED) {

      if (cell.arrow.mutables & FLAGS_ROOTED) {
        Arrow A = resolve(a, 0);
        xl_root(A);
      } else {
        Arrow A = resolve(a, 1);
        if (A && A != NIL) {
          xl_unroot(A);
        }
      }

      cell.arrow.spaceId  = A;
      cell.arrow.mutables ^= FLAGS_ROOTSETTED;
      ram_set(a, (RamCell *)&cell);
      ONDEBUG((SHOWCELL('W', a, &cell)));
    }
  }
  xl_commit();

  zeroalloc((char **)&changeLog, &changeLogMax, &changeLogSize);
}


/** statistic structure and variables
 */
static struct s_buffer_stats {
  // operation and other events counters
  int get; //< GET op counter
  int root; //< root op counter
  int unroot; //< unroot op counter
  int new; //< new arrow event counter
  int found; //< found arrow event counter
  int connect; //< connect op counter
  int disconnect; //< disconnect op counter
  int atom; //< atom creation counter
  int pair; //< pair cration counter
  int forget; //< forget op counter
} buffer_stats_zero = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}, buffer_stats = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/** Things to make the API parallelizable 
 */
static pthread_mutexattr_t apiMutexAttr;
static pthread_mutex_t apiMutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() pthread_mutex_lock(&apiMutex)
#define LOCK_END() pthread_mutex_unlock(&apiMutex)
#define LOCK_OUT(X) ((Arrow)pthread_mutex_unlock(&apiMutex), (X))
#define LOCK_OUT64(X) ((uint64_t)pthread_mutex_unlock(&apiMutex), (X))
#define LOCK_OUTSTR(X) ((char*)pthread_mutex_unlock(&apiMutex), (X))


static pthread_cond_t apiNowDormant;   // Signaled when all thread are ready to commit
static pthread_cond_t apiNowActive;    // Signaled once a thread has finished commit
static int apiActivity = 0; // Activity counter; Incremented/Decremented by buffer_begin/buffer_over. Decremented when waiting for commit. Incremented again after.
static int bufferGCNeeded = 0; // 0->1 when a thread is wait for GC, 1->0 when GC done


#define ACTIVITY_BEGIN() (!apiActivity++ ? (void)pthread_cond_signal(&apiNowActive) : (void)0)
#define ACTIVITY_OVER() (apiActivity ? (--apiActivity ? (void)0 : (void)pthread_cond_signal(&apiNowDormant)) : (void)0)
#define WAIT_DORMANCY() (apiActivity > 0 ? (void)pthread_cond_wait(&apiNowDormant, &apiMutex) : (void)0)

/*
 * Size limit from where data is stored as "blob" or "tag" or "small"
 */
#define TAG_MINSIZE 12
#define BLOB_MINSIZE 100

/*
 * Eve
 */
#define EVE (0)
const Arrow Eve = EVE;
#define EVE_HASH (0x8421A593U) 


/*
 *   RAM only memory device
 *
 *   -------+------+------+------+-------
 *      ... | cell | cell | cell | ...
 *   -------+------+------+------+-------
 *
 *   cell: a RAM cell
 */


/**  Cell structure (28 bytes).
 *
 *   +---------------------------------------+---+---+
 *   |                  data                 | T | P |
 *   +---------------------------------------+---+---+
 *                       26                    1   1
 *
 *      P: "Peeble" count aka "More" counter (8 bits)
 *       T: Cell type ID (8 bits)
 *    Data: Data (26 bytes)
 *
 *
 */
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)     /* set alignment to 1 byte boundary */

typedef union u_cell {
  struct u_full {

    char data[26];
    unsigned char peeble;
    unsigned char type;
  } full;

#define PEEBLE_MAX 0xFFu

/*
 * Cell content type
 */
#define CELLTYPE_EMPTY  0
#define CELLTYPE_PAIR   1
#define CELLTYPE_SMALL  2
#define CELLTYPE_TAG    3
#define CELLTYPE_BLOB   4
#define CELLTYPE_ARROWLIMIT CELLTYPE_BLOB
#define CELLTYPE_SLICE  5
#define CELLTYPE_LAST   6
#define CELLTYPE_SYNC   7
#define CELLTYPE_REATTACHMENT 7
#define CELLTYPE_CHILDREN 8
#define CELLTYPE_OUTGOING_PLACEHOLDER 16
#define CELLTYPE_INCOMING_PLACEHOLDER 17
#define CELLTYPE_PLACEHOLDER 18

  /*
   * raw
  */
  struct s_uint {
    uint32_t data[7];
  } uint;

  /*   T = 0: empty cell.
   *   +---------------------------------------------------+
   *   |                      junk                         |
   *   +---------------------------------------------------+
   *                         26 bytes
   */

  /** T = 1, 2, 3, 4: arrow definition.
  *   +--------+--------+----------------------+-----------+----------+----+----+
  *   | arrowID|  hash  | full or partial def  |  mutables |  Child0  | cr | dr |
  *   +--------+--------+----------------------+-----------+----------+----+----+
  *        4       4                8                4          4        1    1
  *   where mutables = R|W|C0d|wS|Wn|rS|Cn
  *
  *   R = root flag (1 bit)
  *   W = weak flag (1 bit)
  *   rS = root setted flag (1 bit)
  *   wS = weak setted flag (1 bit) 
  *   C0d = child0 direction (1 bit)
  *   Wn = Weak children count (14 bits)
  *   Cn = Non-weak children count  (15 bits)
  *   
  *   child0 = 1st child of this arrow
  *   cr = children revision
  *   dr = definition revision
  */
  struct s_arrow {
     Arrow spaceId;
     uint32_t hash;
     char  def[8];
     uint32_t mutables;
     uint32_t child0;
     unsigned char cr;
     unsigned char dr;
  } arrow;

/** mutables flags.
 */
#define FLAGS_ROOTED           0x80000000u
#define FLAGS_WEAK             0x40000000u
#define FLAGS_C0D              0x20000000u
#define FLAGS_WEAKCHILDRENMASK 0x0FFF8000u
#define MAX_WEAKREFCOUNT       0x0FFFu
#define FLAGS_CHILDRENMASK     0X00003FFFu
#define MAX_REFCOUNT           0x3FFFu
#define FLAGS_WEAKSETTED       0x10000000u
#define FLAGS_ROOTSETTED       0x00004000u
  /* T = 1: regular pair.
  *  T = 16, 17, 18: pair with one or both unloaded end(s), aka a placeholder
  *   +--------+--------+--------+--------+-----------+----------+----+----+
  *   | arrowID|  hash  |  tail  |  head  |  mutables |  Child0  | cr | dr |
  *   +--------+--------+--------+--------+-----------+----------+----+----+
  *        4       4        4        4         4            4       1    1
  */
  struct s_pair {
     uint32_t spaceId;
     uint32_t hash;
     uint32_t tail;
     uint32_t head;
     uint32_t mutables;
     uint32_t child0;
     unsigned char cr;
     unsigned char dr;
  } pair;

  /* T = 2: small.
  *   +--------+---+-------+-------------------+------------+----------+----+----+
  *   | arrowID| s | hash3 |        data       |  mutables  |  Child0  | cr | dr |
  *   +--------+---+-------+-------------------+------------+----------+----+----+
  *        4     1    3               8      
  *            <---hash--->
  *
  *   s : small size (0 < s <= 11)
  *   hash3: (data 1st word ^ 2d word ^ 3d word) & 0xFFFFFFu
  *    ... if s > 8, one can get 3 additional bytes, by computing:(1st word ^ 2d word ^ hash) & 0xFFFFFFu
  */
  struct s_small {
    uint32_t spaceId;
    unsigned char s;
    char hash3[3];
    char data[8];
    uint32_t mutables;
    uint32_t child0;
    unsigned char cr;
    unsigned char dr;
  } small;
     
  /* T = 3: tag or 4: blob footprint.
  *   +--------+--------+------------+----+-----------+----------+----+----+
  *   | arrowID|  hash  |   slice0   | J0 |  mutables |  Child0  | cr | dr |
  *   +--------+--------+------------+----+-----------+----------+----+----+
  *        4       4          7        1                     
  *   J = first slice jump, h-sequence multiplier (1 byte)
  */
  struct s_tagOrBlob {
    uint32_t spaceId;
    uint32_t hash;
    char slice0[7];
    unsigned char jump0;
    uint32_t mutables;
    uint32_t child0;
    unsigned char cr;
    unsigned char dr;
  } tagOrBlob;

#define MAX_JUMP0 0xFFu

  /*   T = 5: slice of binary string (tag/blob)
  *   +-----------------------------------------+---+
  *   |                   data                  | J |
  *   +-----------------------------------------+---+
  *                        25                     1
  */
  struct s_slice {
    char data[25];
    unsigned char jump;
  } slice;

#define MAX_JUMP 0xFFu

  /*   T = 6: last slice of binary string
  *   +-----------------------------------------+---+
  *   |                   data                  | s |
  *   +-----------------------------------------+---+
  *                        25                     1
  *   s = slice size
  */
  struct s_last {
    char data[25];
    unsigned char size;
  } last;

  /*   T = 7: sequence reattachment
   *   +--------+--------+--------------+
   *   |  from  |   to   |  ...         |
   *   +--------+--------+--------------+
   *       4        4          
   */
  struct s_reattachment {
    uint32_t from;
    uint32_t to;
    char junk[18];
  } reattachment;

  /*   T = 8: children with same hash
   *   +------+------+------+------+------+------+---+
   *   |  C5  |  C4  |  C3  |  C2  |  C1  |  C0  | td|
   *   +------+------+------+------+------+------+---+
   *       4      4     4      4      4       4    2
   *   Ci : Child or list terminator
   *        (list terminator = parent address with flag)
   *   td: terminators (1 byte) . directions (1 byte, 1 outgoing)
   *       6 significative bits per byte
   *   
   */
  struct s_children {
    uint32_t C[6];
    uint16_t directions;
  } children;

} Cell;

#pragma pack(pop)   /* restore original alignment from stack */

// The loose log.
// This is a dynamic array containing all arrows in loose state.
// it grows geometrically.
static Arrow  *looseLog = NULL;
static uint32_t looseLogMax = 0;
static uint32_t looseLogSize = 0;


static void cell_getSmallPayload(Cell *cell, char* buffer) {
    int bigSmall = cell->small.s > 8;
    memcpy(buffer, cell->small.data, bigSmall ? 8 : cell->small.s);
    if (bigSmall) {
        buffer[8] = cell->small.hash3[0] ^ cell->small.data[1] ^ cell->small.data[5];
        buffer[9] = cell->small.hash3[1] ^ cell->small.data[2] ^ cell->small.data[6];
        buffer[10] = cell->small.hash3[2] ^ cell->small.data[3] ^ cell->small.data[7];
    }

}

/** log cell access.
* operation = R/W
* address
* cell pointer
*/
static void showCell(int line, char operation, Address address, Cell* cell) {
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
    unsigned peeble = (unsigned)cell->full.peeble;
    const char* cat = cats[type];
    if (type == CELLTYPE_EMPTY) {
        dputs("%d %c %06x EMPTY (peeble=%02x)",
          line, operation, address, peeble);
        return;

    }
    if (type <= CELLTYPE_ARROWLIMIT) {
      char flags[] = {
          (cell->arrow.mutables & FLAGS_ROOTED ? 'R' : '.'),
          (cell->arrow.mutables & FLAGS_WEAK ? 'W' : '.'),
          (cell->arrow.mutables & FLAGS_C0D ? 'O' : 'I'),
          '\0'
      };
      uint32_t hash = cell->arrow.hash;
      int weakChildrenCount = (int)((cell->arrow.mutables & FLAGS_WEAKCHILDRENMASK) >> 15);
      int childrenCount = (int)(cell->arrow.mutables & FLAGS_CHILDRENMASK);
      int cr = (int)cell->arrow.cr;
      int dr = (int)cell->arrow.dr;
      int spaceId = (int)cell->arrow.spaceId;
      if (type == CELLTYPE_PAIR) {
        dputs("%d %c %06x id=%06x peeble=%02x type=%1x hash=%08x Flags=%s weakCount=%04x refCount=%04x child0=%08x cr=%02x dr=%02x %s tail=%08x head=%08x",
          line, operation, address, spaceId, peeble, type,
          hash, flags, weakChildrenCount, childrenCount, cell->arrow.child0, cr, dr, cat,
          cell->pair.tail,
          cell->pair.head);

      } else if (type == CELLTYPE_SMALL) {
        int s = (int)cell->small.s;
        char buffer[11];
        cell_getSmallPayload(cell, buffer);
        dputs("%d %c %06x id=%06x peeble=%02x type=%1x hash=%08x Flags=%s weakCount=%04x refCount=%04x child0=%08x cr=%02x dr=%02x %s size=%d data=%.*s",
          line, operation, address, spaceId, peeble, type,
          hash, flags, weakChildrenCount, childrenCount, cell->arrow.child0, cr, dr, cat,
          s, s, buffer);

      } else if (type == CELLTYPE_BLOB || type == CELLTYPE_TAG) {
        dputs("%d %c %06x id=%06x peeble=%02x type=%1x hash=%08x Flags=%s weakCount=%04x refCount=%04x child0=%08x cr=%02x dr=%02x %s jump0=%1x slice0=%.7s",
          line, operation, address, spaceId, peeble, type,
          hash, flags, weakChildrenCount, childrenCount, cell->arrow.child0, cr, dr, cat,
          (int)cell->tagOrBlob.jump0, cell->tagOrBlob.slice0);

      }
    } else if (type == CELLTYPE_SLICE) {
      dputs("%d %c %06x peeble=%02x type=%1x %s jump=%1x data=%.21s",
        line, operation, address, peeble, type, cat,
        (int)cell->slice.jump, cell->slice.data);

    } else if (type == CELLTYPE_LAST) {
      dputs("%d %c %06x peeble=%02x type=%1x %s size=%d data=%.*s",
        line, operation, address, peeble, type, cat,
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
          (cell->children.directions & 0x20 ? 'O' : '.'),
          (cell->children.directions & 0x0100 ? 'T' : '.'),
          (cell->children.directions & 0x0200 ? 'T' : '.'),
          (cell->children.directions & 0x0400 ? 'T' : '.'),
          (cell->children.directions & 0x0800 ? 'T' : '.'),
          (cell->children.directions & 0x1000 ? 'T' : '.'),
          (cell->children.directions & 0x2000 ? 'T' : '.'),
          ']',
          '\0'
      };
      dputs("%d %c %06x peeble=%02x type=%1x %s C[]={%x %x %x %x %x %x} D=%s",
        line, operation, address, peeble, type, cat,
        cell->children.C[0],
        cell->children.C[1],
        cell->children.C[2],
        cell->children.C[3],
        cell->children.C[4],
        cell->children.C[5],
        directions);

    } else if (type == CELLTYPE_REATTACHMENT) {
      dputs("%d %c %06x peeble=%02x type=%1x %s from=%x to=%x",
        line, operation, address, peeble, type, cat,
        (int)cell->reattachment.from, cell->reattachment.to);

    } else {
      dputs("%d %c %06x peeble=%02x type=%1x ANOMALY",
        line, operation, address, peeble, type);
      assert(0 == 1);
    }
}
#define SHOWCELL(operation, address, cell) showCell(__LINE__, operation, address, cell)

static void looseLogAdd(Address a) {
    geoalloc((char**) &looseLog, &looseLogMax, &looseLogSize, sizeof (Arrow), looseLogSize + 1);
    looseLog[looseLogSize - 1] = a;
}

static void looseLogRemove(Address a) {
    assert(looseLogSize);
    // if the arrow is at the log end, then one reduces the log
    if (looseLog[looseLogSize - 1] == a)
      looseLogSize--;
    // else ... nothing is done.
}


/* ________________________________________
 * Hash functions
 * One computes 3 kinds of hashcode.
 * * hashAddress: default cell address of an arrow.
 *     hashAddress = hashArrow(tail, head) % PRIM0 for a regular arrow
 *     hashAddress = hashString(string) for a tag arrow
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
   return PRIM1 + (tail << 20) + (head << 4) + tail + head; // (tail << 20) ^ (tail >> 4) ^ head;
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
    hash = (hash ^ (hash >> 32) ^ (hash << 32));
    TRACEPRINTF("hashRaw(%d, \"%.*s\") = %016llx", length, length, buffer, hash);

    return hash;
}

/* hash function to get hashChain from a tag or blob containing cell */
uint32_t hashChain(Cell* cell) {
    // This hash mixes the cell content
    uint32_t inverted = cell->arrow.hash >> 16 & cell->arrow.hash << 16;
    if (cell->full.type == CELLTYPE_BLOB || cell->full.type == CELLTYPE_TAG)
        inverted = inverted ^ cell->uint.data[1] ^ cell->uint.data[2];
    return inverted;
}

/* hash function to get hashChild from a cell caracteristics */
uint32_t hashChild(Cell* cell) {
    return hashChain(cell) ^ 0xFFFFFFFFu;
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


/* MACROS
 * ====== */

/* Address shifting */
#define SHIFT_LIMIT 20
#define PROBE_LIMIT 20
#define ADDRESS_SHIFT(ADDRESS, NEW, OFFSET) \
         (NEW = (((ADDRESS) + (31 + (OFFSET)++) / 32) % (RAM_SIZE)))

#define ADDRESS_JUMP(ADDRESS, NEW, OFFSET, JUMP) \
         (NEW = (((ADDRESS) + (31 * (JUMP) + ((OFFSET) * (JUMP) * (1 + JUMP) / 2)) / 32) % (RAM_SIZE)))

/* Cell testing */
#define CELL_CONTAINS_ARROW(CELL) ((CELL).full.type != CELLTYPE_EMPTY && (CELL).full.type <= CELLTYPE_ARROWLIMIT)
#define CELL_CONTAINS_ATOM(CELL) ((CELL).full.type == CELLTYPE_BLOB || (CELL).full.type <= CELLTYPE_TAG)

/** function used to go to the first cell in a chain
 */
static Address jumpToFirst(Cell* cell, Address address, Address offset) {
    Address next = address;
    assert(CELL_CONTAINS_ATOM(*cell));

    int jump = cell->tagOrBlob.jump0 + 1; // Note: jump=1 means 2 shifts

    ADDRESS_JUMP(address, next, offset, jump);

    if (jump == MAX_JUMP0) {
        // one needs to look for a reattachment (sync) cell
        Cell probed;
        ram_get(next, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', next, &probed)));

        int i = PROBE_LIMIT;
        while (--i && !(
                probed.full.type == CELLTYPE_REATTACHMENT
                && probed.reattachment.from == address)) {
            ADDRESS_SHIFT(next, next, offset);
            ram_get(next, (RamCell *)&probed);
            ONDEBUG((SHOWCELL('R', next, &probed)));
        }
        assert(i);

        next = probed.reattachment.to;
    }
    return next;
}

static Address jumpToNext(Cell* cell, Address address, Address offset) {
    Address next = address;
    assert(cell->full.type == CELLTYPE_SLICE);

    uint32_t jump = cell->slice.jump + 1; // Note: jump == 1 means 2 shifts

    ADDRESS_JUMP(address, next, offset, jump);

    if (jump == MAX_JUMP) {
        // one needs to look for a reattachment (sync) cell
        Cell probed;
        ram_get(next, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', next, &probed)));

        int i = PROBE_LIMIT;
        while (--i && !(
                probed.full.type == CELLTYPE_REATTACHMENT
                && probed.reattachment.from == address)) {
            ADDRESS_SHIFT(next, next, offset);
            ram_get(next, (RamCell *)&probed);
            ONDEBUG((SHOWCELL('R', next, &probed)));
        }
        assert(i);

        next = probed.reattachment.to;
    }
    return next;
}

/** return Eve */
Arrow buffer_Eve() {
    return EVE;
}

/** retrieve the arrow corresponding to data at $str with size $length and precomputed $hash.
 * Will create the singleton if missing except if $ifExist is set.
 * $str might be a blob signature or a tag content.
 */
Arrow payload(int cellType, int length, char* str, uint64_t payloadHash, int ifExist) {
    Address hashAddress, hashProbe, hChain;
    uint32_t l;
    uint32_t hash;
    Address probeAddress, firstFreeAddress, next;
    Cell probed, sliceCell;

    unsigned i, jump;
    char c, *p;

    buffer_stats.atom++;

    if (!length) {
        return EVE;
    }

    hash = payloadHash & 0xFFFFFFFFU;
    hashAddress = hash % PRIM0 % RAM_SIZE;
    hashProbe = hash % PRIM1;
    if (!hashProbe) hashProbe = 1; // offset can't be 0
   
    
    // Search for an existing singleton
    probeAddress = hashAddress;
    firstFreeAddress = EVE;
    while (1) {
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));

        if (probed.full.type == CELLTYPE_EMPTY) {
            firstFreeAddress = probeAddress;

        } else if (probed.full.type == cellType
                  && probed.tagOrBlob.hash == hash
                  && !memcmp(probed.tagOrBlob.slice0, str, sizeof(probed.tagOrBlob.slice0))) {
        	// chance we found it
          // now comparing the whole string
          hChain = hashChain(&probed) % PRIM1;
          if (!hChain) hChain = 1; // offset can't be 0

          p = str + sizeof(probed.tagOrBlob.slice0);
          l = length - sizeof(probed.tagOrBlob.slice0);
          assert(l > 0);
          i = 0;

          next = jumpToFirst(&probed, probeAddress, hChain);
          ram_get(next, (RamCell *)&sliceCell);
          ONDEBUG((SHOWCELL('R', next, &sliceCell)));

          while ((c = *p++) == sliceCell.slice.data[i++] && --l) { // cmp loop
            if (i == sizeof(sliceCell.slice.data)) {
                // one shifts to the next linked cell
                if (sliceCell.full.type == CELLTYPE_LAST) {
                    break; // the chain is over
                }
                next = jumpToNext(&sliceCell, next, hChain);
                ram_get(next, (RamCell *)&sliceCell);
                ONDEBUG((SHOWCELL('R', next, &sliceCell)));
                i = 0;
            }
          } // cmp loop
          if (!l && sliceCell.full.type == CELLTYPE_LAST
             && sliceCell.last.size == i) {
            // exact match
            buffer_stats.found++;
            return probeAddress; // found arrow
          } // match
        } // if candidate
        // Not the singleton

        if (!probed.full.peeble) { // nothing further
            break; // Miss
        }

        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    if (ifExist) {
        // if one only wants to check arrow existency, then a miss is a miss
        return EVE;
    }
    // else ... one creates a corresponding arrow right now

    buffer_stats.new++;
    buffer_stats.atom++;

    if (firstFreeAddress == EVE) {
        Cell probed;

        int i = PROBE_LIMIT;
        while (--i) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);

            ram_get(probeAddress, (RamCell *)&probed);
            ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
            
            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        assert(i);
    }

   
    // Create an arrow representation

    Address current, newArrow = firstFreeAddress;
    Cell newCell, nextCell, currentCell;

    ram_get(newArrow, (RamCell *)&newCell);
    ONDEBUG((SHOWCELL('R', newArrow, &newCell)));


    /*   |  hash  | slice0  | J |  mutables |  Child0  | cr | dr |
    *       4          7        1                     
    */
    newCell.tagOrBlob.spaceId = 0;
    newCell.tagOrBlob.hash = hash;
    memcpy(newCell.tagOrBlob.slice0, str, sizeof(newCell.tagOrBlob.slice0));
    newCell.arrow.mutables = 0;
    newCell.arrow.child0 = 0;
    newCell.full.type = cellType;
    newCell.arrow.dr = buffer_stats.new & 0xFFu;

    hChain = hashChain(&newCell) % PRIM1;
    if (!hChain) hChain = 1; // offset can't be 0

    Address offset = hChain;
    ADDRESS_SHIFT(newArrow, next, offset);
    
    ram_get(next, (RamCell *)&nextCell);
    ONDEBUG((SHOWCELL('R', next, &nextCell)));

    jump = 0;
    i = PROBE_LIMIT;
    while (nextCell.full.type != CELLTYPE_EMPTY && --i) {
        jump++;
        ADDRESS_SHIFT(next, next, offset);

        ram_get(next, (RamCell *)&nextCell);
        ONDEBUG((SHOWCELL('R', next, &nextCell)));
    }
    assert(i);

    if (jump >= MAX_JUMP) {
        Address sync = next;
        Cell syncCell = nextCell; 
        syncCell.full.type = CELLTYPE_REATTACHMENT;
        syncCell.reattachment.from = newArrow;
        syncCell.reattachment.to   = newArrow; // will be overwritten as soon as possible
        
        ram_set(sync, (RamCell *)&syncCell);
        ONDEBUG((SHOWCELL('W', sync, &syncCell)));
        
        offset = hChain;
        ADDRESS_SHIFT(next, next, offset);
        
        ram_get(next, (RamCell *)&nextCell);
        ONDEBUG((SHOWCELL('R', next, &nextCell)));

        i = PROBE_LIMIT;
        while (nextCell.full.type == CELLTYPE_EMPTY && --i) {
          ADDRESS_SHIFT(next, next, offset);

          ram_get(next, (RamCell *)&nextCell);
          ONDEBUG((SHOWCELL('R', next, &nextCell)));
        }
        assert(i);

        syncCell.reattachment.to = next;
        
        ram_set(sync, (RamCell *)&syncCell);
        ONDEBUG((SHOWCELL('W', sync, &syncCell)));

        jump = MAX_JUMP;
    }

    newCell.tagOrBlob.jump0 = jump;

    ram_set(newArrow, (RamCell *)&newCell);
    ONDEBUG((SHOWCELL('W', newArrow, &newCell)));
    
    current = next;
    currentCell = nextCell;
    p = str + sizeof(newCell.tagOrBlob.slice0);
    l = length - sizeof(newCell.tagOrBlob.slice0);
    i = 0;
    c = 0;
    if (l) {
        while (1) {
            c = *p++;
            currentCell.slice.data[i] = c;
            if (!--l) break;
            if (++i == sizeof(currentCell.slice.data)) {

                Address offset = hChain;
                ADDRESS_SHIFT(current, next, offset);
    
                ram_get(next, (RamCell *)&nextCell);
                ONDEBUG((SHOWCELL('R', next, &nextCell)));
                
                jump = 0;
                i = PROBE_LIMIT;
                while (nextCell.full.type != CELLTYPE_EMPTY && --i) {
                    jump++;
                    ADDRESS_SHIFT(next, next, offset);
                
                    ram_get(next, (RamCell *)&nextCell);
                    ONDEBUG((SHOWCELL('R', next, &nextCell)));
                }
                assert(i);
                
                if (jump >= MAX_JUMP) {
                  Address sync = next;
                  Cell syncCell = nextCell; 
                  syncCell.full.type = CELLTYPE_REATTACHMENT;
                  syncCell.reattachment.from = current;
                  syncCell.reattachment.to   = current; // will be overwritten as soon as possible
                  
                  ram_set(sync, (RamCell *)&syncCell);
                  ONDEBUG((SHOWCELL('W', sync, &syncCell)));
                  
                  offset = hChain;
                  ADDRESS_SHIFT(next, next, offset);
                  
                  ram_get(next, (RamCell *)&nextCell);
                  ONDEBUG((SHOWCELL('R', next, &nextCell)));

                  int j = PROBE_LIMIT;
                  while (nextCell.full.type == CELLTYPE_EMPTY && --j) {
                    ADDRESS_SHIFT(next, next, offset);

                    ram_get(next, (RamCell *)&nextCell);
                    ONDEBUG((SHOWCELL('R', next, &nextCell)));
                  }
                  assert(j);

                  syncCell.reattachment.to = next;
                  
                  ram_set(sync, (RamCell *)&syncCell);
                  ONDEBUG((SHOWCELL('W', sync, &syncCell)));

                  jump = MAX_JUMP;
                }
                currentCell.full.type = CELLTYPE_SLICE;
                currentCell.slice.jump = jump;

                ram_set(current, (RamCell *)&currentCell);
                ONDEBUG((SHOWCELL('W', current, &currentCell)));

                i = 0;
                current = next;
                ram_get(current, (RamCell *)&currentCell);
            }

        }
    }
    currentCell.full.type = CELLTYPE_LAST;
    currentCell.last.size = 1 + i /* size */;
    ram_set(current, (RamCell *)&currentCell);
    ONDEBUG((SHOWCELL('W', current, &currentCell)));

    // Now incremeting "peeble" counters in the probing path up to the new singleton
    // important to reinitialize probing variables  
    probeAddress = /* hash % PRIM0 % RAM_SIZE */ hashAddress;
    hashProbe = hash % PRIM1;
    if (!hashProbe)
      hashProbe = 1;
    while (probeAddress != newArrow) {
        Cell probed;
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
        if (probed.full.peeble != PEEBLE_MAX)
          probed.full.peeble++;
        
        ram_set(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('W', probeAddress, &probed)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    // log loose arrow
    looseLogAdd(newArrow);
    return newArrow;
}

Arrow serverIdOf(Arrow a) {
    if (a == XL_EVE)
      return EVE;
    if (a >= RAM_SIZE)
      return NIL;
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
       || cell.full.type > CELLTYPE_ARROWLIMIT)
        return NIL; // Not an arrow
    return cell.arrow.spaceId; // current serverId
}

uint32_t hashOf(Arrow a) {
    Cell cell;
    
    if (a == XL_EVE)
      return EVE_HASH; // Eve hash is not zero!
    else if (a >= RAM_SIZE)
      return 0; // Out of buffer
    else {
      ram_get(a, (RamCell *)&cell);
      ONDEBUG((SHOWCELL('R', a, &cell)));
      if (cell.full.type == CELLTYPE_EMPTY
         || cell.full.type > CELLTYPE_ARROWLIMIT)
        return 0; // Not an arrow
      else
        return cell.arrow.hash; // hash
    }
}

uint32_t buffer_hashOf(Arrow a) {
    LOCK();
    uint32_t cs = hashOf(a);
    return LOCK_OUT(cs);
}


/** retrieve small
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
static Arrow small(int length, char* str, int ifExist) {
    DEBUGPRINTF("small(%02x %.*s %1x) begin", length, length, str, ifExist);

    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress, firstFreeAddress;
    uint32_t uint_buffer[3];
    char* buffer = (char *)uint_buffer;
    int i;

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
    
    /*| s | hash3 |        data       |  mutables  |  Child0  | cr | dr |
    *   s : small size (0 < s <= 11)
    *   hash3: (data 1st word ^ 2d word ^ 3d word) with s completion 
    */
    buffer[1] = buffer[1] ^ buffer[5] ^ buffer[9];
    buffer[2] = buffer[2] ^ buffer[6] ^ buffer[10];
    buffer[3] = buffer[3] ^ buffer[7] ^ buffer[11];

    buffer_stats.get++;

    // Compute hashs
    hash = uint_buffer[0];
    hashAddress = hash % PRIM0 % RAM_SIZE; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0
    
    // Probe for an existing singleton
    probeAddress = hashAddress;
    firstFreeAddress = EVE;

    i = PROBE_LIMIT;
    while (--i) { // probe loop
    
        // get probed cell
        Cell probed;
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));

        if (probed.full.type == CELLTYPE_EMPTY) {
            // save the address of the free cell encountered
            firstFreeAddress = probeAddress;

        } else if (probed.full.type == CELLTYPE_SMALL
                && 0 == memcmp(probed.full.data, buffer, 12)) {
            buffer_stats.found++;
            return probeAddress; // OK: arrow found!
        }
        // Not the singleton

        if (probed.full.peeble == 0) {
            break; // Probing over. It's a miss.
        }

        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }
    // Miss

    if (ifExist) // one only want to test for singleton existence in the arrows buffer
        return EVE; // Eve means not found

    buffer_stats.new++;
    buffer_stats.pair++;

    if (firstFreeAddress == EVE) {
        Cell probed;
        i = PROBE_LIMIT;
        while (--i) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
            ram_get(probeAddress, (RamCell *)&probed);
            ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        assert(i);
    }

    // Create a singleton
    Address newArrow = firstFreeAddress;
    Cell newCell;
    ram_get(newArrow, (RamCell *)&newCell);
    ONDEBUG((SHOWCELL('R', newArrow, &newCell)));

    memcpy(&newCell, buffer, 12);
    newCell.arrow.mutables = 0;
    newCell.arrow.child0 = 0;
    newCell.full.type = CELLTYPE_SMALL;
    newCell.arrow.dr = buffer_stats.new & 0xFFu;
    memcpy(newCell.full.data, buffer, 12);
    ram_set(newArrow, (RamCell *)&newCell);
    ONDEBUG((SHOWCELL('W', newArrow, &newCell)));

    /* Now incremeting "more" counters in the probing path up to the new singleton
     */
    probeAddress = hashAddress;
    hashProbe = hash % PRIM1; // reset hashProbe to default
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    while (probeAddress != newArrow) {

        // get probed cell
        Cell probed;
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
        if (probed.full.peeble != PEEBLE_MAX)
          probed.full.peeble++;
        
        ram_set(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('W', probeAddress, &probed)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    // record new arrow as loose arrow
    looseLogAdd(newArrow);
    return newArrow;
}


/** retrieve tail->head pair
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
static Arrow pair(Arrow tail, Arrow head, int ifExist) {
    uint32_t hash;
    Address hashAddress, hashProbe;
    Address probeAddress, firstFreeAddress;
    int i;

    if (tail == NIL || head == NIL)
        return NIL;
 
     // FIXME : Load both ends cells, check validity and don't use hashOf/serverIdOf

    buffer_stats.get++;

    // Compute hashs
    hash = hashPair(hashOf(tail), hashOf(head));
    hashAddress = hash % PRIM0 % RAM_SIZE; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0
    
    // Probe for an existing singleton
    Cell probed;
    probeAddress = hashAddress;
    firstFreeAddress = EVE;
    i = PROBE_LIMIT;
    while (--i) {
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
    
        if (probed.full.type == CELLTYPE_EMPTY)
            firstFreeAddress = probeAddress;
    
        else if (probed.full.type == CELLTYPE_PAIR
                && probed.pair.tail == tail
                && probed.pair.head == head
                && probeAddress /* ADAM can't be put at EVE place! TODO: optimize */) {
            buffer_stats.found++;
            return probeAddress; // OK: arrow found!
        }
        // Not the singleton

        if (probed.full.peeble == 0) {
            break; // Probing over. It's a miss.
        }

        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }
    // Miss

    if (ifExist) // one only want to test for singleton existence in the arrows buffer
        return EVE; // Eve means not found

    buffer_stats.new++;
    buffer_stats.pair++;

    if (firstFreeAddress == EVE) {
        i = PROBE_LIMIT;
        while (--i) {
            ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
        
            Cell probed;
            ram_get(probeAddress, (RamCell *)&probed);
            ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
        
            if (probed.full.type == CELLTYPE_EMPTY) {
                firstFreeAddress = probeAddress;
                break;
            }
        }
        assert(i);
    }

    // Create a singleton

    // read the free cell
    Address newArrow = firstFreeAddress;
    Cell newCell;
    ram_get(newArrow, (RamCell *)&newCell);
    ONDEBUG((SHOWCELL('R', newArrow, &newCell)));

    //
    newCell.pair.tail = tail;
    newCell.pair.head = head;
    newCell.arrow.hash = hash;
    newCell.arrow.mutables = 0;
    newCell.arrow.child0 = 0;
    newCell.full.type = CELLTYPE_PAIR;
    newCell.arrow.dr = buffer_stats.new & 0xFFu;
    
    // buffer-local arrows necessarily produces buffer-local children
    if (serverIdOf(tail) == NIL || serverIdOf(head) == NIL)
      newCell.arrow.spaceId = NIL;
    else
      newCell.arrow.spaceId = 0;
      
    ram_set(newArrow, (RamCell *)&newCell);
    ONDEBUG((SHOWCELL('W', newArrow, &newCell)));

    /* Now incremeting "peeble" counters in the probing path up to the new singleton
     */
    probeAddress = hashAddress;
    hashProbe = hash % PRIM1; // reset hashProbe to default
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    while (probeAddress != newArrow) {
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));
        if (probed.full.peeble != PEEBLE_MAX)
          probed.full.peeble++;
        
        ram_set(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('W', probeAddress, &probed)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

    // record new arrow as loose arrow
    looseLogAdd(newArrow);
    return newArrow;
}

Arrow buffer_pair(Arrow tail, Arrow head) {
    LOCK();
    Arrow a = pair(tail, head, 0);
    return LOCK_OUT(a);
}

Arrow buffer_pairMaybe(Arrow tail, Arrow head) {
    LOCK();
    Arrow a= pair(tail, head, 1);
    return LOCK_OUT(a);
}

/** retrieve a tag arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if ifExist param is set.
 */
static Arrow tag(uint32_t size, char* data, int ifExist, uint64_t hash) {
  if (!hash) hash = hashRaw(data, size);
  return payload(CELLTYPE_TAG, size, data, hash, ifExist);
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
    // - A signature, stored as a specialy typed tag in the arrows buffer.
    // - data stored separatly in some traditional filer outside the arrows buffer.
    mem0_saveData(signature, size, data);
    // TODO: remove data when cell at h is recycled.

    return payload(CELLTYPE_BLOB, signature_size, signature, hash, ifExist);
}

Arrow buffer_atom(char* str) {
    uint32_t size;
    uint64_t hash = hashStringAndGetSize(str, &size);
    LOCK();
    Arrow a =
      (size == 0 ? // EVE has a zero payload
        EVE : (size < TAG_MINSIZE
            ? small(size, str, 0)
            : (size >= BLOB_MINSIZE
              ? blob(size, str, 0)
              : tag(size, str, 0, hash))));
    return LOCK_OUT(a);
}

Arrow buffer_atomMaybe(char* str) {
    uint32_t size;
    uint64_t hash = hashStringAndGetSize(str, &size);
    LOCK();
    Arrow a =
      (size == 0 ? EVE // EVE 0 payload
      : (size < TAG_MINSIZE
        ? small(size, str, 1)
        : (size >= BLOB_MINSIZE
          ? blob(size, str, 1)
          : tag(size, str, 1, hash))));
    return LOCK_OUT(a);
}

Arrow buffer_atomn(uint32_t size, char* mem) {
    Arrow a;
    LOCK();
    if (size == 0)
        a = EVE;
    else if (size < TAG_MINSIZE) {
        a = small(size, mem, 0);
    } else if (size >= BLOB_MINSIZE) {
        a = blob(size, mem, 0);
    } else {
        a = tag(size, mem, 0, /*hash*/ 0);
    }
    return LOCK_OUT(a);
}

Arrow buffer_atomnMaybe(uint32_t size, char* mem) {
    Arrow a;
    LOCK();
    if (size == 0)
        a = EVE;
    else if (size < TAG_MINSIZE) {
        a = small(size, mem, 1);
    } else if (size >= BLOB_MINSIZE)
        a = blob(size, mem, 1);
    else {
        a = tag(size, mem, 1, /*hash*/ 0);
    }
    return LOCK_OUT(a);
}

Arrow buffer_headOf(Arrow a) {
    if (a == EVE)
        return EVE;
    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
        return LOCK_OUT(-1); // Invalid id

    if (cell.full.type != CELLTYPE_PAIR)
        return LOCK_OUT(a); // for atom, head = self
    else
        return LOCK_OUT(cell.pair.head);
}

Arrow buffer_tailOf(Arrow a) {
    if (a == EVE)
        return EVE;

    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
        return LOCK_OUT(-1); // Invalid id

    if (cell.full.type != CELLTYPE_PAIR)
        return LOCK_OUT(a); // for atom, tail = self
    else
        return LOCK_OUT(cell.pair.tail);
}

/** return the payload of an arrow (blob/tag/small), NULL on error */

static char* payloadOf(Arrow a, uint32_t* lengthP) {
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

    // Get the cell pointed by a
    Cell    cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    
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
    uint32_t hChain = hashChain(&cell) % PRIM1;
    if (!hChain) hChain = 1; // offset can't be 0

    uint32_t max = 0; // geoalloc() state variables
    geoalloc((char**) &payload, &max, &size, sizeof (char), sizeof(cell.tagOrBlob.slice0));
    if (!payload) { // allocation failed
      return NULL;
    }

    memcpy(payload, cell.tagOrBlob.slice0, sizeof(cell.tagOrBlob.slice0));

    Address next = jumpToFirst(&cell, a, hChain);
    Cell sliceCell;
    ram_get(next, (RamCell *)&sliceCell);
    ONDEBUG((SHOWCELL('R', next, &sliceCell)));

    while (1) {
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
        next = jumpToNext(&sliceCell, next, hChain);
        ram_get(next, (RamCell *)&sliceCell);
        ONDEBUG((SHOWCELL('R', next, &sliceCell)));
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

/** return the content behind an atom
*/
char* buffer_memOf(Arrow a, uint32_t* lengthP) {
    if (a == EVE) { // Eve has an empty payload
        return payloadOf(a, lengthP);
    }

    // Lock mem access
    LOCK();

    // get the cell pointed by a
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));

    if (cell.full.type < CELLTYPE_SMALL
        || cell.full.type > CELLTYPE_BLOB) { // Not an atom (empty cell, pair, ...)
        return (char *) LOCK_OUT(NULL);
    }

    
    uint32_t payloadLength;
    char* payload = payloadOf(a, &payloadLength);
    if (payload == NULL) { // Error
        return (char *) LOCK_OUT(NULL);
    }

    if (cell.full.type == CELLTYPE_BLOB) {
        // The payload is a BLOB footprint
        uint32_t blobLength;
        char* blobData = mem0_loadData(payload, lengthP);
        free(payload); // one doesn't return the payload, free...
        //* lengthP = ... set by mem0_loadData call
        return LOCK_OUT(blobData);
    }

    // else: Tag or small case
    *lengthP = payloadLength;
    return LOCK_OUT(payload);
}

char* buffer_strOf(Arrow a) {
    uint32_t lengthP;
    char* p = buffer_memOf(a, &lengthP);
    return p;
}


// URL-encode input buffer into destination buffer.
// 0-terminate the destination buffer.

static void percent_encode(const char *src, size_t src_len, char *dst, size_t* dst_len_p) {
    static const char *dont_escape = "_-,;~()";
    static const char *hex = "0123456789abcdef";
    size_t i, j;
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
    size_t i, j;
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

/** get an URI path corresponding to an arrow */
static char* toURI(Arrow a, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (a == XL_EVE) { // Eve is identified by an empty path
        
        // allocate and return an empty string
        char *s = (char*) malloc(1);
        if (!s) { // allocation failed
            return NULL;
        }

        s[0] = '\0';
        if (l) *l = 0; // if asked, return length = 0
        return s;
    }

    if (a >= RAM_SIZE) {
        return NULL; // address anomaly
    }

    // Get cell pointed by a
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));

    switch (cell.full.type) {
        case CELLTYPE_EMPTY:
        {
            return NULL; // address anomaly
        }
        case CELLTYPE_BLOB:
        { // return a BLOB digest
            return buffer_digestOf(a, l);
        }
        case CELLTYPE_SMALL:
        case CELLTYPE_TAG:
        { // return an encoded string with atom content

            // get atom content
            uint32_t memLength, encodedDataLength;
            char* mem = buffer_memOf(a, &memLength);
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
            char *tailUri = toURI(buffer_tailOf(a), &l1);
            if (tailUri == NULL) return NULL;

            char *headUri = toURI(buffer_headOf(a), &l2);
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

#define DIGEST_HASH_SIZE 8
#define DIGEST_SIZE (2 + CRYPTO_SIZE + DIGEST_HASH_SIZE)

char* buffer_digestOf(Arrow a, uint32_t *l) {
    TRACEPRINTF("BEGIN buffer_digestOf(%06x)", a);

    if (a >= RAM_SIZE) { // Address anomaly
        return NULL;
    }

    char    *digest;
    uint32_t hash;
    char    *hashStr = (char *) malloc(CRYPTO_SIZE + 1);
    if (!hashStr) { // alloc failed
      return NULL;
    }

    if (a == XL_EVE) { // Even Eve has a digest
        hash = hashOf(a);
        crypto(0, "", hashStr);
    }

    LOCK();

    // Get cell pointed by a
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));

    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT) {
        // Address anomaly : not an arrow
        return LOCK_OUTSTR(NULL);
    }
    
    hash = cell.arrow.hash;

    switch (cell.full.type) {
        case CELLTYPE_PAIR:
        {
            uint32_t uriLength;
            char* uri = toURI(a, &uriLength);
            if (uri == NULL) {
              return NULL;
            }
            crypto(uriLength, uri, hashStr);
            free(uri);
            break;
        }
        case CELLTYPE_BLOB:
        {
            uint32_t hashLength;
            free(hashStr);
            hashStr = payloadOf(a, &hashLength);
            if (hashStr == NULL)
              return NULL;
            break;
        }
        case CELLTYPE_SMALL:
        case CELLTYPE_TAG:
        {
            uint32_t dataSize;
            char* data = buffer_memOf(a, &dataSize);
            crypto(dataSize, data, hashStr);
            free(data);
            break;
        }
        default:
            assert(0);
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
    return LOCK_OUTSTR(digest);
}

char* buffer_uriOf(Arrow a, uint32_t *l) {
    LOCK();
    char *str = toURI(a, l);
    return LOCK_OUTSTR(str);
}

Arrow buffer_digestMaybe(char* digest) {
    TRACEPRINTF("BEGIN buffer_digestMaybe(%.58s)", digest);

    // read the hash at the digest beginning
    int i = 2; // jump over '$H' string
    uint32_t hash = HEXTOI(digest[i]);
    while (++i < 10) { // read 8 hexa digit
        hash = (hash << 4) | (uint64_t)HEXTOI(digest[i]);
    }
    
    Address hashAddress, hashProbe;
    Address probeAddress;
    Cell cell;

    hashAddress = hash % PRIM0 % RAM_SIZE; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    // Probe for an existing singleton
    DEBUGPRINTF("hash is %06x so probe from %06x", hash, hashAddress);

    LOCK();

    probeAddress = hashAddress;
    i = PROBE_LIMIT;
    while (--i) { // probing limit
        Cell probed;
        
        ram_get(probeAddress, (RamCell *)&probed);
        ONDEBUG((SHOWCELL('R', probeAddress, &probed)));

        if (probed.full.type != CELLTYPE_EMPTY
            && probed.full.type <= CELLTYPE_ARROWLIMIT
            && probed.arrow.hash == hash) { // found candidate

            // get its digest
            char* otherDigest = buffer_digestOf(probeAddress, NULL);
            if (!strcmp(otherDigest, digest)) { // both digest match
                free(otherDigest);
                return LOCK_OUT(probeAddress); // hit! arrow found!
            } else { // full digest mismatch
              free(otherDigest);
            }
        }

        if (probed.full.peeble == 0) {
          break; // Probing over
        }

        // shift probe
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }
    return LOCK_OUT(NIL); // Probing over. It's a miss
}

static Arrow fromUri(uint32_t size, unsigned char* uri, uint32_t* uriLength_p, int ifExist) {
    TRACEPRINTF("BEGIN fromUri(%s)", uri);
    Arrow a = NIL;
#define NAN (uint32_t)(-1)
    uint32_t uriLength = NAN;

    char c = uri[0];
    
    if (c <= 32 || !size) { // Any control-caracters/white-buffers are considered as URI break
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

                a = buffer_digestMaybe(uri);
                
                if (a == NIL) // Non assimilated blob
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

                // compute atom URI length 
                uriLength = 0;
                while ((size == NAN || uriLength < size)
                        && (c = uri[uriLength]) > 32 && c != '+' && c != '/')
                    uriLength++;
                assert(uriLength);

                char *atomStr = malloc(uriLength + 1);
                percent_decode(uri, uriLength, atomStr, &atomLength);
                if (atomLength < TAG_MINSIZE) {
                    a = small(atomLength, atomStr, ifExist);
                } else if (atomLength < BLOB_MINSIZE) {
                    uint64_t hash = hashStringAndGetSize(atomStr, &size);
                    a = payload(CELLTYPE_TAG, atomLength, atomStr, hash, ifExist);
                } else {
                    a = blob(atomLength, atomStr, ifExist);
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
        // white buffers are tolerated and ignored here
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
    if (size == 0)
         return a;

    gap = skeepSpacesAndOnePlus(size, uri + uriLength);
    if (size != NAN)
       size = (gap > size ? 0 : size - gap);
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

Arrow buffer_uri(char* aUri) {
    LOCK();
    Arrow a = uri(NAN, aUri, 0);
    return LOCK_OUT(a);
}

Arrow buffer_uriMaybe(char* aUri) {
    LOCK();
    Arrow a = uri(NAN, aUri, 1);
    return LOCK_OUT(a);
}

Arrow buffer_urin(uint32_t aSize, char* aUri) {
    LOCK();
    Arrow a = uri(aSize, aUri, 0);
    return LOCK_OUT(a);
}

Arrow buffer_urinMaybe(uint32_t aSize, char* aUri) {
    LOCK();
    Arrow a = uri(aSize, aUri, 1);
    return LOCK_OUT(a);
}

Arrow buffer_anonymous() {
    char anonymous[CRYPTO_SIZE + 1];
    Arrow a = NIL;
    do {
      do {
          // peek up a random string
          char random[80];
          snprintf(random, sizeof(random), "an0nymous:)%lx", (long)rand() ^ (long)time(NULL));
          crypto(80, random,  anonymous); // Access to unitialized data is deliberate/wanted
          a = buffer_atomMaybe(anonymous);
          assert(a != NIL);
      } while (a); // while not locally unique
      a = buffer_atom(anonymous);
    } while (resolve(a, 1) != NIL); // while not globally unique

    // what's brillant is that serverIdOf(a) == NIL so any descendant will be auto-local
    // no server-side isRooted/childrenOf
    return a;
}

static Arrow unrootChild(Arrow child, Arrow context) {
    buffer_unroot(child);
    return EVE;
}

static Arrow getHookBadge() {
    static Arrow hookBadge = EVE;
    if (hookBadge != EVE) return hookBadge; // try to avoid the costly lock.
    LOCK();
    if (hookBadge != EVE) return LOCK_OUT(hookBadge);
    hookBadge = buffer_atom("XLhO0K");
    buffer_root(hookBadge); 
    
    // prevent previous hooks to survive the reboot.
    buffer_childrenOfCB(hookBadge, unrootChild, EVE);
    
    return LOCK_OUT(hookBadge);
}

Arrow buffer_hook(void* hookp) {
  char hooks[64]; 
  snprintf(hooks, 64, "%p", hookp);
  // Note: hook must be bottom-rooted to be valid
  Arrow hook = buffer_root(buffer_pair(getHookBadge(), buffer_atom(hooks)));
  return hook;
}

void* buffer_pointerOf(Arrow a) { // Only bottom-rooted hook is accepted to limit hook forgery
    if (!buffer_isPair(a) || buffer_tailOf(a) != getHookBadge() || !buffer_isRooted(a))
        return NULL;
    
    char* hooks = buffer_strOf(buffer_headOf(a));
    void* hookp;
    int n = sscanf(hooks, "%p", &hookp);
    if (!n) hookp = NULL;
    free(hooks);
    return hookp;
}

int buffer_isEve(Arrow a) {
    return (a == EVE);
}

Arrow buffer_isPair(Arrow a) {
    if (a == EVE) {
      return EVE; 
    }

    if (a >= RAM_SIZE) { // Address anomaly
        return NIL;
    }

    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    LOCK_END();

    if (cell.full.type == CELLTYPE_EMPTY || cell.full.type > CELLTYPE_ARROWLIMIT)
        return NIL;

    return (cell.full.type == CELLTYPE_PAIR ? a : EVE);
}

Arrow buffer_isAtom(Arrow a) {
    if (a == EVE) {
      return EVE; 
    }

    if (a >= RAM_SIZE) { // Address anomaly
        return NIL;
    }

    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    LOCK_END();

    if (cell.full.type == CELLTYPE_EMPTY || cell.full.type > CELLTYPE_ARROWLIMIT)
        return NIL;

    return (cell.full.type == CELLTYPE_PAIR ? EVE : a);
}

enum e_xlType buffer_typeOf(Arrow a) {
    if (a == EVE) {
      return XL_EVE; 
    }

    if (a >= RAM_SIZE) {
      return XL_UNDEF;
    }

    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
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
static void connect(Arrow a, Arrow child, int childWeakness, int outgoing) {
    TRACEPRINTF("connect child=%06x to a=%06x weakness=%1x outgoing=%1x", child, a, childWeakness, outgoing);
    if (a == EVE) return; // One doesn't store Eve connectivity. 18/8/11 Why not?
    buffer_stats.connect++;
    
    // get the cell at a
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    assert(cell.full.type != CELLTYPE_EMPTY && cell.full.type <= CELLTYPE_ARROWLIMIT);
  
    // is the parent arrow (@a) loose?
    int childrenCount = (int)(cell.arrow.mutables & FLAGS_CHILDRENMASK);
    int weakChildrenCount = (int)((cell.arrow.mutables & FLAGS_WEAKCHILDRENMASK) >> 15);

    int loose = (childrenCount == 0 && !(cell.arrow.mutables & FLAGS_ROOTED));
    if (loose && !childWeakness) {
          // parent is not loose anymore

          // (try to) remove 'a' from the loose log if strong child
          looseLogRemove(a);
    }

    // propagate connection
    if (childrenCount == 0 && weakChildrenCount == 0) { // the parent arrow was not connected yet
       
        // the parent arrow is now connected. So one "connects" it to its own parents.
        if (cell.full.type == CELLTYPE_PAIR) {
            connect(cell.pair.tail, a, 0, 1 /* outgoing */);
            connect(cell.pair.head, a, 0, 0 /* incoming */);
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

    cell.arrow.mutables = cell.arrow.mutables
       & ((FLAGS_WEAKCHILDRENMASK | FLAGS_CHILDRENMASK) ^ 0xFFFFFFFFu) 
       | (weakChildrenCount << 15)
       | childrenCount;

    // Update child0
    if (!childWeakness) {
      // child0 always point the last strongly connnected child
      Arrow lastChild0 = cell.arrow.child0;
      int lastChild0Direction = ((cell.arrow.mutables & FLAGS_C0D) ? 1 : 0);
      cell.arrow.child0 = child;

      if (outgoing) {
        cell.arrow.mutables |= FLAGS_C0D;
      } else {
        cell.arrow.mutables &= (FLAGS_C0D ^ 0xFFFFFFFFu);
      }

      child = lastChild0; // Now one puts the previous value of child0 into another cell
      outgoing = lastChild0Direction;
    }
    
    // write parent cell
    ram_set(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('W', a, &cell)));

    if (child == EVE) {
      // child0 was empty and got the back-ref, nothing left to do
      return;
    }
  
    // Now, one puts a child reference into a children cell

    // compute hChild used by probing 
    uint32_t hChild = hashChild(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    // probing data
    Address next = a;
    Cell nextCell;
    int j; //< slot index

    while(1) { // probing loop

      // shift
      ADDRESS_SHIFT(next, next, hChild);

      // get the next cell
      ram_get(next, (RamCell *)&nextCell);
      ONDEBUG((SHOWCELL('R', next, &nextCell)));

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
              ram_set(next, (RamCell *)&nextCell);
              ONDEBUG((SHOWCELL('W', next, &nextCell)));
        
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
              ram_set(next, (RamCell *)&nextCell);
              ONDEBUG((SHOWCELL('W', next, &nextCell)));
        
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
    ram_set(next, (RamCell *)&nextCell);
    ONDEBUG((SHOWCELL('W', next, &nextCell)));
 }

/** disconnect a child pair from its parent arrow "a".
 * - it consists in removing the child from the children list of "a".
 * - a pair gets disconnected from its both parents as soon as it gets both unrooted and child-less
 *
 * Steps:
 * 1) compute counters
 * 2) if (child in child0 slot) remove it, update counters and done
 * 3) compute hashChild
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
static void disconnect(Arrow a, Arrow child, int weakness, int outgoing) {
    TRACEPRINTF("disconnect child=%06x from a=%06x weakness=%1x outgoing=%1x", child, a, weakness, outgoing);
    buffer_stats.disconnect++;
    if (a == EVE) return; // One doesn't store Eve connectivity.

    // get parent arrow definition
    Cell parent;
    ram_get(a, (RamCell *)&parent);
    ONDEBUG((SHOWCELL('R', a, &parent)));
    // check it's a valid arrow
    assert(parent.full.type != CELLTYPE_EMPTY && parent.full.type <= CELLTYPE_ARROWLIMIT);

    // get counters
    int childrenCount = (int)(parent.arrow.mutables & FLAGS_CHILDRENMASK);
    int weakChildrenCount = (int)((parent.arrow.mutables & FLAGS_WEAKCHILDRENMASK) >> 15);

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
    parent.arrow.mutables = parent.arrow.mutables
       & ((FLAGS_WEAKCHILDRENMASK | FLAGS_CHILDRENMASK) ^ 0xFFFFFFFFu) 
       | (weakChildrenCount << 15) | childrenCount;

    // check "loose" status evolution
    int loose = (childrenCount == 0 && !(parent.arrow.mutables & FLAGS_ROOTED));
    if (!weakness && loose) { // parent arrow got loose by removing the last strong child
       // add 'a' into the loose log if strong child
       looseLogAdd(a);
    }

    // propagate disconnection    
    if (childrenCount == 0 && weakChildrenCount == 0) { // parent arrow not connected any more 
        // the parent arrow got disconnected. So one "disconnects" it to its own parents.
        if (parent.full.type == CELLTYPE_PAIR) {
            disconnect(parent.pair.tail, a, 0, 1);
            disconnect(parent.pair.head, a, 0, 0);
        }
    }

    // child0 simple case
    if (parent.arrow.child0 == child) {
      int child0direction = parent.arrow.mutables & FLAGS_C0D;
      if (child0direction && outgoing || !(child0direction || outgoing)) {
        parent.arrow.child0 = 0;
        parent.arrow.mutables &= (FLAGS_C0D ^ 0xFFFFFFFFu);
        child = 0; // OK
      }
    }

    // write parent cell
    ram_set(a, (RamCell *)&parent);
    ONDEBUG((SHOWCELL('W', a, &parent)));

    if (child == 0) { // OK done
      return;
    }

    // compute parent arrow hChild (offset for child list)
    uint32_t hChild = hashChild(&parent) % PRIM1;
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
      ram_get(next, (RamCell *)&nextCell);
      ONDEBUG((SHOWCELL('R', next, &nextCell)));

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
                  // but don't delete peeble!
              } 
              ram_set(next, (RamCell *)&nextCell);
              ONDEBUG((SHOWCELL('W', next, &nextCell)));

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
void buffer_childrenOfCB(Arrow a, XLCallBack cb, Arrow context) {
    TRACEPRINTF("buffer_childrenOf a=%06x", a);

    if (a == EVE) {
        return; // Eve connectivity not traced
    }

    // get the parent cell
    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));

    // check arrow
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return; // invalid ID. TODO one might call cb with a


    if (!(cell.arrow.mutables & (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))) {
      // no child
      LOCK_END();
      return;
    }

    // compute hashChild
    uint32_t hChild = hashChild(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    if (cell.arrow.child0) {
      // child0
      cb(cell.arrow.child0, context);

      if ((cell.arrow.mutables & (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK)) == 1) {
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
      ram_get(next, (RamCell *)&nextCell);
      ONDEBUG((SHOWCELL('R', next, &nextCell)));

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
            ram_get(child, (RamCell *)&childCell);
            ONDEBUG((SHOWCELL('R', child, &childCell)));

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
    uint32_t hChild;
    int      iSlot;
    int      type;
} iterator_t;

static int buffer_enumNextChildOf(XLEnum e) {
    iterator_t *iteratorp = e;

    // iterate from current address stored in childrenOf iterator
    Address pos = iteratorp->pos;
    int     i   = iteratorp->iSlot;
    Arrow   a   = iteratorp->parent;

    LOCK();

    // get current cell
    Cell cell;
    ram_get(pos ? pos : a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', pos ? pos : a, &cell)));

    if (pos == EVE || pos == a) { // current cell is parent cell
        
        // check parent arrow is unchanged
        if (iteratorp->currentCell.full.type != cell.full.type
            || iteratorp->currentCell.arrow.hash != cell.arrow.hash
            || iteratorp->currentCell.arrow.dr != cell.arrow.dr
            || memcmp(iteratorp->currentCell.arrow.def,
                 cell.arrow.def, sizeof(cell.arrow.def)) != 0) {
            return LOCK_OUT(0); // arrow changed
        }

        if (!(cell.arrow.mutables &
          (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))) {
          // no child
          return LOCK_OUT(0); // no child
        }

        if (pos == EVE && cell.arrow.child0) {
          // return child0
          iteratorp->pos = a;
          iteratorp->iSlot = 0;
          iteratorp->current = cell.arrow.child0;
          return LOCK_OUT(!0);
        }

        // pos == a (or EVE without child0)
        if (1 == (cell.arrow.mutables &
              (FLAGS_CHILDRENMASK | FLAGS_WEAKCHILDRENMASK))
            && cell.arrow.child0) {
            return LOCK_OUT(0); // no child left
        }
        pos = a;
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

    uint32_t hChild = iteratorp->hChild;
    int iProbe = 0xFFFFF;
    while (1) { // children loop
      if (cell.full.type == CELLTYPE_CHILDREN) {

        while (i < 5) {
          Address inSlot = cell.children.C[i];
          if (inSlot == a && (cell.children.directions & (1 << (8 + i)))) {
            // terminator found
            iteratorp->currentCell = cell;
            iteratorp->pos     = pos;
            iteratorp->iSlot   = i;
            iteratorp->current = NIL;
            iteratorp->hChild  = hChild;
            return LOCK_OUT(0); // no child left

          } else if (inSlot != 0) {
            // child maybe found
            Arrow child = inSlot;

            // get child cell
            Cell childCell;
            ram_get(child, (RamCell *)&childCell);
            ONDEBUG((SHOWCELL('R', child, &childCell)));

            // check at least one arrow end is a
            if (childCell.pair.head == a || childCell.pair.tail == a) {
              // child!
              iteratorp->currentCell = cell;
              iteratorp->pos = pos;
              iteratorp->iSlot = i;
              iteratorp->current = child;
              iteratorp->hChild  = hChild;
              return LOCK_OUT(!0);
            }
          } // child maybe found
          i++;        
        } // slot loop
        iProbe = 0xFFFFF;
      } // children cell
       else {
          assert(--iProbe);
      }
      // shift
      ADDRESS_SHIFT(pos, pos, hChild);
      ram_get(pos, (RamCell *)&cell);
      ONDEBUG((SHOWCELL('R', pos, &cell)));
    } // children loop

    // FIXME what happens if terminator is removed while iterating
}

int buffer_enumNext(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    if (iteratorp->type == 0) {
        return buffer_enumNextChildOf(e);
    }
}

Arrow buffer_enumGet(XLEnum e) {
    assert(e);
    iterator_t *iteratorp = e;
    assert(iteratorp->type == 0);
    if (iteratorp->type == 0) {
        return iteratorp->current;
    }
}

void buffer_freeEnum(XLEnum e) {
    free(e);
}

XLEnum buffer_childrenOf(Arrow a) {
    TRACEPRINTF("buffer_childrenOf a=%06x", a);

    if (a == EVE) {
        return NULL; // Eve connectivity not traced
    }

    // get parent cell
    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    if (cell.full.type == CELLTYPE_EMPTY
      || cell.full.type > CELLTYPE_ARROWLIMIT)
      return LOCK_OUT(NULL); // Invalid id
    LOCK_END();
    
    // compute hashChild
    uint32_t hChild = hashChild(&cell) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    iterator_t *iteratorp = (iterator_t *) malloc(sizeof (iterator_t));
    assert(iteratorp);
    iteratorp->type = 0; // iterator type = childrenOf
    iteratorp->hChild = hChild; // hChild
    iteratorp->parent = a; // parent arrow
    iteratorp->current = EVE; // current child
    iteratorp->pos = EVE; // current cell holding child back-ref
    iteratorp->iSlot = 0; // position of child back-ref in cell : 0..5
    iteratorp->currentCell = cell; // user to detect change
    return iteratorp;
}

Arrow buffer_childOf(Arrow a) {
    XLEnum e = buffer_childrenOf(a);
    if (!e) return EVE;
    int n = 0;
    Arrow chosen = EVE;
    while (buffer_enumNext(e)) {
        Arrow child = buffer_enumGet(e);
        n++;
        if (n == 1 || rand() % n == 1) {
            chosen = child;
        }
    }
    return chosen;
}

/** root an arrow */
Arrow buffer_root(Arrow a) {
    buffer_stats.root++;
    if (a == EVE) {
        return EVE; // no
    }
    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));

    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return LOCK_OUT(EVE);

    if (cell.arrow.mutables & FLAGS_ROOTED) { // already rooted
      if (!(cell.arrow.mutables & FLAGS_ROOTSETTED)) {
        cell.arrow.mutables |= FLAGS_ROOTSETTED;
        ram_set(a, (RamCell *)&cell);
        ONDEBUG((SHOWCELL('W', a, &cell)));
        changeLogAdd(a);
      }
      return LOCK_OUT(a);
    }

    // If this arrow had no child before, it was loose
    int loose = !(cell.arrow.mutables & FLAGS_CHILDRENMASK);

    if (!(cell.arrow.mutables & FLAGS_ROOTSETTED)) {
      cell.arrow.mutables |= FLAGS_ROOTSETTED;
      changeLogAdd(a);
    }

    // change the arrow to ROOTED state
    cell.arrow.mutables |= FLAGS_ROOTED;
    ram_set(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('W', a, &cell)));

    if (loose) { // if arrow was loose, one now connects it to its parents
        if (cell.full.type == CELLTYPE_PAIR) {
            connect(cell.pair.tail, a, 0, 1);
            connect(cell.pair.head, a, 0, 0);
        }
        // TODO tuple case
        // (try to) remove 'a' from the loose log
        looseLogRemove(a);
    }
    return LOCK_OUT(a);
}

/** unroot a rooted arrow */
Arrow buffer_unroot(Arrow a) {
    buffer_stats.unroot++;

    if (a == EVE)
        return EVE; // no.

    LOCK();

    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return LOCK_OUT(EVE);
    
    if (!(cell.arrow.mutables & FLAGS_ROOTED)) {
      if (!(cell.arrow.mutables & FLAGS_ROOTSETTED)) {
        cell.arrow.mutables |= FLAGS_ROOTSETTED;
        ram_set(a, (RamCell *)&cell);
        ONDEBUG((SHOWCELL('W', a, &cell)));
        changeLogAdd(a);
      }
      return LOCK_OUT(a);
    }

    if (!(cell.arrow.mutables & FLAGS_ROOTSETTED)) {
      cell.arrow.mutables |= FLAGS_ROOTSETTED;
      changeLogAdd(a);
    }
    // change the arrow to UNROOTED state
    cell.arrow.mutables ^= FLAGS_ROOTED;
    ram_set(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('W', a, &cell)));

    // If this arrow has no child, it's now loose
    int loose = !(cell.arrow.mutables & FLAGS_CHILDRENMASK);
    if (loose) {
        // loose log
        looseLogAdd(a);

        // If it's loose, one disconnects it from its parents.
        if (cell.full.type == CELLTYPE_PAIR) {
            disconnect(cell.pair.tail, a, 0, 1);
            disconnect(cell.pair.head, a, 0, 0);
        } // TODO Tuple case
    }
    return LOCK_OUT(a);
}

/** return the root status */
int buffer_isRooted(Arrow a) {
    if (a == EVE) return EVE;
      
    LOCK();

    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return LOCK_OUT(EVE);
    
    if (cell.arrow.spaceId == 0
        && !(cell.arrow.mutables & FLAGS_ROOTSETTED)) {
      // if arrow root flags has not been setted yet and arrow is unresolved

      // resolve the arrow
      Arrow A = resolve(a, 1);
      if (A && A != NIL) { // disk counterpart found

        // load the arrow root state from its disk counterpart
        if (xl_isRooted(A)) {
          if (!(cell.arrow.mutables & FLAGS_ROOTED)) {
            cell.arrow.spaceId = A;
            cell.arrow.mutables |= FLAGS_ROOTED;
            ram_set(a, (RamCell *)&cell);
            ONDEBUG((SHOWCELL('W', a, &cell)));
            int loose = !(cell.arrow.mutables & FLAGS_CHILDRENMASK);
            if (loose) { // if arrow was loose, one now connects it to its parents
              if (cell.full.type == CELLTYPE_PAIR) {
                  connect(cell.pair.tail, a, 0, 1);
                  connect(cell.pair.head, a, 0, 0);
              }
              looseLogRemove(a);
            }
          }
        } else {
          if (cell.arrow.mutables & FLAGS_ROOTED)) {
            cell.arrow.spaceId = A;
            cell.arrow.mutables ^= FLAGS_ROOTED;
            ram_set(a, (RamCell *)&cell);
            ONDEBUG((SHOWCELL('W', a, &cell)));
            // If this arrow has no child, it's now loose
            int loose = !(cell.arrow.mutables & FLAGS_CHILDRENMASK);
            if (loose) {
              // loose log
              looseLogAdd(a);

              // If it's loose, one disconnects it from its parents.
              if (cell.full.type == CELLTYPE_PAIR) {
                  disconnect(cell.pair.tail, a, 0, 1);
                  disconnect(cell.pair.head, a, 0, 0);
              } // TODO Tuple case
            }
          }
        }
      }
    }
    return LOCK_OUT(cell.arrow.mutables & FLAGS_ROOTED ? a : EVE);
}

/** check if an arrow is loose */
int buffer_isLoose(Arrow a) {
    if (a == EVE) return EVE;

    LOCK();
    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    LOCK_END();
    
    if (cell.full.type == CELLTYPE_EMPTY
        || cell.full.type > CELLTYPE_ARROWLIMIT)
      return 0;

    if (cell.arrow.mutables & FLAGS_ROOTED)
      return 0;

    if (cell.arrow.mutables & FLAGS_CHILDRENMASK)
      return 0;

    return 1;
}

int buffer_equal(Arrow a, Arrow b) {
    return (a == b);
}




/* TODO Réécrire forget en ajoutant la suppression des weak */


/** forget a loose arrow, that is actually remove it from the main memory */
static void forget(Arrow a) {
    TRACEPRINTF("forget loose arrow %06x", a);
    buffer_stats.forget++;

    Cell cell;
    ram_get(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('R', a, &cell)));
    assert(cell.full.type != CELLTYPE_EMPTY
        || cell.full.type <= CELLTYPE_ARROWLIMIT);

    uint32_t hash = cell.arrow.hash;
    Address hashAddress; // base address
    Address hashProbe; // probe offset
    
    if (cell.full.type == CELLTYPE_TAG
        || cell.full.type > CELLTYPE_BLOB) {
        // Free chain
        // FIXME this code doesn't erase reattachment cells :( don't use jump functions
        Address hChain = hashChain(&cell) % PRIM1;
        if (!hChain) hChain = 1; // offset can't be 0

        Address next = jumpToFirst(&cell, a, hChain);
        Cell sliceCell;
        ram_get(next, (RamCell *)&sliceCell);
        ONDEBUG((SHOWCELL('R', next, &sliceCell)));
        while (sliceCell.full.type == CELLTYPE_SLICE) {
            // free cell
            Address current = next;
            next = jumpToNext(&sliceCell, next, hChain);

            memset(sliceCell.full.data, 0, sizeof(sliceCell.full.data));
            sliceCell.full.type = CELLTYPE_EMPTY;
 
            ram_set(current, (RamCell *)&sliceCell);
            ONDEBUG((SHOWCELL('W', current, &sliceCell)));
            
            ram_get(next, (RamCell *)&sliceCell);
            ONDEBUG((SHOWCELL('R', next, &sliceCell)));
        }
        assert(sliceCell.full.type == CELLTYPE_LAST);
        // free cell
        memset(sliceCell.full.data, 0, sizeof(sliceCell.full.data));
        sliceCell.full.type = CELLTYPE_EMPTY;
 
        ram_set(next, (RamCell *)&sliceCell);
        ONDEBUG((SHOWCELL('W', next, &sliceCell)));
    }

    // Free definition start cell
    memset(cell.full.data, 0, sizeof(cell.full.data));
    cell.full.type = CELLTYPE_EMPTY;
    ram_set(a, (RamCell *)&cell);
    ONDEBUG((SHOWCELL('W', a, &cell)));

    hashAddress = hash % PRIM0 % RAM_SIZE; // base address
    hashProbe = hash % PRIM1; // probe offset
    if (!hashProbe) hashProbe = 1; // offset can't be 0

    /* Now decremeting "peeble" counters in the probing path
     * up to the forgotten singleton
     */
    Address probeAddress = hashAddress;
    // ONDEBUG(if (probeAddress != a) DEBUGPRINTF("real address %X != open Address %X", a, probeAddress));
    while (probeAddress != a) {
        ram_get(probeAddress, (RamCell *)&cell);
        ONDEBUG((SHOWCELL('R', probeAddress, &cell)));
        if (cell.full.peeble)
          cell.full.peeble--;

        ram_set(probeAddress, (RamCell *)&cell);
        ONDEBUG((SHOWCELL('W', probeAddress, &cell)));
        ADDRESS_SHIFT(probeAddress, probeAddress, hashProbe);
    }

}

void buffer_begin() {
    LOCK();
    ACTIVITY_BEGIN();
    LOCK_END();
}

/** type an arrow as a "state" arrow.
 *  TODO: is it necessary?
 */
Arrow buffer_state(Arrow a) {
    static Arrow systemStateBadge = EVE;
    if (systemStateBadge == EVE) { // to avoid locking in most cases
            LOCK();
            if (systemStateBadge == EVE) {
                systemStateBadge = buffer_atom("XLsTAT3");
                buffer_root(systemStateBadge);

                // prevent previous session saved states to survive the reboot.
                buffer_childrenOfCB(systemStateBadge, unrootChild, EVE);
            }
            LOCK_END();
    }
    return buffer_pair(systemStateBadge, a);
}


static void bufferGC() {
    TRACEPRINTF("BEGIN bufferGC()");

    for (unsigned i = looseLogSize; i > 0; i--) { // loose stack scanning
        Arrow a = looseLog[i - 1];

        if (buffer_isLoose(a)) { // a loose arrow is removed NOW
            forget(a);
        }
    }

    TRACEPRINTF("END bufferGC() looseLogSize=%d get=%d root=%d unroot=%d new=%d (pair=%d atom=%d) found=%d connect=%d, disconnect=%d forget=%d",
        looseLogSize, buffer_stats.get, buffer_stats.root,
        buffer_stats.unroot, buffer_stats.new, 
        buffer_stats.pair, buffer_stats.atom, buffer_stats.found,
        buffer_stats.connect, buffer_stats.disconnect, buffer_stats.forget);
    buffer_stats = buffer_stats_zero;

    zeroalloc((char**) &looseLog, &looseLogMax, &looseLogSize);

    bufferGCNeeded = 0;
}

void buffer_over() {
    bufferGCNeeded = 1;

    LOCK();
    ACTIVITY_OVER();
    do {
        WAIT_DORMANCY();
    } while (apiActivity > 0); // spurious wakeup check
    
    if (bufferGCNeeded) {
        bufferGC();
    }
    LOCK_END();
}

void buffer_commit() {
    TRACEPRINTF("buffer_commit (looseLogSize = %d)", looseLogSize);
    bufferGCNeeded = 1;
    
    LOCK();
    ACTIVITY_OVER();
    do {
        WAIT_DORMANCY();
    } while (apiActivity > 0); // spurious wakeup check
    
    if (bufferGCNeeded) {
        bufferGC();
    }
    
    // Apply buffer changes to real entrelacs space
    logChangeFlush();

    ACTIVITY_BEGIN();
    LOCK_END();
    
}

/** buffer_yield */
void buffer_yield(Arrow a) {
    Arrow stateArrow = EVE;
    TRACEPRINTF("buffer_yield (looseLogSize = %d)", looseLogSize);
    if (a != EVE) {
        stateArrow = buffer_state(a);
        buffer_root(stateArrow);
    }
    buffer_over();
    buffer_begin();
    if (stateArrow != EVE) {
        buffer_unroot(stateArrow);
    }
}

/** printf extension for arrow (%O specifier) */
static int printf_arrow_extension(FILE *stream,
        const struct printf_info *info,
        const void *const *args) {
    static const char* nilFakeURI = "(NIL)";
    Arrow arrow = *((Arrow*) (args[0]));
    char* uri = arrow != NIL ? buffer_uriOf(arrow, NULL) : (char *)nilFakeURI;
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

/** initialize a buffered Entrelacs system */
int buffer_init() {
    static int buffer_init_done = 0;
    if (buffer_init_done) return 0;
    buffer_init_done = 1;
  

    assert(sizeof(RamCell) == sizeof(Cell));

    pthread_cond_init(&apiNowActive, NULL);
    pthread_cond_init(&apiNowDormant, NULL);

    pthread_mutexattr_init(&apiMutexAttr);
    pthread_mutexattr_settype(&apiMutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&apiMutex, &apiMutexAttr);

    int rc = ram_init();
    if (rc < 0) { // problem
       return rc;
    }

    // register a printf extension for arrow (glibc only!)
    register_printf_specifier('O', printf_arrow_extension, printf_arrow_arginfo_size);
    
    geoalloc((char **)&changeLog, &changeLogMax, &changeLogSize, sizeof(Arrow), 0);
    geoalloc((char **)&looseLog, &looseLogMax, &looseLogSize, sizeof(Arrow), 0);
    return rc;
}

void buffer_destroy() {
    // TODO complete
    free(looseLog);
    free(changeLog);

    pthread_cond_destroy(&apiNowActive);
    pthread_cond_destroy(&apiNowDormant);
    pthread_mutexattr_destroy(&apiMutexAttr);
    pthread_mutex_destroy(&apiMutex);

    ram_destroy();
}
