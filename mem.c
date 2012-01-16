/** @file
 Orthogonally persistent memory device.
 It basically consists in a software implemented direct-mapped RAM cache with buffering capacity in front of mem0.
 Changed cells in the current micro-transaction are buffered until a commit is performed (it's neither a write-trough nor a 'lazy write' cache).
 */
#define MEM_C
#include <stdlib.h>
#include <assert.h>
#include "mem.h"

#ifndef PRODUCTION
#include "stdio.h"
  #define ONDEBUG(w) w
#else
  #define ONDEBUG(w)
#endif

#define MEMSIZE 0x2000
static const Address memSize = MEMSIZE ; ///< mem size (8192 records)

/** The main RAM cache, aka "mem".
 mem is an array of MEMSIZE records.
 Each record can carry one mem0 cell.
 */
static struct s_mem {
  unsigned short a; ///< <--mem1 internal flags (4 bits)--><--page # (12 bits)-->
  Cell c;
} m, mem[MEMSIZE];

#define RESERVESIZE 1024
static const Address reserveSize = RESERVESIZE ; ///< cache reserve size

/** The RAM cache reserve.
 Modified cells are moved into this reserve when their location in mem[] are need to load other cells
 TODO replace it by cuckoo hashing or smarter alternative
 */
static struct s_reserve {
  Address a;
  Cell    c;
} reserve[RESERVESIZE];

static uint32_t reserveHead = 0; ///< cache reserve stack head

/** The change log.
 This is a log of all changes made in cached memory. It grows geometrically (see geoalloc() function).
 */
static Address* log = NULL; ///< dynamically allocated
static size_t   logMax = 0; ///< heap allocated log size
static size_t   logSize = 0; ///< significative log size



#define MEM1_CHANGED 0x1000
#define MEM1_EMPTY   0xE000
#define memPageOf(M) ((M).a & 0x0FFF)
#define memIsEmpty(M) ((M).a == MEM1_EMPTY)
#define memIsChanged(M) ((M).a & MEM1_CHANGED)


/** set-up or reset a tuple {allocated memory, allocated size, significative size} used by geoalloc() function */
void zeroalloc(char** pp, size_t* maxp, size_t* sp) {
  if (*maxp) {
    free(*pp);
    *maxp = 0;
    *pp = 0;
  }
  *sp = 0;
}

/** geometricaly grow a heap allocated memory area by providing a tuple {allocated memory, allocated size, current significative size} and a new significative size */
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s) {
  if (!*maxp)	{
      *maxp = (s < 128 ? 256 : s + 128 );
      *pp = (char *)malloc(*maxp * us);
      assert(*pp);
  } else {
    while (s > *maxp) {
      *maxp *= 2;
      *pp = (char *)realloc(*pp, *maxp * us);
      assert(*pp);
    }
  }
  *sp = s;
  return;
}

/** Get a cell */
Cell mem_get(Address a) {
  ONDEBUG((fprintf(stderr, "mem_get@%06x = ", a)));
  Address i;
  Address offset = a & 0xFFF;
  uint32_t  page = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page) {
    ONDEBUG((fprintf(stderr, "%016llx from cache\n", m.c)));
    return m.c;
  }
  
  for (i = 0; i < reserveHead ; i++) {
    if (reserve[i].a == a) {
      ONDEBUG((fprintf(stderr, "%016llx from reserve\n", reserve[i].c)));
      return reserve[i].c;
    }
  }

  if (memIsChanged(m)) {
    // When replacing a modified cell, move it to reserve
    Address moved = (memPageOf(m) << 12) | offset;
    ONDEBUG((fprintf(stderr, "(%06x moved in reserve) ", moved)));
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = moved;
    reserve[reserveHead++].c = m.c;
  }

  mem[offset].a = page;
  return (mem[offset].c = mem0_get(a));
  // rem: debug trace end put by mem0_get
}

/** Set a cell */
void mem_set(Address a, Cell c) {
    ONDEBUG((fprintf(stderr, "mem_set@%06x : %016llx", a, c)));
    Address offset = a & 0xFFF;
    uint32_t  page = a >> 12;
    m = mem[offset];
    if (memIsEmpty(m) || memPageOf(m) != page)  { // Uhh that's not fun
        for (int i = reserveHead - 1; i >=0 ; i--) { // Look at the reserve
            if (reserve[i].a == a) {
                ONDEBUG((fprintf(stderr, "(found in reserve)\n")));
                // one changes data in the reserve cell
                reserve[i].c = c;
                return; // no need to log it (it's already done)
            }
        }
        // no copy in the reserve, one puts the modified cell in mem0
        if (memIsChanged(m)) { // one replaces a modificied cell that one moves to reserve
            Address moved = (memPageOf(m) << 12) | offset;
            ONDEBUG((fprintf(stderr, "(%06x moved in reserve) ", moved)));
            assert(reserveHead < reserveSize);
            reserve[reserveHead].a = moved;
            reserve[reserveHead].c = m.c;
            reserveHead++;
        }
        // No need to load the cell from mem0 since one overwrites it
        m.a = 0;
    }
    
    if (!memIsChanged(m)) { // Log it
        geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), logSize + 1);
        log[logSize - 1] = a;
    }

    mem[offset].a = MEM1_CHANGED | page;
    mem[offset].c = c;
    ONDEBUG((fprintf(stderr, "logSize=%d\n", logSize)));
}

static int _addressCmp(const void *a, const void *b) {
  return (a == b ? 0 : (a > b ? 1 : -1));
}

/** commit */
void mem_commit() {
  ONDEBUG((fprintf(stderr, "mem_commit (logSize = %d)\n", logSize)));
  Address i;
  if (!logSize) return;

  qsort(log, logSize, sizeof(Address), _addressCmp);
  // TODO: optimize by commiting all reserved cells first then all logged cells found in mem
  for (i = 0; i < logSize; i++) {
    Address a = log[i];
    Address offset = a & 0xFFF;
    mem0_set(a, mem_get(a));
    mem[offset].a &= 0xEFFF; // reset changed flag for this offset (even if the changed cell is actually in reserve)
  }
  mem0_commit();

  zeroalloc((char **)&log, &logMax, &logSize);
  reserveHead = 0;
  ONDEBUG((fprintf(stderr, "mem_commit done\n")));
}

/** init
  @return 1 if very first start, <0 if error, 0 otherwise
*/
int mem_init() {
  ONDEBUG((fprintf(stderr, "mem_init (memSize = %d)\n", memSize)));
  int rc = mem0_init();
  if (rc < 0) return rc;
  Address i;
  for (i = 0; i < memSize; i++) {
    mem[i].a = MEM1_EMPTY;
  }

  geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), 0);

  return rc;
}
