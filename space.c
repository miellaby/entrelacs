#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "entrelacs/entrelacs.h"
#include "mem0.h"
#include "mem.h"
#include "sha1.h"

#ifndef PRODUCTION
#define ONDEBUG(w) (fprintf(stderr, "%s:%d ", __FILE__, __LINE__), w)
#else
#define ONDEBUG(w)
#endif

/*
 * Eve
 */
#define EVE (0)
const Arrow Eve = EVE;
/*
 * Cell category bits
 */
#define CATBITS_MASK   0x0700000000000000LLU
#define CATBITS_ARROW  0x0000000000000000LLU
#define CATBITS_TUPLE  0x0100000000000000LLU
#define CATBITS_SMALL  0x0200000000000000LLU
#define CATBITS_TAG    0x0300000000000000LLU
#define CATBITS_BLOB   0x0400000000000000LLU
#define CATBITS_CHAIN  0x0500000000000000LLU
#define CATBITS_LAST   0x0600000000000000LLU
#define CATBITS_SYNC   0x0700000000000000LLU

/*
 * Reattachment cell type
 */
 #define REATTACHMENT_FIRST  0
 #define REATTACHMENT_NEXT   1
 #define REATTACHMENT_CHILD0 2

/*
 * ROOTBIT
 */
#define ROOTBIT_ROOTED 0x80
#define ROOTBIT_UNROOTED 0

/* ________________________________________
 * Cell building macros
 *
 *   Cell structure overview:
 *  ---------------------------
 *
 *   <-M-><-T-><----------------data--------------------> : a cell (64 bits)
 *  with
 *    M = "More" counter (5 bits)
 *    T = Category ID (3 bits)
 *    data = Cell content depending on category (56 bits)
 *
 *   Structure per category:
 *  -------------------------
 *
 *         +---<category>
 *         |
 *   <-M-> 0 <----tail@----> <----head@----> <R><-C-> : an arrow
 *   <-M-> 1 <--checksum----------> <--J---> <R><-C-> : a tuple
 *   <-M-> 2 <--data-----------------------> <R><-C-> : a small
 *   <-M-> 3 <--checksum----------> <--J---> <R><-C-> : a tag
 *   <-M-> 4 <--checksum----------> <--J---> <R><-C-> : a blob
 *   <-M-> 5 <--end@-------> <--end@-------> <--J---> : 2 ends of a tuple
 *   <-M-> 6 <--end@-------> <--end@-------> <S= 0/1> : Tuple last 2 ends
 *                                                       (size = 0 => 1 end only)
 *   <-M-> 5 <-----slice-------------------> <--J---> : blob/tag slice
 *   <-M-> 6 <-----slice---->    nothing     <slice-size> : blob/tag last slice
 *                                                         (size in bytes)
 *   <-M-> 5 <--child@-----> <--child@-----> <--J---> : 2 children
 *   <-M-> 6 <--child@-----> <--child@-----> <S= 0/1> : last 2 children
 *                                                        (size = 0 => 1 child)
 *   <-M-> 7 <--from@------> <--next@------> <Type>   : cell chain reattachment
 *                                             Type = 0 : Jump chain reattachment
 *                                             Type = 1 : Child0 chain reattachment
 * with
 *   R = Root flag (1 bit)
 *   C = To first child jump, h-child multiplier (7 bits)
 *   J = To next cell jump, h-sequence multiplier (1 byte)
 *   tail@, head@, end@, child@, child0@, moved@ : Cell address (3 bytes)
 *   data, slice = Data / data slice (6 bytes)
 *   checksum = Long definition checksum (40 bits / 5 bytes)
 */

#define get_moreBits(CELL) ((CELL) & 0xF800000000000000LLU)


// This remaining part is useless as attributs are always zero when building: | ((rooted) << 7) | (child0)

#define arrow_build(CELL, tail, head)  (get_moreBits(CELL) | CATBITS_ARROW | ((Cell)(tail) << 32) | ((Cell)(head) << 8))

#define tuple_build(CELL, checksum, J) (get_moreBits(CELL) | CATBITS_TUPLE | ((Cell)(checksum) << 16) | ((J) << 8))

#define small_build(CELL, data)        (get_moreBits(CELL) | CATBITS_SMALL | ((Cell)(data) << 8))

#define tag_build(CELL, checksum, J)   (get_moreBits(CELL) | CATBITS_TAG | (((Cell)(checksum)) << 16) | ((J) << 8))

#define blob_build(CELL, checksum, J)  (get_moreBits(CELL) | CATBITS_BLOB | (((Cell)(checksum)) << 16) | ((J) << 8))

#define refs_build(CELL, r0, r1, J)               (get_moreBits(CELL) | CATBITS_CHAIN | ((Cell)(r0) << 32) | ((r1) << 8) | (J))

#define lastRefs_build(CELL, r0, r1, S)           (get_moreBits(CELL) | CATBITS_LAST | ((Cell)(r0) << 32) | ((r1) << 8) | (S))

#define slice_build(CELL, data, J)                (get_moreBits(CELL) | CATBITS_CHAIN | ((Cell)(data) << 8) | (J))

#define lastSlice_build(CELL, data, S)            (get_moreBits(CELL) | CATBITS_LAST | (((Cell)data) << 8) | (S))

#define sync_build(CELL, from, next, type)        (get_moreBits(CELL) | CATBITS_SYNC | ((Cell)(from) << 32) | ((next) << 8) | (type))

#define cell_setRootBit(cell, ROOTBIT) (((cell) & 0xFFFFFFFFFFFFFF7FLLU) | (ROOTBIT))

#define cell_chain(cell, jump) (((cell) & 0xF8FFFFFFFFFFFF00LLU) | CATBITS_CHAIN | (jump))

#define cell_unchain(cell) (((cell) & 0xF8FFFFFFFFFFFF00LLU) | CATBITS_LAST)

#define cell_chainChild0(cell, jump) (((cell) & 0xFFFFFFFFFFFFFF80LLU) | (jump))


/*
 * "More" counter management (all cells)
 */

#define cell_isOvermore(C) (((C) & 0xF800000000000000LLU) == 0xF800000000000000LLU)

/** "more" a C cell.
 * increments its more counter if not to the max
 */
#define cell_more(C) (cell_isOvermore(C) ? (C) : ((C) + 0x0800000000000000LLU))
/** "unmore" a C cell.
 * decrements its more counter if not to the max
 */
#define cell_unmore(C) (cell_isOvermore(C) ? (C) : ((C) - 0x0800000000000000LLU))

/** change C cell into a freed cell.
 */
#define cell_free(C) ((C) & 0xF800000000000000LLU)

/* ________________________________________
 * Cell administrative bits
 */
#define MAX_JUMP   0xFF
#define MAX_CHILD0 0x7F

/* Administrative bits of volatile memory cached cell
 */
#define MEM1_LOOSE 0x1

/* ________________________________________
 * Cell unbuilding macros
 */
#define cell_getCatBits(C) ((C) & CATBITS_MASK)
#define cell_getMore(C) ((C) >> 59)
#define cell_getTail(C) (Arrow)(((C) & 0xFFFFFF00000000LLU) >> 32)
#define cell_getHead(C) (Arrow)(((C) & 0xFFFFFF00LLU) >> 8)
#define cell_getChecksum(C) (((C) & 0xFFFFFFFFFF0000LLU) >> 16)
#define cell_getSlice(C)    (((C) & 0xFFFFFFFFFFFF00LLU) >> 8)
#define cell_getSmall(C)    (((C) & 0xFFFFFFFFFFFF00LLU) >> 8)
#define cell_getRef0(C)    (Arrow)(((C) & 0xFFFFFF00000000LLU) >> 32)
#define cell_getRef1(C)    (Arrow)(((C) & 0xFFFFFF00LLU) >> 8)
#define cell_getChild0(C) (Address)((C) & 0x7FLLU)
#define cell_getRootBit(C) ((C) & 0x80LLU)
#define cell_getJumpFirst(C) (((C) & 0xFF00LLU) >> 8)
#define cell_getJumpNext(C) ((C) & 0xFFLLU)
#define cell_getSize(C) ((C) & 0xFFLLU)
#define cell_getSyncType(C) ((C) & 0xFFLLU)
#define cell_getFrom(C) (Address)cell_getTail(C)
#define cell_getNext(C) (Address)cell_getHead(C)

