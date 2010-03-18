#include <stdio.h>
#include <stdlib.h>
#include "_entrelacs.h"
#include "mem0.h"
#include "space.h"

static const Arrow Eve = 0;

#define ARROW 0xF
#define BLOB 0xE
#define TAG 0xD
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
#define cell_isArrow(C) ((C) & 0x0F000000) == 0x0F000000)
#define cell_isTag(C) ((C) & 0x0F000000) == 0x0E000000)
#define cell_isBlob(C) ((C) & 0x0F000000) == 0x0D000000)
#define cell_isFollower(C) ((C) & 0x0F000000) < 0x0D000000)

#define cell_build(X, J, D) (((X) << 28) | ((J) << 24) | D)
#define cell_free(C) (C = 0)
#define cell_tooManyCrossOver(C) ((C) & 0xF0000000 == 0xF0000000)
#define cell_stuff(C) (cell_isZero(C) ? 0x1B000000 : (cell_tooManyCrossOver(C) ? 0xFB000000 :(C + 0x10000000))


uint32 hash(Arrow tail, Arrow head) {
  return (tail << 18) ^ (tail >> 4) ^ head;
}

#define hash1(tail, head) (hash(tail, head) % PRIM0)
#define hash2(tail, head) (hash(tail, head) % PRIM1)

uint32 hashChain(address, cell) { POUR OFFSET DE CHAINE
  cell << 18 ^ cell >> 4 ^ address  => head // JEN SUIS LA
}


Arrow Eve() { return Eve; }

Arrow _arrow(Arrow tail, Arrow head, int locateOnly) {
  uint32 h, h1, h2, h3;
  uint32 a, next;
#define shiftTo(H,N,O,S) (N = ((H + O) % SPACE_SIZE), assert(N != S))

  if (tail == Eve && head == Eve) {
    return Eve;
  }

  h = hash(tail, head);
  h1 = h % PRIM1;
  h2 = h % PRIM2;

  a = h1;
  cell1 = space_get(a);
  while (!cell_isZero(cell1)) {
    if (cell_isArrow(cell1) && cell_getData(cell1) == tail) {
      // good start
      // chain offset
      h3 = hash(a, tail) % PRIM1; 
      shiftTo(a, next, h3, a);
      cell2 = space_get(next);
      if (cell_getData(cell2) == head) {
        return a; // found arrow
      }
      a = next;
    }
    shiftTo(a, a, h2, h1);
    cell1 = space_get(a);
  }

  if (locateOnly)
    return Eve;

  // miss, let's put the arrow now!
  // but we need 2 consecutive free cells in a chain
  h3 = hash(a, tail) % PRIM1; 
  shiftTo(a, next, h3, next);
  cell2 = space_get(next);
  while (!cell_isFree(cell2)) {
    space_set(a, cell_stuff(cell1));
    shift(a, a, h2, h1);
    cell1 = space_get(a);
    while (!cell_isFree(cell1)) {
      shiftTo(a, a, h2, h1);
    }
    h3 = hash(a, tail) % PRIM1; 
    shiftTo(a, next, h3, next);
    cell2 = space_get(next);
  }

  // (cell_isFree(cell1) && cell_isFree(cell2))
  cell1 = cell_build(cell_crossover(cell1), ARROW, tail);
  cell2 = cell_build(cell_crossover(cell2), FOLLOW, head);
  space_set(a, cell1);
  space_set(next, cell2);
}

Arrow arrow(tail, head) { return _arrow(tail, head, 0); }
Arrow locate(tail, head) { return _arrow(tail, head, 1); }

Arrow string(char* s) {
}

Arrow blob(uint32 size, char* data) {
}

Arrow root(Arrow) {
}

void unroot(Arrow) {
}

void init() {
  if (space_init()) { // very first start 
    // Eve
    space_set(0, ARROW, 0);
    space_set(1, 0, 0);
    space_commit();
  }
}
