#ifndef MEM_H
#define MEM_H
/*
 * Persistent memory device with RAM cache
 */
#include "mem0.h"

#define DONTTOUCH 0xFFFFFFFF

int  mem_init(); // return !0 if very first start
void mem_set(Address, Cell, uint32_t); // set
Cell mem_get(Address); // get address
void mem_commit(); // commit changes

void mem_setAdmin(Address, uint32_t); // attach 3 admin bits to Address
uint32_t mem_getAdmin(Address); // read 3 attached admin bits (0 if unmodified)

void zeroalloc(char** pp, size_t* maxp, size_t* sp);
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s);
#endif /* MEM_H */