/* ________________________________________
 * Cell testing macros
 */
#define more(C) ((C) & 0xF800000000000000LLU)
#define cell_isRooted(C) (cell_getRootBit(C))
#define cell_isFree(C) (((C) & 0x07FFFFFFFFFFFFFFLLU) == 0)
#define cell_isPair(C) (cell_getCatBits(C) == CATBITS_ARROW)
#define cell_isTuple(C) (cell_getCatBits(C) == CATBITS_TUPLE)
// The next tricky macro returns !0 for any valid arrow address
#define cell_isArrow(C) (((C) & 0x07FFFFFFFFFFFFFFLLU) && cell_getCatBits(C) <= CATBITS_CHAIN)
#define cell_isSmall(C) (cell_getCatBits(C) == CATBITS_SMALL)
#define cell_isTag(C) (cell_getCatBits(C) == CATBITS_TAG)
#define cell_isBlob(C) (cell_getCatBits(C) == CATBITS_BLOB)
// The next trick returns !0 for entrelacs type arrows
#define cell_isEntrelacs(C) ((cell_getCatBits(C) + 7) & 0x0300000000000000LLU)
#define cell_isSyncFrom(C, address, type) (cell_getCatBits(C) == CATBITS_SYNC && cell_getFrom(C) == (address) && cell_getSyncType(cell) == (type))

// The loose stack.
// This is a stack of all newly created arrows still in loose state.
static Address* looseStack = NULL;
static Address  looseStackMax = 0;
static Address  looseStackSize = 0;

static void show_cell(Cell C, int ofSliceChain) {
   Cell catBits = cell_getCatBits(C);
   int  catI = catBits >> 56;
   int  more = (int)cell_getMore(C);
   Arrow tail = cell_getTail(C);
   Arrow head = cell_getHead(C);
   CellData checksum = (CellData)cell_getChecksum(C);
   CellData slice = (CellData)cell_getSlice(C);
   int  small = (int)cell_getSmall(C);
   Arrow ref0 = cell_getRef0(C);
   Arrow ref1 = cell_getRef1(C);
   int child0 = cell_getChild0(C);
   int rooted = cell_getRootBit(C) ? 1 : 0;
   int jumpFirst = cell_getJumpFirst(C);
   int jumpNext = cell_getJumpNext(C);
   int size = cell_getSize(C);
   int syncType = cell_getSyncType(C);
   Address from = cell_getFrom(C);
   Address next = cell_getNext(C);

   static char* cats[] = { "Arrow", "Tuple", "Small", "Tag", "Blob", "Chain", "Last", "Sync" };
   char* cat = cats[catI];

   if (cell_isFree(C)) {
     fprintf(stderr, "FREE (M=%02x)", more);
   } else if (cell_isPair(C)) {
      fprintf(stderr, "M=%02x C=%1x tail@=%06x head@=%06x R=%1x C=%02x (%s)", more, catI, tail, head, rooted, child0, cat); // an arrow
   } else if (cell_isTuple(C) || cell_isTag(C) || cell_isBlob(C)) {
      fprintf(stderr, "M=%02x C=%1x checksum=%010llx J=%02x R=%1x C=%02x (%s)", more, catI, checksum, jumpFirst, rooted, child0, cat); // a tuple/tag/blob
   } else if (cell_isSmall(C)) {
      fprintf(stderr, "M=%02x C=%1x small=%12x R=%1x C=%02x (%s)", more, catI, small, rooted, child0, cat); // a small
   } else if (catBits == CATBITS_SYNC) {
      fprintf(stderr, "M=%02x C=%1x from@=%06x next@=%06x T=%1x (%s)", more, catI, from, next, syncType, cat); // Sync cell
   } else if (catBits == CATBITS_CHAIN) {
      static char c;
#define charAt(SLICE, POS) ((c = ((char*)&SLICE)[POS]) ? c : '.')
      if (ofSliceChain) {
         fprintf(stderr, "M=%02x C=%1x slice=%012llx <%c%c%c%c%c%c> J=%02x (%s)", more, catI, slice,
           charAt(slice, 0), charAt(slice, 1), charAt(slice, 2), charAt(slice, 3), charAt(slice, 4), charAt(slice, 5), jumpNext, cat); // Chained cell with slice
      } else {
         fprintf(stderr, "M=%02x C=%1x ref0@=%06x ref1@=%06x J=%02x (%s)", more, catI, ref0, ref1, jumpNext, cat); // Chained cell with refs
      }
   } else if (catBits == CATBITS_LAST) {
      static char c;
      if (ofSliceChain) {
         fprintf(stderr, "M=%02x C=%1x slice=%012llx <%c%c%c%c%c%c> S=%02x (%s)", more, catI, slice,
           charAt(slice, 0), charAt(slice, 1), charAt(slice, 2), charAt(slice, 3), charAt(slice, 4), charAt(slice, 5),
           size, cat); // Last chained cell with slice
      } else {
         fprintf(stderr, "M=%02x C=%1x ref0@=%06x ref1@=%06x S=%02x (%s)", more, catI, ref0, ref1, size, cat); // Last chained cell with refs
      }
   } else {
      assert(0 == 1);
   }
   fprintf(stderr, "\n");
   fflush(stderr);
}

static void looseStackAdd(Address a) {
  geoalloc((char**)&looseStack, &looseStackMax, &looseStackSize, sizeof(Address), looseStackSize + 1);
  looseStack[looseStackSize - 1] = a;
}

static void looseStackRemove(Address a) {
  assert(looseStackSize);
  if (looseStack[looseStackSize - 1] == a)
    looseStackSize--;
  else if (looseStackSize > 1 && looseStack[looseStackSize - 2] == a) {
    looseStackSize--;
    looseStack[looseStackSize - 1] = looseStack[looseStackSize];
  }
}


/* ________________________________________
 * Hash functions
 * One computes 3 kinds of hashcode.
 * * hashLocation: default cell address of an arrow.
 *     hashLocation = hashArrow(tail, head) % PRIM0 for a regular arrow
 *     hashLocation = hashString(string) for a tag arrow
 * * hashProbe: probing offset when looking for a free cell nearby a conflict.
 *     hProbe = hashArrow(tail, head) % PRIM1 for a regular arrow
 * * hashChain: offset to separate chained cells
 *       chained data slices (and tuple ends) are separated by JUMP * h3 cells
 * * hashChild : offset to separate children cells
 *       first child in parent children list is at @parent + cell.child0 * hChild
 *       chained children cells are separated by JUMP * hChild cells
 */

/* hash function to get H1 from a regular arrow definition */
uint64_t hashArrow(Arrow tail, Arrow head) {
  return (tail << 20) ^ (tail >> 4) ^ head;
}

/* hash function to get H1 from a tag arrow definition */
uint64_t hashString(char *str) { // simple string hash
  uint64_t hash = 5381;
  char *s = str;
  int c;

  while (c = *s++) {
    // 5 bits shift only because ASCII generally works within 5 bits
    hash = ((hash << 5) + hash) + c;
  }

  ONDEBUG((fprintf(stderr, "hashString(\"%s\") = %016llx\n", str, hash)));
  return hash;
}

