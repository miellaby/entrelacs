#include <stdio.h>
#include <stdlib.h>
#include "_entrelacs.h"
#include "mem0.h"
#include "space.h"

typedef unsigned char uchar;

static const Arrow Eve = 0;

#define ROOTED 0xF
#define ARROW 0xE
#define TAG 0xD
// #define BLOB 0xC
#define STUFFING 0xB
#define MAX_JUMP 0xA
#define MAX_CROSSOVER 0xF

#define SUCCESS 0

// Cell = XJD w/ X = Crossover (4bits), J = Jump (4bits), D = Data (24bits)

#define cell_getCrossover(C) ((C) >> 28)
#define cell_getJump(C) (((C) >> 24) && 0xF)
#define cell_getData(C) ((C) & 0x00FFFFFF)

#define cell_isZero(C) ((C) == 0)
#define cell_isStuffed(C) (((C) & 0x0F000000) == 0x0B000000)
#define cell_isFree(C) (cell_isZero(C) || cell_isStuffed(C))
#define cell_isArrow(C) ((C) & 0x0F000000) > 0x0E000000)
#define cell_isTag(C) ((C) & 0x0F000000) == 0x0D000000)
// #define cell_isBlob(C) ((C) & 0x0F000000) == 0x0C000000)
#define cell_isFollower(C) ((C) & 0x0F000000) < 0x0C000000)
#define cell_isRooted(C) ((C) & 0x0F000000) == 0x0F000000)


#define cell_build(X, J, D) (((X) << 28) | ((J) << 24) | D)
#define cell_free(C) (C = 0)
#define cell_tooManyCrossOver(C) ((C) & 0xF0000000 == 0xF0000000)
#define cell_stuff(C) (cell_isZero(C) ? 0x1B000000 : (cell_tooManyCrossOver(C) ? 0xFB000000 :(C + 0x10000000))


uint32 hash(Arrow tail, Arrow head) {
  return (tail << 20) ^ (tail >> 4) ^ head;
}

#define hash1(tail, head) (hash(tail, head) % PRIM0)
#define hash2(tail, head) (hash(tail, head) % PRIM1)

uint32 hashChain(address, cell) { // Data chain hash offset
  // Take an address, take the targeted cell content, mix
  return (cell << 20) ^ (cell >> 4) ^ address;
}

uint32 hashString(uchar *s) { // simple string hash
  unsigned long hash = 5381;
  int c;

  while (c = *str++)
    // 5 bits shift only because ASCII generally works within 5 bits
    hash = ((hash << 5) + hash) + c;

  return hash;
}

char sha_buff[256];
char* sha() { /* ... */ return sha_buf; }


#define shift(H,N,O,S) (N = ((H + O) % SPACE_SIZE), assert(N != S))
#define jump(C,H,N,O,S) (N = ((H + O * cell_getJump(C)) % SPACE_SIZE), assert(N != S))

Arrow Eve() { return Eve; }

Arrow _arrow(Arrow tail, Arrow head, int locateOnly) {
  uint32 h, h1, h2, h3;
  uint32 a, next, stuff;
  uint32 cell1, cell2;

  // Eve special case
  if (tail == Eve && head == Eve) {
    return Eve;
  }

  // Compute hashs
  h = hash(tail, head);
  h1 = h % PRIM1; // base address
  h2 = h % PRIM2; // probe offset

  // Search an existing copy of this arrow
  stuff = Eve;
  a = h1;
  cell1 = space_get(a);
  while (!cell_isZero(cell1)) {
    if (cell_isStuffed(cell1)) {
      if (stuff == Eve) 
        stuff = a; // one remembers first stuffed cell while scannning
    } else if (cell_isArrow(cell1) && cell_getData(cell1) == tail) {
      // good start
      // chain offset
      h3 = hashChain(a, tail) % PRIM2; 
      shift(a, next, h3, a);
      cell2 = space_get(next);
      if (cell_getData(cell2) == head)
        return a; // OK: arrow found!
    }
    shift(a, a, h2, h1);
    cell1 = space_get(a);
  }

  if (locateOnly)
    return Eve;

  // miss, so let's put the arrow into the space
  if (stuff) a = stuff;

  // but we need 2 consecutive free cells in a chain!

  // h3 is the chain offset
  // it depends on the actual arrow address
  h3 = hashChain(a, tail) % PRIM2;
  shift(a, next, h3, next);
  cell2 = space_get(next);

  while (!cell_isFree(cell2)) {
    // one stuffs unusable empty cell
    space_set(a, cell_stuff(cell1));
    shift(a, a, h2, h1);
    cell1 = space_get(a);
    while (!cell_isFree(cell1)) {
      shift(a, a, h2, h1);
    }
    h3 = hashChain(a, tail) % PRIM2; 
    shift(a, next, h3, next);
    cell2 = space_get(next);
  }

  // (cell_isFree(cell1) && cell_isFree(cell2))
  cell1 = cell_build(cell_getCrossover(cell1), ARROW, tail);
  cell2 = cell_build(cell_getCrossover(cell2), 0, head);
  space_set(a, cell1);
  space_set(next, cell2);

  return a; // we did it;
}


