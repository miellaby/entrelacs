/** @file
 Volatile memory device.
 */
#define RAM_C
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#define LOG_CURRENT LOG_RAM
#include "log.h"
#include "ram.h"

static const RamAddress ramSize = RAMSIZE; ///< ram size

/** The main RAM cache.
 An array of n=ramSize "RAM cells".
 */
static RamCell ramBank[RAMSIZE];


/** Get a cell */
int ram_get(RamAddress a, RamCell* pRamCell) {
  *pRamCell = ramBank[a];
  return 0;
}

/** Set a cell */
int ram_set(RamAddress a, RamCell* pRamCell) {
    ramBank[a] = *pRamCell;
    return 0;
}


/** init
  @return 0
*/
int ram_init() {
  memset(ramBank, sizeof(ramBank), 0);
  return 0;
}

int ram_destroy() {
}
