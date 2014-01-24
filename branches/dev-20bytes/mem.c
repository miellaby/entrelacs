/** @file
 Orthogonally persistent memory device.
 It basically consists in a software implemented direct-mapped RAM cache with buffering capacity in front of mem0.
 Changed cells in the current micro-transaction are buffered until a commit is performed (it's neither a write-trough nor a 'lazy write' cache).
 */
#define MEM_C
#include <stdlib.h>
#include <assert.h>
#define LOG_CURRENT LOG_MEM
#include "log.h"
#include "mem.h"

#define MEMSIZE (8192<<3)
// 0x20000
static const Address memSize = MEMSIZE ; ///< mem size

/** The main RAM cache, aka "mem".
 mem is an array of MEMSIZE records.
 Each record can carry one mem0 cell.
 */
static struct s_mem {
  CellBody c;
  uint16_t a; ///< <--mem1 internal flags (4 bits)--><--page # (12 bits)-->
  uint16_t stamp;
} mem[MEMSIZE];

#define RESERVESIZE 1024
static const Address reserveSize = RESERVESIZE ; ///< cache reserve size
static uint32_t pokes = 0;

/** The RAM cache reserve.
 Modified cells are moved into this reserve when their location in mem[] are need to load other cells
 TODO replace it by cuckoo hashing or smarter alternative
 */
static struct s_reserve { // TODO : change the name
  Cell    c;
  Address a;
  uint16_t stamp;
} reserve[RESERVESIZE];

static uint32_t reserveHead = 0; ///< cache reserve stack head

/** The change log.
 This is a log of all changes made in cached memory. It grows geometrically (see geoalloc() function).
 */
static Address* log = NULL; ///< dynamically allocated
static size_t   logMax = 0; ///< heap allocated log size
static size_t   logSize = 0; ///< significative log size

static struct s_mem_stats {
    int notFound;
    int mainFound;
    int reserveFound;
    int reserveMovesBecauseSet;
    int reserveMovesBecauseGet;
    int mainReplaces;
    int getCount;
    int setCount;
    int dontKnown;
} mem_stats_zero = {
    0, 0, 0, 0, 0, 0, 0, 0
}, mem_stats = {
    0, 0, 0, 0, 0, 0, 0, 0    
};


#define MEM1_CHANGED 0x1000
#define MEM1_EMPTY   0xE000
#define memPageOf(M) ((M)->a & 0x0FFF)
#define memIsEmpty(M) ((M)->a == MEM1_EMPTY)
#define memIsChanged(M) ((M)->a & MEM1_CHANGED)


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
  } else {
    while (s > *maxp) {
      *maxp *= 2;
    }
    *pp = (char *)realloc(*pp, *maxp * us);
  }
  *sp = s;
  return;
}

/** Get a cell */
int mem_get_advanced(Address a, CellBody* pCellBody, uint16_t* stamp_p) {
  TRACEPRINTF("mem_get(%06x) begin", a);
  Address i;
  Address offset = a % MEMSIZE;
  uint32_t  page = a / MEMSIZE;
  mem_stats.getCount++;
  struct s_mem* m = &mem[offset];
  
  if (!memIsEmpty(m) && memPageOf(m) == page) {
    // cache hit
    DEBUGPRINTF("mem_get cache hit");
    if (stamp_p != NULL) *stamp_p = m.stamp;
    mem_stats.mainFound++;
    *pCellBody = m->c;
    return 0;
  }

  
  for (i = 0; i < reserveHead ; i++) {
    if (reserve[i].a == a) {
      // reserve hit
      DEBUGPRINTF("mem_get reserve hit");
      if (stamp_p != NULL) *stamp_p = reserve[i].stamp;
      mem_stats.reserveFound++;
      *pCellBody = reserve[i].c;
      return 0;
    }
  }

  if (memIsChanged(m)) {
    // FIXME: we're obliged to cache a cell read in place of modified cell
    // because mem_write doesn't work otherwise.
    // if (stamp_p != NULL) *stamp_p = pokes;
    // return mem0_get(a);
        
    // When replacing a modified cell, move it to reserve
    Address moved = memPageOf(m) * MEMSIZE + offset;
    DEBUGPRINTF("mem_get moves %06x from mem to reserve", moved);
    if (reserveHead >= reserveSize){
        LOGPRINTF(LOG_ERROR, "reserve full :( :( logSize=%d getCount=%d setCount=%d reserveMovesBecauseSet=%d"
          " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d mainFound=%d reserveFound=%d",
          logSize, mem_stats.getCount, mem_stats.setCount,
          mem_stats.reserveMovesBecauseSet, mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
          mem_stats.notFound, mem_stats.mainFound, mem_stats.reserveFound);
        return -1;
    }
    reserve[reserveHead].a = moved;
    reserve[reserveHead].stamp = m->stamp;
    reserve[reserveHead++].c = m->c;
    mem_stats.reserveMovesBecauseGet++;
  }

  mem[offset].a = page;
  mem[offset].stamp = pokes;
  if (stamp_p != NULL) *stamp_p = pokes;
  mem_stats.notFound++;
  *pCellBody = (mem[offset]->c = mem0_get(a));
  return 0;
}

