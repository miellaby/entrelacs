/** @file mem.c
 * @brief Orthogonally persistent memory storage for the arrow space.
 * @details
 * "mem" is a Direct-mapped RAM cache with buffering capacity accessing "mem0".
 * "mem0" is a persistence file subdivised into cells (slots).
 * Modified cells in the current micro-transaction are buffered until next commit.
 * This cache is neither a write-trough nor a 'lazy write' cache.
 * Cache conflicts are solved by moving conflicting cells in a linear buffer called "the reserve".
 * FIXME: add a flag in the main cache to tell there is conflicting content for this cell.
 */
#define MEM_C
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "mem/_mem.h"
#include "mem/geoalloc.h"
#include "mem/mem_log.h"
#define LOG_CURRENT LOG_MEM
#include "log/log.h"

// --------------------------------------
// Main RAM Direct-Mapped Cache
// --------------------------------------

struct s_mem mem[MEMSIZE];

// --------------------------------------
// RAM Cache "Reserve"
// Where modified cells go on cache conflict.
// TODO replace it by cuckoo hashing or smarter alternative
// --------------------------------------

struct s_reserve reserve[RESERVESIZE];

Address reserveHead = 0; ///< cache reserve stack head

static uint32_t pokes = 0; /// this counter helps identifies write operations.

// --------------------------------------
// RAM Cache Change Log
// It logs all changes made. It grows geometrically via geoalloc
// --------------------------------------

static Address *log = NULL;  ///< dynamically allocated change log
static uint32_t logMax = 0;  ///< heap allocated log size
static uint32_t logSize = 0; ///< significative log size

// --------------------------------------
// RAM Cache Stats for debugging purpose
// --------------------------------------

time_t lastCommitTime = 0;
time_t mem_lastCommitted = 0;

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
} mem_stats_zero = {0, 0, 0, 0, 0, 0, 0, 0, 0},
  mem_stats = {0, 0, 0, 0, 0, 0, 0, 0, 0};

