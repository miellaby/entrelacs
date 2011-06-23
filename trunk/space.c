#define SPACE_C
#include <stdlib.h>
#include <assert.h>
#include "space.h"

#ifndef PRODUCTION
#include "stdio.h"
  #define DEBUG(w) w
#else
  #define DEBUG(w)
#endif

// The entrelacs space code.
// It consists in a RAM cache in front of the memory 0 level storage.
// Todo: Cuckoo hashing
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
#define memIsChanged(M) ((M).a & MEM1_CHANGED) 


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

/* Get memory level 1 meta-data associated with a cell */
uint32_t space_getAdmin(Address a) {
  DEBUG((fprintf(stderr, "space_getAdmin@%06x = ", a)));
  int i;
  Address offset = a & 0xFFF;
  uint32_t  page   = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page) {
    uint32_t mem1admin =  (m.a & 0xF) >> 1;
    DEBUG((fprintf(stderr, "%01x from cache\n", mem1admin)));
    return mem1admin;
  }
  
  for (i = 0; i < reserveHead ; i++) {
    if ((reserve[i].a >> 4) == a) {
      uint32_t mem1admin = (reserve[i].a & 0xF) >> 1;
      DEBUG((fprintf(stderr, "%01x from reserve\n", mem1admin)));
      return mem1admin;
    }
  }

  // for a newly loaded cell: mem1admin = 0;
  DEBUG((fprintf(stderr, "0 because not cached\n")));  
  return 0;
}

/* Get a cell */
Cell space_get(Address a) {
  DEBUG((fprintf(stderr, "space_get@%06x = ", a)));
  Address i;
  Address offset = a & 0xFFF;
  uint32_t  page   = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page) {
    DEBUG((fprintf(stderr, "%016llx from cache\n", m.c)));
    return m.c;
  }
  
  for (i = 0; i < reserveHead ; i++) {
    if ((reserve[i].a >> 4) == a) {
      DEBUG((fprintf(stderr, "%016llx from reserve\n", reserve[i].c)));
      return reserve[i].c;
    }
  }

  if (memIsChanged(m)) {
    // When replacing a modified cell, move it to reserve
    DEBUG((fprintf(stderr, "(reserve move) ")));
    assert(reserveHead < reserveSize);
    reserve[reserveHead].a = ((m.a & 0xFFF0) << 8) | (offset << 4) | (m.a & 0xF) ;
    reserve[reserveHead++].c = m.c;
  }

  mem[offset].a = page << 4; // for a newly loaded cell: mem1admin = 0;
  return (mem[offset].c = mem0_get(a));
  // rem: debug trace end put by mem0_get
}

void space_set(Address a, Cell c, uint32_t mem1admin) { // FIXME What if cell is in reserve?
  DEBUG((fprintf(stderr, "space_set@%06x : %016llx A1=%01x ", a, c, mem1admin)));
  Address offset = a & 0xFFF;
  uint32_t page    = a >> 12;
  mem1admin = (mem1admin & 7) << 1; // Note one shift admin bits once left here
  m = mem[offset];
  if (memIsEmpty(m) || memPageOf(m) != page)  { // Uhh that's not fun
    for (int i = reserveHead - 1; i >=0 ; i--) { // Look at the reserve
      if ((reserve[i].a >> 4) == a) {
         DEBUG((fprintf(stderr, "(found in reserve)\n")));
		 // one changes data in the reserve cell
		 if (mem1admin != DONTTOUCH)
  		     reserve[i].a = a << 4 | mem1admin | MEM1_CHANGED;
		 reserve[i].c = c;
		 return; // no need to log it (it's already done)
      }
    }
	// no copy in the reserve, one puts the modified cell in mem0
    if (memIsChanged(m)) { // one replaces a modificied cell that one moves to reserve
       DEBUG((fprintf(stderr, "(reserve move) ")));
       assert(reserveHead < reserveSize);
       reserve[reserveHead].a = ((m.a & 0xFFF0) << 8) | (offset << 4) | (m.a & 0xF);
       reserve[reserveHead].c = m.c;
       reserveHead++;
	}
	// No need to load the cell from mem0 since one overwrites it
    m.a = 0; // previous mem1admin ignored.
  }

  if (!memIsChanged(m)) { // Log it
    geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), logSize + 1);
    log[logSize - 1] = a;
  }
  
  mem[offset].a = page << 4 | (mem1admin == DONTTOUCH ? (m.a & 0xF) : mem1admin) | MEM1_CHANGED;
  mem[offset].c = c;
  DEBUG((fprintf(stderr, "logSize=%d\n", logSize)));
}


void space_setAdmin(Address a, uint32_t mem1admin) {
  DEBUG((fprintf(stderr, "space_setAdmin@%06x : %01x ", a, mem1admin)));
  Address offset = a & 0xFFF;
  uint32_t page    = a >> 12;
  m = mem[offset];
  if (!memIsEmpty(m) && memPageOf(m) == page) { // ideal case
      if ((m.a >> 1 & 7) == mem1admin) {
         DEBUG((fprintf(stderr, " nothing to do.\n")));
	     return;
	  }
  
      if (!memIsChanged(m)) { // Log it
        geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), logSize + 1);
        log[logSize - 1] = a;
      }
      mem1admin = (mem1admin & 7) << 1;
      mem[offset].a = page << 4 | mem1admin | MEM1_CHANGED;
	  // FIXME actually, one should distinguish between admin-changed cell and really modified cell
      DEBUG((fprintf(stderr, " in cache.\n")));
  } else { // oww the pain
      DEBUG((fprintf(stderr, " ==> space_set(space_get(...))\n", a, mem1admin)));
	  if (space_getAdmin(a) != mem1admin)
         return space_set(a, space_get(a), mem1admin); // TODO Might be optimized
  }
}

static int _addressCmp(const void *a, const void *b) {
  return (a == b ? 0 : (a > b ? 1 : -1));
}

void space_commit() {
  DEBUG((fprintf(stderr, "space_commit (logSize = %d)\n", logSize)));
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
  DEBUG((fprintf(stderr, "space_commit done\n")));
}

int space_init() {
  DEBUG((fprintf(stderr, "space_init (memSize = %d)\n", memSize)));
  int firstTime = mem0_init();
  Address i;
  for (i = 0; i < memSize; i++) {
    mem[i].a = MEM1_EMPTY;
  }

  geoalloc((char **)&log, &logMax, &logSize, sizeof(Address), 0);
  
  return firstTime; // return !0 if very first start
}
