// memory level 0 data storage API

#ifndef MEM0_H
#define MEM0_H
typedef unsigned int uint32;
typedef uint32 Cell;
typedef uint32 CellData;
typedef CellData Address;

// initialization
init mem0_init(); // return !0 if very first init

// Twin Prims prevent clustering when open-addressing
// The nearest twin prims lower than 8388608 (2^23) is 2^23-157 and 2^23-159
// Thanks to http://primes.utm.edu/lists/2small/0bit.html

const Address PRIM0 = ((1 << 23) - 157);
const Address PRIM1 = ((1 << 23) - 159);
const Address SPACE_SIZE = PRIM0;

// Persistence file
#define PERSISTENCE_DIR "~/.entrelacs"
#define PERSISTENCE_FILE "entrelacs.dat"
#define PERSISTENCE_ENV "ENTRELACS"

// mem0 accessors
Cell mem0_get(Address);
void mem0_set(Address, Cell);

// prototype only methods
void mem0_saveData(char *h, size_t size, uchar* data);
void mem0_deleteData(char* h);
#endif
