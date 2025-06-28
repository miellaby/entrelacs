/** @file _mem.h
 Orthogonally persistent memory device.
 Internal header
 */
#include <stdlib.h>
#include "mem.h"

// --------------------------------------
// Main RAM Cache
// --------------------------------------

// 0x100000 = 1048576 cells
#define MEMSIZE (0x100000)

struct s_mem {
    CellBody c; ///< mem0 cell
    uint16_t flags; ///< mem1 internal flags (16 bits)
    uint16_t page;  ///< page # (16 bits)
    uint16_t stamp;
};

extern struct s_mem mem[MEMSIZE]; ///<  The main RAM cache. AN array of MEMSIZE records

#define MEM1_CHANGED ((uint16_t)0x0001)
#define MEM1_EMPTY ((uint16_t)0x0100)
#define memIsEmpty(M) ((M)->flags == MEM1_EMPTY)
#define memIsChanged(M) ((M)->flags & MEM1_CHANGED)

// --------------------------------------
// RAM Cache "Reserve"
// Where modified cells go on hash conflict.
// TODO replace it by cuckoo hashing or smarter alternative
// --------------------------------------

struct s_reserve { // TODO : change the name
    CellBody c;  ///< mem0 cell
    Address a;   ///< mem0 address
    uint16_t stamp;
};

#define RESERVESIZE 1024
extern struct s_reserve reserve[RESERVESIZE]; ///< RAM cache reserve
extern Address reserveHead; ///< cache reserve stack head
