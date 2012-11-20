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

#define MEMSIZE 0x2000
static const Address memSize = MEMSIZE ; ///< mem size (8192 records)

/** The main RAM cache, aka "mem".
 mem is an array of MEMSIZE records.
 Each record can carry one mem0 cell.
 */
static struct s_mem {
  Cell c;
  uint16_t a; ///< <--mem1 internal flags (4 bits)--><--page # (12 bits)-->
  uint16_t stamp;
} m, mem[MEMSIZE];

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
Cell mem_get_advanced(Address a, uint16_t* stamp_p) {
  TRACEPRINTF("mem_get(%06x) begin", a);
  Address i;
  Address offset = a & 0xFFF;
  uint32_t  page = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page) {
    DEBUGPRINTF("mem_get returns %016llx from cache", m.c);
    if (stamp_p != NULL) *stamp_p = m.stamp;
    return m.c;
  }
  
  for (i = 0; i < reserveHead ; i++) {
    if (reserve[i].a == a) {
      DEBUGPRINTF("mem_get returns %016llx from reserve", reserve[i].c);
      if (stamp_p != NULL) *stamp_p = reserve[i].stamp;
      return reserve[i].c;
    }
  }

  if (memIsChanged(m)) {
    // When replacing a modified cell, move it to reserve
    Address moved = (memPageOf(m) << 12) | offset;
    DEBUGPRINTF("mem_get moves %06x to reserve", moved);
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = moved;
    reserve[reserveHead].stamp = m.stamp;
    reserve[reserveHead++].c = m.c;
  }

  mem[offset].a = page;
  mem[offset].stamp = pokes;
  if (stamp_p != NULL) *stamp_p = pokes;
  return (mem[offset].c = mem0_get(a));
}

/** Get a cell */
Cell mem_get(Address a) {
    return mem_get_advanced(a, NULL);
}

/** Set a cell */
void mem_set(Address a, Cell c) {
    TRACEPRINTF("mem_set(%06x, %016llx) begin", a, c);
    Address offset = a & 0xFFF;
    uint32_t  page = a >> 12;
    m = mem[offset];
    if (memIsEmpty(m) || memPageOf(m) != page)  { // Uhh that's not fun
        for (int i = reserveHead - 1; i >=0 ; i--) { // Look at the reserve
            if (reserve[i].a == a) {
                DEBUGPRINTF("mem cell found in reserve");
                // one changes data in the reserve cell
                reserve[i].c = c;
                reserve[i].stamp = ++pokes;
                return; // no need to log it (it's already done)
            }
        }
        // no copy in the reserve, one puts the modified cell in mem0
        if (memIsChanged(m)) { // one replaces a modificied cell that one moves to reserve
            Address moved = (memPageOf(m) << 12) | offset;
            DEBUGPRINTF("mem_set moved %06x to reserve", moved);
            assert(reserveHead < reserveSize);
            reserve[reserveHead].a = moved;
            reserve[reserveHead].stamp = m.stamp;
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
    mem[offset].stamp = ++pokes;
    mem[offset].c = c;
    DEBUGPRINTF("mem_set end, logSize=%d", logSize);
}

static int _addressCmp(const void *a, const void *b) {
  return (a == b ? 0 : (a > b ? 1 : -1));
}

/** commit */
void mem_commit() {
  TRACEPRINTF("mem_commit begin, logSize=%d", logSize);
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
    Address offset = a & 0xFFF;
    mem0_set(a, mem_get(a));
    mem[offset].a &= 0xEFFF; // reset changed flag for this offset (even if the changed cell is actually in reserve)
  }
  mem0_commit();
  if (mem_is_out_of_sync) { // Reset cache because out of sync
    for (i = 0; i < memSize; i++) {
      mem[i].a = MEM1_EMPTY;
      mem[i].stamp = 0;
    }
  }
  zeroalloc((char **)&log, &logMax, &logSize);
  reserveHead = 0;
  LOGPRINTF(LOG_WARN, "mem_commit done");
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