/* hash function to get H1 from a binary tag arrow definition */
uint64_t hashBString(char *buffer, uint32_t length) { // simple string hash
  uint64_t hash = 5381;
  int c;
  char *p = buffer;
  uint32_t l = length;
  if (l && !p[l - 1]) l--; // ignoring trailing \0 like hashString
  while (l--) { // all bytes but the last one
	c = *p++;
    // 5 bits shift only because ASCII generally works within 5 bits
    hash = ((hash << 5) + hash) + c;
  }

  ONDEBUG((fprintf(stderr, "hashBString(%d, \"%.*s\") = %016llx\n", length, length, buffer, hash)));
  return hash;
}

/* hash function to get hashChain from a cell caracteristics */
uint32_t hashChain(Address address, CellData cell) { // Data chain hash offset
  // This hash mixes the cell address and its content
  return ((cell & 0x00FFFFFFFFFF0000LLU) << 8) ^ ((cell & 0x00FFFFFFFFFF0000LLU) >> 16) ^ address;
}

/* hash function to get hashChild from a cell caracteristics */
uint32_t hashChild(Address address, CellData cell) { // Data chain hash offset
  return -hashChain(address, cell);
}

/* ________________________________________
 * The blob cryptographic hash computing fonction.
 * This function returns a string used to define a tag arrow.
 *
 */
char* crypto(uint32_t size, char* data, char output[20]) {
  sha1(data, size, output);
  return output;
}


/* ________________________________________
 * Address shifting and jumping macros
 * N : new location
 * H : current location
 * O : offset
 * S : start location
 * J : Jump multiplier
 * C : Cell
 */
#define shift(LOCATION, NEW, OFFSET) (NEW = (((LOCATION) + (OFFSET)) % (SPACE_SIZE)))

#define growingShift(LOCATION, NEW, OFFSET, START) ((shift(LOCATION, NEW, OFFSET) == (START)) ? shift(LOCATION, NEW,( ++OFFSET ? OFFSET :( OFFSET = 1 ))) : NEW)

#define jump(LOCATION, NEXT, JUMP, OFFSET, START) (NEXT = ((LOCATION + OFFSET * ( 1 + JUMP)) % SPACE_SIZE), assert(NEW != START))
static Address jumpToFirst(Cell cell, Address address, Address offset, Address start) {
   Address next = address;
   assert(cell_isArrow(cell));
   Cell jump = cell_getJumpFirst(cell);
   // Note jump=1 means 2 shifts!
   for (int i = 0; i <= jump; i++)
     growingShift(next, next, offset, address);

   if (jump == MAX_JUMP) { // one needs to look for a reattachment (sync) cell
     cell = mem_get(next);  ONDEBUG((show_cell(cell, 0)));
	 int loopDetect = 10000;
	 while (--loopDetect && !cell_isSyncFrom(cell, address, REATTACHMENT_FIRST)) {
        growingShift(next, next, offset, address);
		cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
	 }
	 assert(loopDetect);
     next = cell_getNext(cell);
   }
   return next;
}

static Address jumpToNext(Cell cell, Address address, Address offset, Address start) {
   Address next = address;
   assert(cell_getCatBits(cell) == CATBITS_CHAIN);
   Cell jump = cell_getJumpNext(cell);
   // Note jump=1 means 2 shifts!
   for (int i = 0; i <= jump; i++)
     growingShift(next, next, offset, address);

   if (jump == MAX_JUMP) { // one needs to look for a sync cell
     cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
	 int loopDetect = 10000;
	 while (--loopDetect && !cell_isSyncFrom(cell, address, REATTACHMENT_NEXT)) {
	    growingShift(next, next, offset, address);
        cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
	 }
	 assert(loopDetect);
     next = cell_getNext(cell);
   }
   return next;
}

static Address jumpToChild0(Cell cell, Address address, Address offset, Address start) {
   Address next = address;
   assert(cell_isArrow(cell));
   Cell jump = cell_getChild0(cell);
   if (!jump) return (Address)0; // no child when jump=0

   // Note jump=1 means 1 shift!
   for (int i = 0; i < jump; i++)
     growingShift(next, next, offset, address);

   if (jump == MAX_CHILD0) { // one needs to look for a sync cell
     cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
	 int loopDetect = 10000;
	 while (--loopDetect && !cell_isSyncFrom(cell, address, REATTACHMENT_CHILD0)) {
	    growingShift(next, next, offset, address);
        cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
	 }
	 assert(loopDetect);
     next = cell_getNext(cell);
   }
   return next;
}

/** return Eve */
Arrow xl_Eve() { return Eve; }

/** retrieve tail->head arrow
 * by creating the singleton if not found.
 * except if locateOnly param is set.
 */
static Arrow arrow(Arrow tail, Arrow head, int locateOnly) {
  uint64_t hash;
  Address hashLocation, hashProbe;
  Address probeAddress, firstFreeCell, newArrow;
  Cell cell;

  // Eve special case
  if (tail == Eve && head == Eve) {
    return Eve;
  }

  // Compute hashs
  hash = hashArrow(tail, head);
  hashLocation = hash % PRIM0; // base address
  hashProbe = hash % PRIM1; // probe offset
  if (!hashProbe) hashProbe = 1; // offset can't be 0

  // Probe for an existing singleton
  probeAddress = hashLocation;
  firstFreeCell = Eve;
  while (1) {	 // TODO: thinking about a probing limit
     cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
     if (cell_isFree(cell))
	    firstFreeCell = probeAddress;
     else if (cell_isPair(cell) // TODO test in a single binary word comparison
  	    && cell_getTail(cell) == tail
	    && cell_getHead(cell) == head) {
           return probeAddress; // OK: arrow found!
     }
     // Not the singleton

     if (!more(cell)) {
        break; // Probing over. It's a miss.
     }
 
     growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }
  // Miss

  if (locateOnly) // one only want to test for singleton existence in the arrows space
    return Eve; // Eve means not found

  if (firstFreeCell == Eve) {
    while (1) {
      growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
      cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
      if (cell_isFree(cell)) {
        firstFreeCell = probeAddress;
        break;
      }
    }
  }
   
  /* Create a singleton */
  newArrow = firstFreeCell;
  cell = mem_get(newArrow); ONDEBUG((show_cell(cell, 0)));
  cell = arrow_build(cell, tail, head);
  mem_set(newArrow, cell, MEM1_LOOSE); ONDEBUG((show_cell(cell, 0)));


  /* Now incremeting "more" counters in the probing path up to the new singleton
  */
  probeAddress = hashLocation;
  hashProbe = hash % PRIM1; // reset hashProbe to default
  while (probeAddress != newArrow) {
     cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
     cell = cell_more(cell);
     mem_set(probeAddress, cell, DONTTOUCH); ONDEBUG((show_cell(cell, 0)));
     growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }

  // record new arrow as loose arrow
  looseStackAdd(newArrow);
  return newArrow;
}

/** retrieve a tag arrow defined by str C string,
 * by creating the singleton if not found.
 * except if locateOnly param is set.
 */
