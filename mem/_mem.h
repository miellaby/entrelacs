/** @file _mem.h
 Orthogonally persistent memory device.
 Internal header
 */
#include <stdlib.h>
#include "mem/mem.h"

// --------------------------------------
// Main RAM Cache
// --------------------------------------

// 0x100000 = 1048576 cells
#define MEMSIZE (0x100000)

// RAM memory slot
typedef struct s_mem {
    CellBody c; ///< mem0 cell
    uint16_t flags; ///< mem1 flags (16 bits)
    uint16_t page;  ///< page = cell adress / MEMSIZE (16 bits)
    uint16_t stamp;
} Mem1;

extern Mem1 mem[MEMSIZE]; ///<  The main RAM cache. AN array of MEMSIZE slots

#define MEM1_CHANGED ((uint16_t)0x0001) // set when mem1.c is modified
#define MEM1_RESERVE ((uint16_t)0x0010) // set when reserve contain a offset % MEMSIZE cell
#define MEM1_EMPTY ((uint16_t)0x0100)   // unloaded memory slot

#define mem1_isEmpty(M) ((M)->flags == MEM1_EMPTY) // TODO stamp == 0
#define mem1_isChanged(M) ((M)->flags & MEM1_CHANGED) // TODO stamp > mem_revision()
#define mem1_hasReserve(M) ((M)->flags & MEM1_RESERVE)

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