Arrow _tag(uchar* str, int locateOnly) {
  uint32 h, h1, h2, h3;
  uint32 a, next, stuff;
  uint32 cell1, cell2, content;
  unsigned i;
  uchar c, *p;
  if (!str) {
    return Eve;
  }

  h = hashString(str);
  h1 = h % PRIM1;
  h2 = h % PRIM2;

  a = h1;
  stuff = Eve;
  cell = space_get(a);
  while (!cell_isZero(cell)) {
    if (cell_isStuffed(cell1)) {
      if (stuff == Eve) 
        stuff = a; // one remembers first stuffed cell while scannning
    } else if (cell_isString(cell) && cell_getData(cell) == h1) {
      // good start
      // now one must compare the whole string, char by char
      h3 = hashChain(a, h1) % PRIM2; 
      p = str;
      shift(a, next, h3, a);
      cell = space_get(next);
      i = 1;
      while ((c = *p++) == ((uchar*)&cell)[i] && c != 0) {
        i++;
        if (i == 4) {
          i = 1;
          if (cell_getJump(cell) == 0) {
            //i = 3;// useless
            break;// the chain is over
          }
          jump(cell, next, next, h3, a);
          cell = space_get(next);
        }
      }
      if (!c && !((uchar*)&cell)[i])
        return a; // found arrow
      
      shift(a, a, h2, h1);
      cell = space_get(a);
    }

  if (locateOnly)
    return Eve;

  // miss, so let's put the arrow into the space.
  // but we need 2 consecutive free cells in a chain
  if (stuff) a = stuff;

  // h3 is the chain offset, h3 is not h1
  h3 = hashChain(a, h1) % PRIM2; 
  shift(a, next, h3, next);
  cell2 = space_get(next);

  while (!cell_isFree(cell2)) {
    // one stuffs unusable empty cell
    space_set(a, cell_stuff(cell1));
    shift(a, a, h2, h1);
    cell1 = space_get(a);
    while (!cell_isFree(cell1)) {
      shift(a, a, h2, h1);
    }
    h3 = hashChain(a, h1) % PRIM2; 
    shift(a, next, h3, next);
    cell2 = space_get(next);
  }
  // 2 cells in a chain row found.

  // First H2, then all chars
  cell1 = cell_build(cell_getCrossover(cell1), STRING, h2);
  space_set(a, cell1);
  current = next;
  cell1 = cell2;
  // h3 already set
  p = str;
  i = 0;
  content = 0;
  for (;;) {
    c = *p++;
    i++;
    if (i == 4) {
      unsigned jump = 1;
      shift(current, next, h3, a);
      cell2 = space_get(next);
      while (!cell_isFree(cell2)) {
        jump++;
        shift(next, next, h3, a);
        cell2 = space_get(next);
      }
      cell1 = cell_build(cell_getCrossover(cell1), jump && 15, content);
      space_set(current, cell1);
      current = next;
      cell1 = cell2;
      i = 1;
      content = 0;
    }
    ((uchar*)&content)[i] = c; 

    if (!c) break;
  }
  cell1 = cell_build(cell_getCrossover(cell1), 0, content);
  space_set(current, cell1);
  return a;
}


Arrow _blob(uint32 size, uchar* data, int locateOnly) {
  return pair(tag("#BLOB"), tag(sha(size, data)));
}

Arrow arrow(tail, head) { return _arrow(tail, head, 0); }
Arrow locateArrow(tail, head) { return _arrow(tail, head, 1); }

Arrow tag(uchar* s) { return _tag(s, 0);}
Arrow locateTag(uchar* s) { return _tag(s, 1};}

Arrow blob(uint32 size, uchar* data) { return _blob(size, data, 0);}
Arrow locateBlob(uint32 size, uchar* data) { return _blob(size, data, 1);}

Arrow root(Arrow a) {  
  uint32 cell = space_get(a);
  if (cell_isArrow(cell) && !cell_isRooted(cell))
    space_set(a, cell_build(cell_getCrossover(cell), ROOTED, cell_getContent(cell));
}

void unroot(Arrow) {
  uint32 cell = space_get(a);
  if (cell_isRooted(cell))
    space_set(a, cell_build(cell_getCrossover(cell), ARROW, cell_getContent(cell));
}

void init() {
  if (space_init()) { // very first start 
    // Eve
    space_set(0, ARROW, 0);
    space_set(1, 0, 0);
    space_commit();
  }
}