static Arrow tagOrBlob(Cell catBits, char* str, int locateOnly) {
  uint64_t hash;
  Address hashLocation, hashProbe, hChain;
  CellData checksum;
  Address probeAddress, firstFreeCell, newArrow, current, next;
  Cell cell, nextCell, content;
  unsigned i, jump;
  char c, *p;
  if (!str) {
    return Eve;
  }

  hash = hashString(str);
  hashLocation = hash % PRIM0;
  hashProbe    = hash % PRIM1;
  if (!hashProbe) hashProbe = 1; // offset can't be 0

  checksum = hash & 0xFFFFFFFFFFLLU;

  // Search for an existing singleton
  probeAddress = hashLocation;
  firstFreeCell = Eve;
  while (1) {
    cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
    if (cell_isFree(cell))
	    firstFreeCell = probeAddress;
    else if (cell_getCatBits(cell) == catBits
  	     && cell_getChecksum(cell) == checksum) {
        // chance we found it
		// now comparing the whole string
        hChain = hashChain(probeAddress, cell) % PRIM1;
        if (!hChain) hChain = 1; // offset can't be 0
        next = jumpToFirst(cell, probeAddress, hChain, probeAddress);
        p = str;
        i = 1;
  	    cell = mem_get(next); ONDEBUG((show_cell(cell, 1)));
	    while ((c = *p++) == ((char*)&cell)[i] && c != 0) {
           i++;
           if (i == 7) {
              if (cell_getCatBits(cell) == CATBITS_LAST) {
                 break ; // the chain is over
              }
              next = jumpToNext(cell, next, hChain, probeAddress);
              cell = mem_get(next); ONDEBUG((show_cell(cell, 1)));
              i = 1;
            }
        }
        if (!c && !((char*)&cell)[i] && cell_getCatBits(cell) == CATBITS_LAST && cell_getSize(cell) == i)
            return probeAddress; // found arrow
    }
    // Not the singleton

    if (!more(cell)) {
        break; // Miss
    }
	
    growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }

  if (locateOnly)
    return Eve;

  if (firstFreeCell == Eve) {
    while (1) {
      growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
      cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
      if (cell_isFree(cell)) {
        firstFreeCell = probeAddress;
        break;
      }
    }
  }

  /* Create a missing singleton */
  newArrow = firstFreeCell;
  cell = mem_get(newArrow); ONDEBUG((show_cell(cell, 1)));
  cell = ( catBits == CATBITS_TAG
			? tag_build(cell, checksum, 0 /* jump */)
			: blob_build(cell, checksum, 0 /* jump */));

  hChain = hashChain(newArrow, cell) % PRIM1;
  if (!hChain) hChain = 1; // offset can't be 0

  Address offset = hChain;
  growingShift(newArrow, next, offset, newArrow);
  nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
  jump = 0; // Note how jump=0 means 1 shift.
  while (!cell_isFree(nextCell)) {
     jump++;
     growingShift(next, next, offset, newArrow);
	 nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
  }

  if (jump >= MAX_JUMP) {
	Address sync = next;
	Cell syncCell = nextCell;
    syncCell = sync_build(syncCell, newArrow, 0, REATTACHMENT_FIRST);
    mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));
    offset = hChain;
    growingShift(next, next, offset, sync);
	nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
    while (!cell_isFree(nextCell)) {
       growingShift(next, next, hChain, sync);
	   nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
    }
  	syncCell = sync_build(syncCell, newArrow, next, REATTACHMENT_FIRST);
	mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

	jump = MAX_JUMP;
  }
  cell = catBits == CATBITS_TAG
      ? tag_build(cell, checksum, jump)
	  : blob_build(cell, checksum, jump);
  mem_set(newArrow, cell, MEM1_LOOSE); ONDEBUG((show_cell(cell, 0)));

  current = next;
  p = str;
  i = 0;
  content = 0;
  c = 0;
  while (1) {
    c = *p++;

    ((char*)&content)[i] = c;
    if (!c) break;
    i++;
    if (i == 6) {
	  Address offset = hChain;
      growingShift(current, next, offset, current);
	  jump = 0;  // Note how jump=0 means 1 shift.
      nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
	  while (!cell_isFree(nextCell)) {
        jump++;
        growingShift(next, next, offset, current);
     	nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
      }
      if (jump >= MAX_JUMP) {
	     Address sync = next;
	     Cell syncCell = nextCell;
  	     syncCell = sync_build(syncCell, current, 0, REATTACHMENT_NEXT); // 0 = jump sync
         mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

		 offset = hChain;
         growingShift(next, next, offset, sync);
         nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
         while (!cell_isFree(nextCell)) {
           growingShift(next, next, offset, sync);
           nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
         }
  	     syncCell = sync_build(syncCell, current, next, REATTACHMENT_NEXT); // 0 = jump sync
	     mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

	     jump = MAX_JUMP;
      }
	  cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));
      cell = slice_build(cell, content, jump);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 1)));
      i = 0;
      content = 0;
	  current = next;
    }

  }
  cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));
  cell = lastSlice_build(cell, content, 1 + i /* size */);
  mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 1)));


  /* Now incremeting "more" counters in the probing path up to the new singleton
  */
  probeAddress = hashLocation;
  hashProbe = hash % PRIM1;
  while (probeAddress != newArrow) {
     cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
     cell = cell_more(cell);
     mem_set(probeAddress, cell, DONTTOUCH); ONDEBUG((show_cell(cell, 0)));
     growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }

  // log loose arrow
  looseStackAdd(newArrow);
  return newArrow;
}

/** retrieve a tag arrow defined by str C string,
 * by creating the singleton if not found.
 * except if locateOnly param is set.
 */
Arrow btag(int length, char* str, int locateOnly) {
  uint64_t hash;
  Address hashLocation, hashProbe, hChain;
  uint32_t l;
  CellData checksum;
  Address probeAddress, firstFreeCell, newArrow, current, next;
  Cell cell, nextCell, content;
  unsigned i, jump;
  char c, *p;
  if (!str) {
    return Eve;
  }

  hash = hashBString(str, length);
  hashLocation = hash % PRIM0;
  hashProbe    = hash % PRIM1;
  if (!hashProbe) hashProbe = 1; // offset can't be 0

  checksum = hash & 0xFFFFFFFFFFLLU;

  // Search for an existing singleton
  probeAddress = hashLocation;
  firstFreeCell = Eve;
  while (1) {
    cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 1)));
    if (cell_isFree(cell))
	    firstFreeCell = probeAddress;
    else if (cell_getCatBits(cell) == CATBITS_TAG
  	     && cell_getChecksum(cell) == checksum) {
        // chance we found it
		// now comparing the whole string
        hChain = hashChain(probeAddress, cell) % PRIM1;
        if (!hChain) hChain = 1; // offset can't be 0
        next = jumpToFirst(cell, probeAddress, hChain, probeAddress);
        p = str;
		l = length;
        i = 1;
  	    cell = mem_get(next); ONDEBUG((show_cell(cell, 1)));
	    if (l) { while ((c = *p++) == ((char*)&cell)[i] && --l) {
		   if (++i == 7) {
              if (cell_getCatBits(cell) == CATBITS_LAST) {
                 break ; // the chain is over
              }
              next = jumpToNext(cell, next, hChain, probeAddress);
              cell = mem_get(next); ONDEBUG((show_cell(cell, 1)));
              i = 1;
            }
        } }
        if (!l && cell_getCatBits(cell) == CATBITS_LAST && cell_getSize(cell) == i)
            return probeAddress; // found arrow
    }
    // Not the singleton

    if (!more(cell)) {
        break; // Miss
    }
     
    growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }

  if (locateOnly)
    return Eve;

  if (firstFreeCell == Eve) {
    while (1) {
      growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
      cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
      if (cell_isFree(cell)) {
        firstFreeCell = probeAddress;
        break;
      }
    }
  }
   
  /* Create a missing singleton */
  newArrow = firstFreeCell;
  cell = mem_get(newArrow); ONDEBUG((show_cell(cell, 1)));
  cell = tag_build(cell, checksum, 0 /* jump */);

  hChain = hashChain(newArrow, cell) % PRIM1;
  if (!hChain) hChain = 1; // offset can't be 0

  Address offset = hChain;
  growingShift(newArrow, next, offset, newArrow);
  nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
  jump = 0;  // Note how jump=0 means 1 shift.
  while (!cell_isFree(nextCell)) {
     jump++;
     growingShift(next, next, offset, newArrow);
	 nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
  }

  if (jump >= MAX_JUMP) {
	Address sync = next;
	Cell syncCell = nextCell;
    syncCell = sync_build(syncCell, newArrow, 0, REATTACHMENT_FIRST);
    mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));
    offset = hChain;
    growingShift(next, next, offset, sync);
	nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
    while (!cell_isFree(nextCell)) {
       growingShift(next, next, offset, sync);
	   nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
    }
  	syncCell = sync_build(syncCell, newArrow, next, REATTACHMENT_FIRST);
	mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

	jump = MAX_JUMP;
  }
  cell = tag_build(cell, checksum, jump);
  mem_set(newArrow, cell, MEM1_LOOSE); ONDEBUG((show_cell(cell, 0)));

  current = next;
  p = str;
  l = length;
  i = 0;
  content = 0;
  c = 0;
  if (l) { while (1) {
    c = *p++;
    ((char*)&content)[i] = c;
    if (!--l) break;
    if (++i == 6) {

	  Address offset = hChain;
      growingShift(current, next, offset, current);
	  jump = 0;  // Note how jump=0 means 1 shift.
      nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
	  while (!cell_isFree(nextCell)) {
        jump++;
        growingShift(next, next, offset, current);
     	nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
      }
      if (jump >= MAX_JUMP) {
	     Address sync = next;
	     Cell syncCell = nextCell;
  	     syncCell = sync_build(syncCell, current, 0, REATTACHMENT_NEXT); // 0 = jump sync
         mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

		 offset = hChain;
         growingShift(next, next, offset, sync);
         nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
         while (!cell_isFree(nextCell)) {
           growingShift(next, next, offset, sync);
           nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 1)));
         }
  	     syncCell = sync_build(syncCell, current, next, REATTACHMENT_NEXT); // 0 = jump sync
	     mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

	     jump = MAX_JUMP;
      }
	  cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));
      cell = slice_build(cell, content, jump);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 1)));
      i = 0;
      content = 0;
	  current = next;
    }

  } }
  cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));
  cell = lastSlice_build(cell, content, 1 + i /* size */);
  mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 1)));


  /* Now incremeting "more" counters in the probing path up to the new singleton
  */
  probeAddress = hashLocation;
  hashProbe = hash % PRIM1;
  while (probeAddress != newArrow) {
     cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
     cell = cell_more(cell);
     mem_set(probeAddress, cell, DONTTOUCH); ONDEBUG((show_cell(cell, 0)));
     growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }

  // log loose arrow
  looseStackAdd(newArrow);
  return newArrow;
}

