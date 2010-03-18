#include "mem0.h"

#ifdef SPACE_C
static const unsigned long mem1Size = 256 << 7 ; // mem level 1 size
static uint32 memory1[mem1Size]; // here is the whole thing
#endif

Address space_get(Address);
void    space_set(Address, Address);
void    space_commit();
int     space_init(); // return !0 if very first start
