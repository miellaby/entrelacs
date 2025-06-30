
#pragma once
#include "mem/mem.h"
void mem_log(int line, char operation, Address offset);
#define MEM_LOG(operation, offset) mem_log(__LINE__, operation, offset)
void mem_show(Address a);
int  mem_whereIs(Address a);
