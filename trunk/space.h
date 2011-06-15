/* The entrelacs space API */

#include "mem0.h"
#define DONTTOUCH -1
int  space_init(); // return !0 if very first start
void space_set(Address, Cell, uint32_t); // set
Cell space_get(Address); // get address
void space_commit(); // commit changes
void space_setAdmin(Address, uint32_t); // attach 3 admin bits to Address  
uint32_t space_getAdmin(Address); // read 3 attached admin bits (0 if unmodified)
void zeroalloc(char** pp, size_t* maxp, size_t* sp);
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s);