/** Get a cell */
int mem_get_advanced(Address a, CellBody *pCellBody, uint32_t *stamp_p) {
  TRACEPRINTF("mem_get(%06x) begin", a);
  Address i;
  Address offset = a % MEMSIZE;
  uint32_t page = a / MEMSIZE;
  mem_stats.getCount++;
  struct s_mem *m = &mem[offset];

  assert(mem0_isOpened());

  if (!memIsEmpty(m) && m->page == page) {
    // cache hit
    DEBUGPRINTF("mem_get cache hit");
    if (stamp_p != NULL)
      *stamp_p = m->stamp;
    mem_stats.mainFound++;
    *pCellBody = m->c;
    return 0;
  }

  for (i = 0; i < reserveHead; i++) {
    if (reserve[i].a == a) {
      // reserve hit
      DEBUGPRINTF("mem_get reserve hit");
      if (stamp_p != NULL)
        *stamp_p = reserve[i].stamp;
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
    Address moved = m->page * MEMSIZE + offset;
    DEBUGPRINTF("mem_get moves %06x from mem to reserve", moved);
    if (reserveHead >= RESERVESIZE) {
      LOGPRINTF(LOG_ERROR,
                "reserve full :( :( logSize=%d getCount=%d setCount=%d "
                "reserveMovesBecauseSet=%d"
                " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d "
                "mainFound=%d reserveFound=%d",
                logSize, mem_stats.getCount, mem_stats.setCount,
                mem_stats.reserveMovesBecauseSet,
                mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
                mem_stats.notFound, mem_stats.mainFound,
                mem_stats.reserveFound);
      return -1;
    }
    reserve[reserveHead].a = moved;
    reserve[reserveHead].stamp = m->stamp;
    reserve[reserveHead++].c = m->c;
    mem_stats.reserveMovesBecauseGet++;
  }

  mem[offset].page = page;
  mem[offset].flags = 0;
  mem[offset].stamp = pokes;
  if (stamp_p != NULL)
    *stamp_p = pokes;
  mem_stats.notFound++;
  mem0_get(a, &mem[offset].c);
  *pCellBody = mem[offset].c;
  return 0;
}

/** Open the memory for I/O
 * if mem_lastCommited is different than mem0_lastModified, it means
 * that a concurrent system did change the persistence file,
 * the cache is out of sync and need to be reset
 *
 */
int mem_open() {
  if (mem0_isOpened()) {
    DEBUGPRINTF("mem already open");
    return 0;
  }

  if (mem0_open())
    return -1;

  if (mem_lastCommitted != 0 && mem_lastCommitted != mem0_lastModified) {
    // Reset cache because out of sync
    for (int i = 0; i < MEMSIZE; i++) {
      mem[i].flags = MEM1_EMPTY;
      mem[i].page = 0;
      mem[i].stamp = 0;
    }
  }
  mem_lastCommitted = mem0_lastModified;

  return 0;
}

/** Close the memory */
int mem_close() {
  if (!mem0_isOpened()) {
    DEBUGPRINTF("mem already closed.");
    return 0;
  }

  return mem0_close();
}

/** Get a cell */
int mem_get(Address a, CellBody *pCellBody) {
  return mem_get_advanced(a, pCellBody, NULL);
}

/** Set a cell */
int mem_set(Address a, CellBody *pCellBody) {
  TRACEPRINTF("mem_set(%06x) begin", a);
  Address offset = a % MEMSIZE;
  uint32_t page = a / MEMSIZE;
  mem_stats.setCount++;
  struct s_mem *m = &mem[offset];
  ONDEBUG(MEM_LOG('R', offset));

  assert(mem0_isOpened());

  if (!memIsEmpty(m) && m->page != page) {       // Uhh that's not fun
    for (int i = reserveHead - 1; i >= 0; i--) { // Look at the reserve
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
    if (memIsChanged(
            m)) { // one replaces a modificied cell that one moves to reserve
      Address moved = m->page * MEMSIZE + offset;
      DEBUGPRINTF("mem_set moved %06x to reserve", moved);
      if (reserveHead >= RESERVESIZE) {
        LOGPRINTF(LOG_ERROR,
                  "reserve full :( :( logSize=%d getCount=%d setCount=%d "
                  "reserveMovesBecauseSet=%d"
                  " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d "
                  "mainFound=%d reserveFound=%d",
                  logSize, mem_stats.getCount, mem_stats.setCount,
                  mem_stats.reserveMovesBecauseSet,
                  mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
                  mem_stats.notFound, mem_stats.mainFound,
                  mem_stats.reserveFound);
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

  m->flags = MEM1_CHANGED;
  m->page = page;
  m->stamp = ++pokes;
  m->c = *pCellBody;
  ONDEBUG(MEM_LOG('W', offset));
  DEBUGPRINTF("mem_set end, logSize=%d", logSize);
  return 0;
}

/// cmp function for qsort in mem_commit
static int _addressCmp(const void *pa, const void *pb) {
  Address a = *(Address *)pa;
  Address b = *(Address *)pb;
  return (a == b ? 0 : (a > b ? 1 : -1));
}

//// commit modified cells
int mem_commit() {
  TRACEPRINTF("BEGIN mem_commit() logSize=%d", logSize);
  int i;
  Address a, offset;

  assert(mem0_isOpened());

  lastCommitTime = time(NULL);

  if (!logSize) {
    // nothing to commit
    assert(reserveHead == 0);
  } else {

    // sort memory log
    qsort(log, logSize, sizeof(Address), _addressCmp);

    // TODO: optimize by commiting all reserved cells first then all logged
    // cells found in mem
    for (i = 0; i < logSize; i++) {
      a = log[i];
      offset = a % MEMSIZE;
      CellBody c;
      if (mem_get(a, &c)) {
        ERRORPRINTF("END mem_commit() failed mem_get");
        return -1;
      }

      if (mem0_set(a, &c)) {
        ERRORPRINTF("END mem_commit() failed mem0_set");
        return -1;
      }

      mem[offset].flags &=
          0xFFFE; // reset changed flag for this offset (even if the changed
                  // cell is actually in reserve)
      ONDEBUG(MEM_LOG('W', offset));
    }
  }

  if (logSize) {
    if (mem0_commit()) {
      ERRORPRINTF("END mem_commit() failed mem0_commit");
      return -1;
    }
  }

  /* allows to detect concurrent access to data */
  mem_lastCommitted = mem0_lastModified;

  TRACEPRINTF("END mem_commit() logSize=%d getCount=%d setCount=%d "
              "reserveMovesBecauseSet=%d"
              " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d "
              "mainFound=%d reserveFound=%d",
              logSize, mem_stats.getCount, mem_stats.setCount,
              mem_stats.reserveMovesBecauseSet,
              mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
              mem_stats.notFound, mem_stats.mainFound, mem_stats.reserveFound);
  zeroalloc((char **)&log, &logMax, &logSize);
  mem_stats = mem_stats_zero;
  reserveHead = 0;
}

/** yield
 * may lead to intermediate commit and mem0 release
 * @return 1 if effective commit, -1 on failure, 0 if nothing happens
 */
int mem_yield() {
  time_t now = time(NULL);

  double seconds = difftime(now, lastCommitTime);
  if (seconds > 1.0 || reserveHead > RESERVESIZE / 4) {
    if (mem_commit()) {
      return -1;
    }
  }
  return 0;
}

uint32_t mem_pokes() { return pokes; }

/** init
  @return 1 if very first start, <0 if error, 0 otherwise
*/
int mem_init() {
  TRACEPRINTF("mem_init (memSize = %d)", MEMSIZE);
  int rc = mem0_init();
  if (rc < 0)
    return rc;

  Address i;
  for (i = 0; i < MEMSIZE; i++) {
    mem[i].flags = MEM1_EMPTY;
    mem[i].page = 0;
    mem[i].stamp = 0;
  }

  geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), 0);

  lastCommitTime = time(NULL);

  return rc;
}

int mem_destroy() {
  free(log);
  mem0_destroy();
}
