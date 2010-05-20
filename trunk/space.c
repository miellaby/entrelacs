#define SPACE_C
#include <stdlib.h>

#include "space.h"

// The entrelacs space code.
// It consists in a RAM cache in front of the memory 0 level storage.
#define MEMSIZE 0x1000
#define RESERVESIZE 256
static const Address memSize = MEMSIZE ; // cache size (4096)
static const Address reserveSize = RESERVESIZE ; // cache reserve size

// The RAM cache : an n="size" array of record. Each record can carry one mem0 cell. 
static struct s_mem {
  unsigned short a; // <--page # (12 bits)--><--mem1admin (3 bits)--><--CHANGED (1 bit)-->
  Cell c;
} m, mem[MEMSIZE];


// The RAM cache reserve.
// Modified cells are put into this reserve when their location are needed to load other cells
static struct s_reserve {
  Address a; // <--padding (4 bits)--><--cell address (24 bits)--><--mem1admin (3 bits)--><-- CHANGED (1 bit) -->
  Cell    c;
} reserve[RESERVESIZE]; // cache reserve stack
static uint32_t reserveHead = 0; // cache reserve stack head

// The change log.
// This is a log of all changes made in cached memory.
static Address* log = NULL;
static size_t   logMax = 0;
static size_t   logSize = 0;



#define MEM1_CHANGED 0x1
#define MEM1_EMPTY   0xE
#define memPageOf(M) ((M).a >> 4) 
#define memIsEmpty(M) ((M).a == MEM1_EMPTY) 
#define memIsCHANGED(M) ((M).a & MEM1_CHANGED) 


// you don't want to know
void zeroalloc(char** pp, size_t* maxp, size_t* sp) {
  if (*maxp) {
    free(*pp);
    *maxp = 0;
    *pp = 0;
  }
  *sp = 0;
}

// you don't want to know
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s) {
  if (!*maxp)	{
      *maxp = (s < 128 ? 256 : s + 128 );
      *pp = (char *)malloc(*maxp * us);
      assert(*pp);
  } else {
    while (s > *maxp) {
      *maxp *= 2;
      *pp = (char *)realloc(*pp, *maxp * us);
      assert(*pp);
    }
  }
  *sp = s;
  return;
}

Cell space_getAdmin(Address a) {
  Address i;
  Address offset = a & 0xFFF;
  uint32_t  page   = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page)
    return m.a & 0xF;

  for (i = 0; i < reserveHead ; i++) {
    if ((reserve[i].a >> 4) == a)
      return reserve[i].a & 0xF;
  }
  return 0;
}


Cell space_get(Address a) {
  Address i;
  Address offset = a & 0xFFF;
  uint32_t  page   = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page)
    return m.c;

  for (i = 0; i < reserveHead ; i++) {
    if ((reserve[i].a >> 4) == a)
      return reserve[i].c;
  }

  if (memIsChanged(m)) {
    // move it to reserve
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = ((m.a & 0xFFF0) << 8) | (offset << 4) | (m.a & 0xF) ;
    reserve[reserveHead++].c = m.c;
  }

  mem[offset].a = page << 4; // for a newly loaded cell: mem1admin = 0;
  return (mem[offset].c = mem0_get(a));
}

void space_set(Address a, Cell c, uint32_t mem1admin) {
  Address offset = a & 0xFFF;
  uint32_t page    = a >> 12;
  mem1admin = (mem1admin & 7) << 1;
  m = mem[offset];
  if (memIsChanged(m) && memPageOf(m) != page)  {
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = ((m.a & 0xFFF0) << 8) | (offset << 4) | (m.a & 0xF);
    reserve[reserveHead].c = m.c;
    reserveHead++;
  }
  mem[offset].a = page << 4 | (mem1admin == DONTTOUCH ? (m.a & 0xF) : mem1admin) | MEM1_CHANGED;
  mem[offset].c = c;
  geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), logSize + 1);
  log[logSize - 1] = a;
}


void space_setAdmin(Address a, uint32_t mem1admin) {
  Address offset = a & 0xFFF;
  uint32_t page    = a >> 12;
  mem1admin = (mem1admin & 7) << 1;
  m = mem[offset];
  if (memIsChanged(m) && memPageOf(m) != page)  {
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = ((m.a & 0xFFF0) << 8) | (offset << 4) | (m.a & 0xF);
    reserve[reserveHead].c = m.c;
    reserveHead++;
  }
  mem[offset].a = page << 4 | mem1admin | MEM1_CHANGED;
}

static int _addressCmp(const void *a, const void *b) {
  return (a == b ? 0 : (a > b ? 1 : -1));
}

void space_commit() {
  Address i;
  if (!logSize) return;

  qsort(log, logSize, sizeof(Address), _addressCmp);
  for (i = 0; i < logSize; i++) {
    Address a = log[i];
    mem0_set(a, space_get(a));
    mem[a & 0xFFF].a &= 0xFFFE;
  }
  zeroalloc((char **)&log, &logMax, &logSize);
  reserveHead = 0;
}

int space_init() {
  int firstTime = mem0_init();
  Address i;
  for (i = 0; i < memSize; i++) {
    mem[i].a = MEM1_EMPTY;
  }

  geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), 0);
  
  return firstTime; // return !0 if very first start
}
