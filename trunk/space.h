/* The entrelacs space API */

#include "mem0.h"

int  space_init(); // return !0 if very first start
void space_set(Address, Cell, uint32); // set
Cell space_get(Address); // get address
void space_commit(); // commit changes
void space_setAdmin(Address, uint32); // attach 3 admin bits to Address  
uint32 space_getAdmin(Address); // read 3 attached admin bits (0 if unmodified)