/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the singleton if not found.
 * except if locateOnly param is set.
 */
static Arrow blob(uint32_t size, char* data, int locateOnly) {
  char h[20];
  crypto(size, data, h);

  // Prototype only: a BLOB is stored into a pair arrow from "blobTag" to the blog cryptographic hash. The blob data is stored separatly outside the arrows space.
  mem0_saveData(h, size, data);
  // TODO: remove the data when cell at h is recycled.

  return tagOrBlob(CATBITS_BLOB, h, locateOnly);
}

Arrow xl_arrow(Arrow tail, Arrow head) { return arrow(tail, head, 0); }
Arrow xl_tag(char* s) { return tagOrBlob(CATBITS_TAG, s, 0);}
Arrow xl_btag(uint32_t size, char* s) { return btag(size, s, 0);}
Arrow xl_blob(uint32_t size, char* data) { return blob(size, data, 0);}

Arrow xl_arrowMaybe(Arrow tail, Arrow head) { return arrow(tail, head, 1); }
Arrow xl_tagMaybe(char* s) { return tagOrBlob(CATBITS_TAG, s, 1);}
Arrow xl_btagMaybe(uint32_t size, char* s) { return btag(size, s, 1);}
Arrow xl_blobMaybe(uint32_t size, char* data) { return blob(size, data, 1);}

Arrow xl_headOf(Arrow a) {
  if (a == Eve)
     return Eve;
  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  assert(cell_isArrow(cell));
  if (!cell_isArrow(cell))
     return -1; // Invalid id

  if (cell_isTuple(cell))
     return a; //TODO: Tuple

  return (Arrow)cell_getHead(cell);
}

Arrow xl_tailOf(Arrow a) {
  if (a == Eve)
     return Eve;
  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  assert(cell_isArrow(cell));
  if (!cell_isArrow(cell))
     return -1; // Invalid id

  Cell catBits = cell_getCatBits(cell);

  if (catBits != CATBITS_ARROW)
     return a; // TODO: Tuple

  return (Arrow)cell_getTail(cell);
}

static char* tagOrBlobOf(Cell catBits, Arrow a, uint32_t* lengthP) {
  Address current = a;
  if (a == Eve) {
     lengthP = 0;
     return (char *)0; // Invalid Id
  }
  
  Cell cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));
  Cell _catBits = cell_getCatBits(cell);
  if (_catBits != catBits) {
     lengthP = 0;
     return (char *)0; // Invalid Id
  }

  uint32_t hChain = hashChain(a, cell) % PRIM1;
  if (!hChain) hChain = 1; // offset can't be 0

  current = jumpToFirst(cell, current, hChain, a);
  cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));

  unsigned i = 1;
  char* tag =  NULL;
  uint32_t size = 0;
  uint32_t max = 0;
  geoalloc((char**)&tag, &max, &size, sizeof(char), 1);
  //TODO:QUICKER COPY
  while ((tag[size-1] = ((char*)&cell)[i])) {
    geoalloc((char**)&tag, &max, &size, sizeof(char), size + 1);
    i++;
    if (i == 7) {
      i = 1;
      if (cell_getCatBits(cell) == CATBITS_LAST) {
	    size -= (6 - cell_getSize(cell));
        break;// the chain is over
      }
      current = jumpToNext(cell, current, hChain, a);
      cell = mem_get(current); ONDEBUG((show_cell(cell, 1)));
    }
  }
  *lengthP = size;
  return tag;
}

char* xl_btagOf(Arrow a, uint32_t* lengthP) {
  return tagOrBlobOf(CATBITS_TAG, a, lengthP);
}

char* xl_tagOf(Arrow a) {
  uint32_t lengthP;
  return tagOrBlobOf(CATBITS_TAG, a, &lengthP);
}

char* xl_blobOf(Arrow a, uint32_t* lengthP) {
  uint32_t hl;
  char* h = tagOrBlobOf(CATBITS_BLOB, a, &hl);
  if (!h) {
     *lengthP = 0;
	 return (char *)0;
  }
  char* data = mem0_loadData(h, lengthP);
  free(h);
  return data;
}


int xl_isEve(Arrow a) {
  return (a == Eve);
}


enum e_xlType xl_typeOf(Arrow a) {
  if (a == Eve) return XL_EVE;

  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  if (cell_isFree(cell))
      return XL_UNDEF;

  static enum e_xlType map[] = { XL_ARROW, XL_TUPLE, XL_SMALL, XL_TAG, XL_BLOB, XL_UNDEF, XL_UNDEF, XL_UNDEF};

  Cell catBits = cell_getCatBits(cell);
  int catI = catBits >> 56;
  return map[catI];
}


/** connect a child arrow to its parent arrow "a"
 * * one adds the child back-ref to the children list of "a"
 * * if the parent arrow was loose (eg. disconnected & unrooted), one connects the parent arrow to its head and tail (recursive calls) and one removes the LOOSE mem1 flag attached to 'a' cell
 *
 * TODO : when adding a new cell, try to reduce jumpers from time to time
 */