/** Get a cell */
int mem_get(Address a, CellBody *pCellBody) {
    return mem_get_advanced(a, NULL);
}

/** Set a cell */
int mem_set(Address a, CellBody *pCellBody) {
    TRACEPRINTF("mem_set(%06x) begin", a);
    Address offset = a % MEMSIZE;
    uint32_t  page = a / MEMSIZE;
    mem_stats.setCount++;
    struct s_mem* m = &mem[offset];

    if (!memIsEmpty(m) && memPageOf(m) != page)  { // Uhh that's not fun
        for (int i = reserveHead - 1; i >=0 ; i--) { // Look at the reserve
            if (reserve[i].a == a) {
                DEBUGPRINTF("mem cell found in reserve");
                // one changes data in the reserve cell
                reserve[i].c = *pCellBody;
                reserve[i].stamp = ++pokes;
                mem_stats.reserveFound++;
                return 0; // no need to log it (it's already done)
            }
        }

        // no copy in the reserve, one puts the modified cell in mem0
        if (memIsChanged(m)) { // one replaces a modificied cell that one moves to reserve
            Address moved = memPageOf(m) * MEMSIZE + offset;
            DEBUGPRINTF("mem_set moved %06x to reserve", moved);
            if (reserveHead >= reserveSize) {
                LOGPRINTF(LOG_ERROR, "reserve full :( :( logSize=%d getCount=%d setCount=%d reserveMovesBecauseSet=%d"
                  " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d mainFound=%d reserveFound=%d",
                  logSize, mem_stats.getCount, mem_stats.setCount,
                  mem_stats.reserveMovesBecauseSet, mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
                  mem_stats.notFound, mem_stats.mainFound, mem_stats.reserveFound);
                return -1;
            }

            reserve[reserveHead].a = moved;
            reserve[reserveHead].stamp = m->stamp;
            reserve[reserveHead].c = m->c;
            reserveHead++;
            mem_stats.reserveMovesBecauseSet++;
        } else {
            mem_stats.mainReplaces++;
            // No need to load the cell from mem0 since one overwrites it
        }
    } else if (!memIsEmpty(m) && !memIsChanged(m)) { // Log change
        geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), logSize + 1);
        log[logSize - 1] = a;
    }

    m->.a = MEM1_CHANGED | page;
    m->stamp = ++pokes;
    m->c = *pCellBody;
    DEBUGPRINTF("mem_set end, logSize=%d", logSize);
    return 0;
}

static int _addressCmp(const void *a, const void *b) {
  return (a == b ? 0 : (a > b ? 1 : -1));
}

/** commit */
int mem_commit() {
  TRACEPRINTF("BEGIN mem_commit() logSize=%d", logSize);
  Address i;
  if (!logSize) {
      assert(reserveHead == 0);
      return;
  }

  // sort memory log
  qsort(log, logSize, sizeof(Address), _addressCmp);

  // TODO: optimize by commiting all reserved cells first then all logged cells found in mem
  for (i = 0; i < logSize; i++) {
    Address a = log[i];
    Address offset = a % MEMSIZE;
    CellBody c;
    if (mem_get(a, &c))
      return -1:

    if (mem0_set(a, &c))
      return -1;

    mem[offset].a &= 0xEFFF; // reset changed flag for this offset (even if the changed cell is actually in reserve)
  }

  if (mem0_commit())
    return -1;

  if (mem_is_out_of_sync) { // Reset cache because out of sync
    for (i = 0; i < memSize; i++) {
      mem[i].a = MEM1_EMPTY;
      mem[i].stamp = 0;
    }
  }
  TRACEPRINTF("END mem_commit() logSize=%d getCount=%d setCount=%d reserveMovesBecauseSet=%d"
          " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d mainFound=%d reserveFound=%d",
          logSize, mem_stats.getCount, mem_stats.setCount,
          mem_stats.reserveMovesBecauseSet, mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
          mem_stats.notFound, mem_stats.mainFound, mem_stats.reserveFound);
  zeroalloc((char **)&log, &logMax, &logSize);
  mem_stats = mem_stats_zero;
  reserveHead = 0;
}

int mem_yield() {
    if (reserveHead > reserveSize  / 4) {
        if (mem_commit())
          return -1;

        return 1;
    }
    return 0;
}

uint32_t mem_pokes() {
    return pokes;
}

/** init
  @return 1 if very first start, <0 if error, 0 otherwise
*/
int mem_init() {
  TRACEPRINTF("mem_init (memSize = %d)");
  int rc = mem0_init();
  if (rc < 0) return rc;

  Address i;
  for (i = 0; i < memSize; i++) {
    mem[i].a = MEM1_EMPTY;
    mem[i].stamp = 0;
  }

  geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), 0);

  return rc;
}

int mem_destroy() {
    free(log);
    mem0_destroy();
}
