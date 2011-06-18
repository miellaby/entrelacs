#ifndef MEM0_H
#define MEM0_H
// memory level 0 data storage API

#include <stdint.h>
#include <sys/types.h>

typedef uint64_t Cell;
typedef uint64_t CellData;
typedef uint32_t Address;

// initialization
int mem0_init(); // return !0 if very first init

// Twin Prims prevent clustering when open-addressing
// The nearest twin prims lower than 8388608 (2^23) is 2^23-157 and 2^23-159
// Thanks to http://primes.utm.edu/lists/2small/0bit.html

#define _PRIM0 ((1 << 23) - 157)

const Address PRIM0;
const Address PRIM1;
const Address SPACE_SIZE;
#ifdef MEM0_C
const Address PRIM0 = _PRIM0;
const Address PRIM1 = (_PRIM0 - 2);
const Address SPACE_SIZE = _PRIM0;
#endif

// Persistence file
#define PERSISTENCE_DIR "~/.entrelacs"
#define PERSISTENCE_FILE "entrelacs.dat"
#define PERSISTENCE_ENV "ENTRELACS"

// mem0 accessors
Cell mem0_get(Address);
void mem0_set(Address, Cell);

// prototype only methods
void   mem0_saveData(char *h, size_t size, char* data);
char*  mem0_loadData(char* h, size_t* sizeP);
void   mem0_deleteData(char* h);
#endif