static void connect(Arrow a, Arrow child) {
  ONDEBUG((fprintf(stderr, "connect child=%06x to a=%06x\n", child, a)));
  if (a == Eve) return; // One doesn't store Eve connectivity. 18/8/11 Why not?
  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  assert(cell_isArrow(cell));
  Cell catBits = cell_getCatBits(cell);

  if (mem_getAdmin(a) == MEM1_LOOSE) {
      // One removes the mem1 LOOSE flag attached to 'a' cell.
      mem_setAdmin(a, 0);
      // (try to) remove 'a' from the loose log
      looseStackRemove(a);
      // One recursively connects the parent arrow to its ancestors.
	  if (catBits == CATBITS_ARROW) {
		connect(cell_getTail(cell), a);
		connect(cell_getHead(cell), a);
	  } else if (catBits == CATBITS_TUPLE) {
	    // TODO connect every end
	  }
  }
  CellData data = (catBits == CATBITS_ARROW || catBits == CATBITS_SMALL)
      ? cell_getSmall(cell)
	  : cell_getChecksum(cell);

  uint32_t hChild = hashChild(a, data) % PRIM1;
  if (!hChild) hChild = 2; // offset can't be 0

  Address current = jumpToChild0(cell, a, hChild, a);
  if (current) {
	cell = mem_get(current); ONDEBUG((show_cell(cell, 0)));
	while ((cell & 0xFFFFFF00000000LLU) &&
        	(cell & 0x000000FFFFFF00LLU) &&
		   cell_getCatBits(cell) != CATBITS_LAST) {
		if (cell_getJumpNext(cell) ==  MAX_JUMP) {
		   // jump == MAX ==> "deep link": ref1 points to next
		   current = cell_getRef1(cell);
		} else {
		   current = jumpToNext(cell, current, hChild, a);
		}
		cell = mem_get(current); ONDEBUG((show_cell(cell, 0)));
    }
	if (!(cell & 0xFFFFFF00000000LLU)) {
      // first ref bucket is empty
      cell |= ((Cell)child << 32);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
      ONDEBUG((fprintf(stderr, "Connection using first ref bucket of %06x\n", current)));
	  return;
    } else if (!((Cell)cell & 0xFFFFFF00LLU)) {
      // second ref bucket is empty
	  cell |= ((Cell)child << 8);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
      ONDEBUG((fprintf(stderr, "Connection using second ref bucket of %06x\n", current)));
	  return;
    }
  } else {
    current = a;
  }

  // if no free cell in list
  // free cell search and jump computing
  Address next;
  Cell nextCell;
  Address offset = hChild;
  unsigned jump = 1;
  growingShift(current, next, offset, current);
  nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 0)));
  while (!cell_isFree(nextCell)) {
      jump++;
      growingShift(next, next, offset, current);
      nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 0)));
  }

  if (current == a) { // No child yet
     ONDEBUG((fprintf(stderr, "new child0: %06x ", next)));
     if (jump >= MAX_CHILD0) { // ohhh the pain
        ONDEBUG((fprintf(stderr, "MAX CHILD0!\n")));
		Address sync = next;
		Cell syncCell = nextCell;
		syncCell = sync_build(syncCell, a, 0, REATTACHMENT_CHILD0);
		mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

		offset = hChild;
        growingShift(next, next, offset, current);
		nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 0)));
		while (!cell_isFree(nextCell)) {
			growingShift(next, next, offset, current);
			nextCell = mem_get(next); ONDEBUG((show_cell(nextCell, 0)));
		}
		syncCell = sync_build(syncCell, a, next, REATTACHMENT_CHILD0);
		mem_set(sync, syncCell, 0); ONDEBUG((show_cell(syncCell, 0)));

		nextCell = lastRefs_build(nextCell, child, 0, 0);

		jump = MAX_CHILD0;
     } else { // Easy one
        ONDEBUG((fprintf(stderr, "child0=%d\n", jump)));
        nextCell = lastRefs_build(nextCell, child, 0, 0);
     }
     mem_set(next, nextCell, 0); ONDEBUG((show_cell(nextCell, 0)));
     cell = cell_chainChild0(cell, jump);
     mem_set(a, cell, 0); ONDEBUG((show_cell(cell, 0)));
  } else {
    ONDEBUG((fprintf(stderr, "new chain: %06x ", next)));
    if (jump >= MAX_JUMP) {
      ONDEBUG((fprintf(stderr, " MAX JUMP!\n")));
	  nextCell = lastRefs_build(nextCell, cell_getRef1(cell), child, 0);
	  mem_set(next, nextCell, 0); ONDEBUG((show_cell(nextCell, 0)));

	  // When jump = MAX_JUMP in a children cell, ref1 actually points to the next chained cell
	  cell = refs_build(cell, cell_getRef0(cell), next, MAX_JUMP);
	  mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
    } else {
      ONDEBUG((fprintf(stderr, " jump=%d\n", jump)));
      // One add a new cell (a "last" chained cell)
      nextCell = lastRefs_build(nextCell, child, 0, 0);
      mem_set(next, nextCell, 0); ONDEBUG((show_cell(nextCell, 0)));
	  // One chain the previous last cell with this last celll
      cell = cell_chain(cell, jump - 1);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
    } 
  }
}


/** disconnect a child arrow from its parent arrow.
 * One removes the child back-ref from the "child0 jump list" of its parent.
 * If the parent arrow gets unreferred (no child and unrooted), it is disconnected in turn from its head and tail (recursive calls) and it is recorded as a loose arrow
 */
static void disconnect(Arrow a, Arrow child) {
    ONDEBUG((fprintf(stderr, "disconnect child=%06x from a=%06x\n", child, a)));
    if (a == Eve) return; // One doesn't store Eve connectivity.
    Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
    assert(cell_isArrow(cell));
    Cell catBits = cell_getCatBits(cell);

    // compute hashChild
	CellData data = (catBits == CATBITS_ARROW || catBits == CATBITS_SMALL)
      ? cell_getSmall(cell)
	  : cell_getChecksum(cell);

	uint32_t hChild = hashChild(a, data) % PRIM1;
    if (!hChild) hChild = 2; // offset can't be 0

    // find the arrow in its parent child chain
    Address current = jumpToChild0(cell, a, hChild, a);
	assert(current);
	Address previous = 0;
	Cell childToRef0 = (Cell)child << 32;
	Cell childToRef1 = (Cell)child << 8;
	cell = mem_get(current); ONDEBUG((show_cell(cell, 0)));
    while ((cell & 0xFFFFFF00000000LLU) != childToRef0 &&
           (cell &       0xFFFFFF00LLU) != childToRef1 &&
		   cell_getCatBits(cell) != CATBITS_LAST) {

		previous = current;
		if (cell_getJumpNext(cell) ==  MAX_JUMP) {
		   // jump == MAX ==> "deep link": ref1 points to next
		   current = cell_getRef1(cell);
		} else {
		   current = jumpToNext(cell, current, hChild, a);
		}
		cell = mem_get(current); ONDEBUG((show_cell(cell, 0)));
    }
    // erase back-ref
    if (childToRef0 == (cell & 0xFFFFFF00000000LLU)) {
      // empty out first ref bucket
      cell &= 0xFF000000FFFFFFFFLLU;
    } else {
  	  // empty out second ref
	  assert(childToRef1 == (cell & 0xFFFFFF00LLU));
      cell &= 0xFFFFFFFF000000FFLLU;
    }

    if (cell_getCatBits(cell) == CATBITS_CHAIN && cell_getJumpNext(cell) == MAX_JUMP) { // "deep link" case
      if (cell & 0xFFFFFF00000000LLU)
        // still a back-ref in this "deep link" child chain cell
        return;
    } else if (cell & 0xFFFFFFFFFFFF00LLU) {
	   // still an other back-ref in this child chain cell
       mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
	   // nothing more to do
	   return;
    }

	// The modified cell contains no more back-reference
	// One removes the cell from the children list
    if (cell_getCatBits(cell) == CATBITS_LAST) {
      // this is the last cell

      // freeing the whole cell
	  cell = cell_free(cell);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
	  
      // FIXME detect a chain of deep links up to this last cell
	  if (previous) {
	      // Unchain previous cell (make it the last chained cell)
	      cell = mem_get(previous); ONDEBUG((show_cell(cell, 0)));
          cell = cell_unchain(cell);
          mem_set(previous, cell, 0); ONDEBUG((show_cell(cell, 0)));
      } else {
	      // this is the first and last cell of its chain!
          // so we've removed the last child from the list. The list is empty.
          cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
		  cell = cell_chainChild0(cell, 0);
          // FIXME: if cell.jump == MAX_CHILD0 there is a child0 reattachment cell to delete!
	      if (cell_isRooted(cell)) {
	         // the cell is still rooted
			 mem_set(a, cell, 0); ONDEBUG((show_cell(cell, 0)));
		  } else {
  		     // The parent arrow is unreferred
		     // Let's switch on its LOOSE status.
			 mem_set(a, cell, MEM1_LOOSE); ONDEBUG((show_cell(cell, 0)));
             // And add it to the loose log
             looseStackAdd(a);
 	         // One disconnects the arrow from its parents
			 // 2 recursive calls
			 if (cell_isPair(cell)) {
               disconnect(cell_getTail(cell), a);
               disconnect(cell_getHead(cell), a);
		     } // TODO tuple
		  }
      }
    } else { // That's not the last cell

	  // Find the next cell
      unsigned jump = cell_getJumpNext(cell);
      Address next;
      if (jump == MAX_JUMP)
         next = cell_getRef1(cell);
      else         
	     next = jumpToNext(cell, current, hChild, a);

	  // Free the whole cell
	  cell = cell_free(cell);
      mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));

      if (previous) {
	      // Update previous chain jumper
	      cell = mem_get(previous); ONDEBUG((show_cell(cell, 0)));
          unsigned previousJump = cell_getJumpNext(cell);
		  if (previousJump == MAX_JUMP) {
		     // There was already a deep link, one simply updates it
		     cell = refs_build(cell, cell_getRef0(cell), next, MAX_JUMP);
             mem_set(previous, cell, 0); ONDEBUG((show_cell(cell, 0)));
		  } else {
		     // one adds the jump amount of the removed cell to the previous cell jumper
			 // note: don't forget to jump over the removed cell itself (+1)
	         previousJump += 1 + jump; // note "jump" might be at max here
             if (previousJump < MAX_JUMP) {
			    // an easy one at last
                cell = cell_chain(cell, previousJump);
                mem_set(previous, cell, 0); ONDEBUG((show_cell(cell, 0)));
		     } else {
			    // It starts to be tricky, one has to put a deep link into the previous cell.
				// As one overwrites a child back-reference, one connects this child again. :(
		        Arrow child1 = cell_getRef1(cell);
                cell = refs_build(cell, cell_getRef0(cell), next, MAX_JUMP);
                mem_set(previous, cell, 0); ONDEBUG((show_cell(cell, 0)));
			    connect(a, child1); // Q:may it connect 'a' to its parents twice? R: no. It's ok.
		     }
		  }
      } else {
	      // This is the first cell of the children cells chain
	      // one adds the jump amount to a.child0
          cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
		  unsigned child0 = cell_getChild0(cell) + jump + 1; // don't forget + 1 to jump over freed cell
		  if (child0 < MAX_CHILD0) { // an easy one: one updates child0
            cell = cell_chainChild0(cell, child0);
            mem_set(a, cell, 0); ONDEBUG((show_cell(cell, 0)));
		  } else {
            // One reuses the freed cell and convert it into a "deep link" cell
            cell = refs_build(cell, 0, next, MAX_JUMP);
		    mem_set(current, cell, 0); ONDEBUG((show_cell(cell, 0)));
          }
	  }
    }
}


