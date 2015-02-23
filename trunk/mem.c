/** @file
 Orthogonally persistent memory device.
 It basically consists in a software implemented direct-mapped RAM cache with buffering capacity in front of mem0.
 Changed cells in the current micro-transaction are buffered until a commit is performed (it's neither a write-trough nor a 'lazy write' cache).
 */
#define MEM_C
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#define LOG_CURRENT LOG_MEM
#include "log.h"
#include "mem.h"

#define MEMSIZE (0x100000)
// 0x10000 cells
static const Address memSize = MEMSIZE ; ///< mem size

/** The main RAM cache, aka "mem".
 mem is an array of MEMSIZE records.
 Each record can carry one mem0 cell.
 */
static struct s_mem {
  CellBody c;
  uint16_t flags; //< mem1 internal flags (16 bits)
  uint16_t page; //< page # (16 bits)
  uint16_t stamp;
} mem[MEMSIZE];

#define RESERVESIZE 1024
static const Address reserveSize = RESERVESIZE ; ///< cache reserve size
static uint32_t pokes = 0;
time_t lastCommitTime = 0;
time_t mem_lastCommitted = 0;

/** The RAM cache reserve.
 Modified cells are moved into this reserve when their location in mem[] are need to load other cells
 TODO replace it by cuckoo hashing or smarter alternative
 */
static struct s_reserve { // TODO : change the name
  CellBody c;
  Address a;
  uint16_t stamp;
} reserve[RESERVESIZE];

static uint32_t reserveHead = 0; ///< cache reserve stack head

/** The change log.
 This is a log of all changes made in cached memory. It grows geometrically (see geoalloc() function).
 */
static Address* log = NULL; ///< dynamically allocated
static uint32_t   logMax = 0; ///< heap allocated log size
static uint32_t   logSize = 0; ///< significative log size

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
    0, 0, 0, 0, 0, 0, 0, 0, 0
}, mem_stats = {
    0, 0, 0, 0, 0, 0, 0, 0, 0 
};


#define MEM1_CHANGED ((uint16_t)0x0001)
#define MEM1_EMPTY   ((uint16_t)0x0100)
#define memIsEmpty(M) ((M)->flags == MEM1_EMPTY)
#define memIsChanged(M) ((M)->flags & MEM1_CHANGED)


/** set-up or reset a tuple {allocated memory, allocated size, significative size} used by geoalloc() function */
void zeroalloc(char** pp, uint32_t* maxp, uint32_t* sp) {
  if (*maxp) {
    free(*pp);
    *maxp = 0;
    *pp = 0;
  }
  *sp = 0;
}

