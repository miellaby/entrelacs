/** @file
 * Utilities to log mem.c structures
 */
#define MEM_LOG_C
#include "mem_log.h"
#include <stdlib.h>
#define LOG_CURRENT LOG_MEM
#include "log.h"
#include "_mem.h"

/** log action on mem[offset]
 */
void mem_log(int line, char operation, Address offset) {
  LOGPRINTF(LOG_DEBUG, "%03d %c@%d: page=%d, address=%d, flags=[%c%c] stamp=%d %s",
    line, operation, offset,
    (int)mem[offset].page, (int)mem[offset].page * MEMSIZE + offset,
    mem[offset].flags & MEM1_CHANGED ? 'C' : ' ',
    mem[offset].flags & MEM1_EMPTY ? 'E' : ' ', mem[offset].stamp,
    mem[offset].flags == MEM1_EMPTY ? "EMPTY!" : "");
      
}

/** debugging helper
 * which logs mem metadata for Arrow Id a
 */
void mem_show(Address a) {
  Address offset = a % MEMSIZE;
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
int mem_whereIs(Address a) {
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