void xl_childrenOfCB(Arrow a, XLCallBack cb, Arrow context) {
  ONDEBUG((fprintf(stderr, "xl_childrenOf a=%06x\n", a)));

  if (a == Eve) {
     return; // Eve connectivity not traced
  }

  Arrow child;
  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  if (!cell_isArrow(cell)) return; // invalid ID
  // TODO one might call cb with a

  Cell catBits = cell_getCatBits(cell);

  // compute hashChild
  CellData data = (catBits == CATBITS_ARROW || catBits == CATBITS_SMALL)
      ? cell_getSmall(cell)
	  : cell_getChecksum(cell);

  uint32_t hChild = hashChild(a, data) % PRIM1;
  if (!hChild) hChild = 2; // offset can't be 0

  Address current = jumpToChild0(cell, a, hChild, a);
  if (!current) return; // no child
  cell = mem_get(current); ONDEBUG((show_cell(cell, 0)));
   // For every cell but last
  while (cell_getCatBits(cell) != CATBITS_LAST) {
    child = cell_getRef0(cell);
	if (child && cb(child, context))
	   return;

	if (cell_getJumpNext(cell) ==  MAX_JUMP) {
		// jump == MAX ==> ref1 points to next
        current = cell_getRef1(cell);
	} else {
	    child = cell_getRef1(cell);
	    if (child && cb(child, context))
  	        return;
   		current = jumpToNext(cell, current, hChild, a);
	}
	cell = mem_get(current); ONDEBUG((show_cell(cell, 0)));
  }
  // Last cell
  child = cell_getRef0(cell);
  if (child && cb(child, context)) return;
  child = cell_getRef1(cell);
  if (child && cb(child, context)) return;
}

typedef struct iterator_s {
    int type;
    uint32_t hash;
    Arrow parent;
    Arrow current;
    Address pos;
    int rank;
} iterator_t;

static int xl_enumNextChildOf(XLEnum e) {
  iterator_t *iteratorp = e;
  
  // iterate from current location stored in childrenOf iterator
  Arrow    a = iteratorp->parent;
  uint32_t hChild = iteratorp->hash;
  Address pos = iteratorp->pos;
  int     rank = iteratorp->rank;
  
  Cell cell;
  Arrow child;
  
  if (pos == Eve) { // First call to "next" for this enumeration
    cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
    pos = jumpToChild0(cell, a, hChild, a);
    if (!pos) {
       return 0; // no child
    }
  } else {
    cell = mem_get(pos); ONDEBUG((show_cell(cell, 0)));
    if (!rank) {
  	  if (cell_getCatBits(cell) != CATBITS_LAST && cell_getJumpNext(cell) ==  MAX_JUMP) {
		// jump == MAX ==> ref1 points to next
        pos = cell_getRef1(cell);
      } else {
        Arrow child = cell_getRef1(cell);
	    if (child) {
          iteratorp->rank = 1;
          iteratorp->current = child;
	      return !0;
        }
        if (cell_getCatBits(cell) == CATBITS_LAST) {
          return 0; // No more child
        }
   		pos = jumpToNext(cell, pos, hChild, a);
      }
    } else {
      if (cell_getCatBits(cell) == CATBITS_LAST) {
        return 0; // No more child
      }
      pos = jumpToNext(cell, pos, hChild, a);
	  rank = 0;
    }
  } 
  cell = mem_get(pos); ONDEBUG((show_cell(cell, 0)));
  
   // For every cell but last
  while (cell_getCatBits(cell) != CATBITS_LAST) {
    child = cell_getRef0(cell);
	if (child) {
         iteratorp->pos = pos;
         iteratorp->rank = 0;
         iteratorp->current = child;
	     return !0;
    }
	if (cell_getJumpNext(cell) ==  MAX_JUMP) {
		// jump == MAX ==> ref1 points to next
        pos = cell_getRef1(cell);
	} else {
	    child = cell_getRef1(cell);
	    if (child) {
           iteratorp->pos = pos;
           iteratorp->rank = 1;
           iteratorp->current = child;
	       return !0;
        }   
   		pos = jumpToNext(cell, pos, hChild, a);
	}
	cell = mem_get(pos); ONDEBUG((show_cell(cell, 0)));
  }

  // Last cell
  iteratorp->pos = pos;
  child = cell_getRef0(cell);

  if (child) {
      iteratorp->rank = 0;
      iteratorp->current = child;
	  return !0;
  }

  iteratorp->rank = 1;
  child = cell_getRef1(cell);
  if (child) {
      iteratorp->current = child;
	  return !0;
  } else {
      iteratorp->current = Eve;
      return 0; // last iteration
  }
}
  
int xl_enumNext(XLEnum e) {
  assert(e);
  iterator_t *iteratorp = e;
  assert(iteratorp->type == 0);
  if (iteratorp->type == 0) {
    return xl_enumNextChildOf(e);
  }
}

