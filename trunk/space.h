#include "mem0.h"

Cell space_get(Address);
void space_set(Address, Cell);
void space_commit();
int  space_init(); // return !0 if very first start
