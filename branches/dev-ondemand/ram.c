/** @file
 Volatile memory device.
 */
#define RAM_C
#include <stdlib.h>
#include <assert.h>
#define LOG_CURRENT LOG_RAM
#include "log.h"
#include "ram.h"

static const Address ramSize = RAMSIZE ; ///< ram size

/** The main RAM cache, aka "ram".
 ram is an array of ramSize records.
 Each record can carry one mem0 cell.
 */
static RamCell ram[ramSize];


/** Get a cell */
int ram_get(Address a, RamCell* pRamCell) {
  *pRamCell = &ram[a];
  return 0;
}

/** Set a cell */
int ram_set(Address a, RamCell *pRamCell) {
    ram[a] = *pRamCell;
    return 0;
}


/** init
  @return 0
*/
int ram_init() {
  memset(ram, sizeof(ram), 0);
  return 0;
}

int ram_destroy() {
}