Arrow xl_enumGet(XLEnum e) {
  assert(e);
  iterator_t *iteratorp = e;
  assert(iteratorp->type == 0);
  if (iteratorp->type == 0) {
    return iteratorp->current;
  }
}

void xl_freeEnum(XLEnum e) {
   free(e);
}

XLEnum xl_childrenOf(Arrow a) {
  ONDEBUG((fprintf(stderr, "xl_childrenOf a=%06x\n", a)));

  if (a == Eve) {
     return NULL; // Eve connectivity not traced
  }

  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  if (!cell_isArrow(cell)) return NULL; // invalid ID

  Cell catBits = cell_getCatBits(cell);

  // compute hashChild
  CellData data = (catBits == CATBITS_ARROW || catBits == CATBITS_SMALL)
      ? cell_getSmall(cell)
	  : cell_getChecksum(cell);

  uint32_t hChild = hashChild(a, data) % PRIM1;
  if (!hChild) hChild = 2; // offset can't be 0

  iterator_t *iteratorp = (iterator_t *)malloc(sizeof(iterator_t));
  assert(iteratorp);
  iteratorp->type = 0; // iterator type = childrenOf
  iteratorp->hash = hChild;  // hChild
  iteratorp->parent = a;   // parent arrow
  iteratorp->current = Eve; // current child
  iteratorp->pos = Eve; // current cell holding child back-ref
  iteratorp->rank = 0; // position of child back-ref in cell : 0/1
  return iteratorp;
}

/** root an arrow */
Arrow xl_root(Arrow a) {
  if (a == Eve) {
     return Eve; // no
  }

  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  if (!cell_isArrow(cell) || cell_isRooted(cell)) return Eve;

  int loose = (mem_getAdmin(a) == MEM1_LOOSE); // checked before set to zero hereafter

  // change the arrow to ROOTED state + remove the Mem1 LOOSE state is any
  mem_set(a, cell_setRootBit(cell, ROOTBIT_ROOTED), 0); ONDEBUG((show_cell(cell_setRootBit(cell, ROOTBIT_ROOTED), 0)));

  if (loose) { // if the arrow has just lost its LOOSE state, one connects it to its parents
    if (cell_isPair(cell)) {
       connect(cell_getTail(cell), a);
       connect(cell_getHead(cell), a);
	}
	// TODO tuple case
    // (try to) remove 'a' from the loose log
    looseStackRemove(a);
  }
  return a;
}

/** unroot a rooted arrow */
Arrow xl_unroot(Arrow a) {
  if (a == Eve)
      return Eve; // stop dreaming
      
  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  if (!cell_isArrow(cell))
      return Eve;
  if (!cell_isRooted(cell))
      return a;
 
  // change the arrow to UNROOTED state
  cell = cell_setRootBit(cell, ROOTBIT_UNROOTED);

  // If this arrow has no child, it's in loose state
  int loose = (cell_getChild0(cell) ? 0 : !0);



  mem_set(a, cell, loose ? MEM1_LOOSE : 0); ONDEBUG((show_cell(cell, 0)));

  if (loose) {
     // If loose, one disconnects the arrow from its parents.
    if (cell_isPair(cell)) {
       disconnect(cell_getTail(cell), a);
       disconnect(cell_getHead(cell), a);
	} // TODO Tuple case
	// loose log
    looseStackAdd(a);
  }
  return a;
}

/** return the root status */
int xl_isRooted(Arrow a) {
  if (a == Eve) return Eve;

  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  assert(cell_isArrow(cell));
  if (!cell_isArrow(cell)) return -1;
  return (cell_isRooted(cell) ? a : Eve);
}

/** return the loose status */
int xl_isLoose(Arrow a) {
  if (a == Eve) return 0;
  
  if (mem_getAdmin(a) == MEM1_LOOSE) return 1;

  // Real loose test, herefater (dead code)

  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  if (!cell_isArrow(cell)) return 0; // mmm might it happen?

  if (cell_isRooted(cell)) {
    return 0; // rooted ==> not loose!
  }

  if (cell_getChild0(cell)) {
    return 0;
  }

  return 1; // Loose for sure.
}

int xl_equal(Arrow a, Arrow b) {
  return (a == b);
}

static void forget(Arrow a) {
  ONDEBUG((fprintf(stderr, "forget a=%06x\n", a)));

  Cell cell = mem_get(a); ONDEBUG((show_cell(cell, 0)));
  Cell catBits = cell_getCatBits(cell);
  // Compute hashs
  uint64_t hash;
  Address hashLocation; // base address
  Address hashProbe; // probe offset

  if (catBits == CATBITS_TAG || catBits == CATBITS_BLOB || catBits == CATBITS_TUPLE) {
    // Compute content hash
	if (catBits == CATBITS_TAG || catBits == CATBITS_BLOB) {
	   uint32_t strl;
	   char* str = tagOrBlobOf(catBits, a, &strl);
       hash = hashString(str);
       free(str);
	} else {
	   // TODO Tuple
	}

    // Free chain
    // FIXME this code doesn't erase reattachment cells :(
  	Cell hChain = hashChain(a, cell) % PRIM1;
    if (!hChain) hChain = 1; // offset can't be 0
    Address next = jumpToFirst(cell, a, hChain, a);
    cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
	while (cell_getCatBits(cell) == CATBITS_CHAIN) {
        mem_set(next, cell_free(cell), 0); ONDEBUG((show_cell(cell, 0)));
        next = jumpToNext(cell, next, hChain, a);
        cell = mem_get(next); ONDEBUG((show_cell(cell, 0)));
    }
    assert(cell_getCatBits(cell) == CATBITS_LAST);
    mem_set(next, cell_free(cell), 0); ONDEBUG((show_cell(cell, 0)));
    

  } else {
    // Compute content hash
    if (catBits == CATBITS_ARROW) {
       // Compute hash
       hash = hashArrow(cell_getTail(cell), cell_getHead(cell));
    } else {
		assert(catBits == CATBITS_SMALL);
	    // TODO
	}
  }

  // Free definition start cell
  mem_set(a, cell_free(cell), 0); ONDEBUG((show_cell(cell_free(cell), 0)));

    
  hashLocation = hash % PRIM0; // base address
  hashProbe = hash % PRIM1; // probe offset
  if (!hashProbe) hashProbe = 1; // offset can't be 0

  /* Now decremeting "more" counters in the probing path up to the forgotten singleton
  */
  Cell probeAddress = hashLocation;
  while (probeAddress != a) {
     cell = mem_get(probeAddress); ONDEBUG((show_cell(cell, 0)));
     cell = cell_unmore(cell);
     mem_set(probeAddress, cell, DONTTOUCH); ONDEBUG((show_cell(cell, 0)));
     growingShift(probeAddress, probeAddress, hashProbe, hashLocation);
  }
}

void xl_commit() {
  ONDEBUG((fprintf(stderr, "xl_commit (looseStackSize = %d)\n", looseStackSize)));
  unsigned i;
  for (i = 0; i < looseStackSize ; i++) { // loose stack scanning
    Arrow a = looseStack[i];
    if (xl_isLoose(a)) { // a loose arrow is removed NOW
      forget(a);
    }
  }
  zeroalloc((char**)&looseStack, &looseStackMax, &looseStackSize);
  mem_commit();
}

/** initialize the Entrelacs system */
int space_init() {
  int rc = mem_init();
  if (rc) { // very first start
    // Eve
	Cell cellEve = arrow_build(0, Eve, Eve);
	cellEve = cell_setRootBit(cellEve, ROOTBIT_ROOTED);
    mem_set(Eve, cellEve, 0); ONDEBUG((show_cell(cellEve, 0)));
    mem_commit();
  }

  geoalloc((char**)&looseStack, &looseStackMax, &looseStackSize, sizeof(Address), 0);
  return rc;
}

// GLOBAL TODO: SMALL and TUPLE