/** geometricaly grow a heap allocated memory area by providing a tuple {allocated memory, allocated size, current significative size} and a new significative size */
void geoalloc(char** pp, uint32_t* maxp, uint32_t* sp, uint32_t us, uint32_t s) {
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
int mem_get_advanced(Address a, CellBody* pCellBody, uint32_t* stamp_p) {
  TRACEPRINTF("mem_get(%06x) begin", a);
  Address i;
  Address offset = a % MEMSIZE;
  uint32_t  page = a / MEMSIZE;
  mem_stats.getCount++;
  struct s_mem* m = &mem[offset];

  assert(mem0_isOpened());
  
  if (!memIsEmpty(m) && m->page == page) {
    // cache hit
    DEBUGPRINTF("mem_get cache hit");
    if (stamp_p != NULL) *stamp_p = m->stamp;
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
    Address moved = m->page * MEMSIZE + offset;
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

  mem[offset].page = page;
  mem[offset].flags = 0;
  mem[offset].stamp = pokes;
  if (stamp_p != NULL) *stamp_p = pokes;
  mem_stats.notFound++;
  mem0_get(a, &mem[offset].c);
  *pCellBody = mem[offset].c;
  return 0;
}

/** log action on mem[offset]
 */
static void mem_log(int line, char operation, Address offset) {
  LOGPRINTF(LOG_DEBUG, "%03d %c@%d: page=%d, address=%d, flags=[%c%c] stamp=%d %s",
    line, operation, offset,
    (int)mem[offset].page, (int)mem[offset].page * memSize + offset,
    mem[offset].flags & MEM1_CHANGED ? 'C' : ' ',
    mem[offset].flags & MEM1_EMPTY ? 'E' : ' ', mem[offset].stamp,
    mem[offset].flags == MEM1_EMPTY ? "EMPTY!" : "");
      
}

/** debugging helper
 * which logs mem metadata for Arrow Id a
 */
static void mem_show(Address a) {
  Address offset = a % memSize;
  uint32_t  page = a / MEMSIZE;

  mem_log(__LINE__, 'R', offset);
  
  struct s_mem* m = &mem[offset];
  if (memIsEmpty(m) || m->page != page) { // cache miss
    // look at the reserve
    for (int i = 0; i < reserveHead ; i++) {
      if (reserve[i].a == a) { // reserve hit
        LOGPRINTF(LOG_DEBUG, "%06x MOD IN RESERVE stamp=%d", a, reserve[i].stamp);
      }
    }
  }
}

/** debugging helper
 * which tells about the caching status of a
 */
static int mem_whereIs(Address a) {
  Address offset = a % MEMSIZE;
  uint32_t  page = a / MEMSIZE;
  struct s_mem* m = &mem[offset];
  int i;
  if (!memIsEmpty(m) && m->page == page) { // cache hit
    INFOPRINTF("%06x LOADED %s stamp=%d", a,
                m->flags & MEM1_CHANGED ? "MODIFIED" : "", m->stamp);
    return 1;
  }
   
  for (i = 0; i < reserveHead ; i++) {
    if (reserve[i].a == a) { // reserve hit
      INFOPRINTF("%06x MOD IN RESERVE stamp=%d", a, reserve[i].stamp);
      return 2;
    }
  }
  
  return 0;
}

#define MEM_LOG(operation, offset) mem_log(__LINE__, operation, offset)

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
    for (int i = 0; i < memSize; i++) {
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
    uint32_t  page = a / MEMSIZE;
    mem_stats.setCount++;
    struct s_mem* m = &mem[offset];
    ONDEBUG(MEM_LOG('R', offset));
  
    assert(mem0_isOpened());

    if (!memIsEmpty(m) && m->page != page)  { // Uhh that's not fun
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
            Address moved = m->page * MEMSIZE + offset;
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

    m->flags = MEM1_CHANGED;
    m->page = page;
    m->stamp = ++pokes;
    m->c = *pCellBody;
    ONDEBUG(MEM_LOG('W', offset));
    DEBUGPRINTF("mem_set end, logSize=%d", logSize);
    return 0;
}

/** cmp function for qsort in mem_commit
 */
static int _addressCmp(const void *pa, const void *pb) {
  Address a = *(Address *)pa;
  Address b = *(Address *)pb;
  return (a == b ? 0 : (a > b ? 1 : -1));
}

/** commit */
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
  
    // TODO: optimize by commiting all reserved cells first then all logged cells found in mem
    for (i = 0; i < logSize; i++) {
      a = log[i];
      offset = a % memSize;
      CellBody c;
      if (mem_get(a, &c)) {
        ERRORPRINTF("END mem_commit() failed mem_get");
        return -1;
      }
  
      if (mem0_set(a, &c)) {
        ERRORPRINTF("END mem_commit() failed mem0_set");
        return -1;
      }
      
      mem[offset].flags &= 0xFFFE; // reset changed flag for this offset (even if the changed cell is actually in reserve)
      ONDEBUG(MEM_LOG('W', offset));
    }
  }
  
  if (logSize) {
    if  (mem0_commit()) {
      ERRORPRINTF("END mem_commit() failed mem0_commit");
      return -1;
    }
  }
  
  /* allows to detect concurrent access to data */
  mem_lastCommitted = mem0_lastModified;
  
  TRACEPRINTF("END mem_commit() logSize=%d getCount=%d setCount=%d reserveMovesBecauseSet=%d"
          " reserveMovesBecauseGet=%d mainReplaces=%d notFound=%d mainFound=%d reserveFound=%d",
          logSize, mem_stats.getCount, mem_stats.setCount,
          mem_stats.reserveMovesBecauseSet, mem_stats.reserveMovesBecauseGet, mem_stats.mainReplaces,
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
    if (seconds > 1.0 || reserveHead > reserveSize  / 4) {
        if (mem_commit()) {
          return -1;
        }
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
