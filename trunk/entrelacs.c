#include <stdio.h>
#include <stdlib.h>
#include "mem0.h"
#include "entrelacs/entrelacs.h"
#include "space.h"
#include "sha1.h"
#define SUCCESS 0

/*
 * Eve
 */
#define EVE (0)
const Arrow Eve = EVE;

/*
 * _blobTag : A system defined tag-arrow used to store BLOB
 */
static Arrow blobTag = EVE;
#define BLOB_TAG "#BLOB"



/* ________________________________________
 * Cell administrative bits
 */
#define ROOTED 0xF
#define ARROW 0xE
#define TAG 0xD
// #define BLOB 0xC
#define STUFFING 0xC
#define MAX_JUMP 0xA
#define MAX_CROSSOVER 0xF

/* Administrative bits of volatile memory cached cell
 */
#define MEM1_LOOSE 0x1


/* ________________________________________
 * Cell building macros
 *
 * Each cell (32 bits) is structured as follow
 *  Cell = <--X--><--J--><----------D----------> (32 bits)
 *  with
 *    X = Crossover (4 bits)
 *    J = Jump (4 bits) OR "Type ID"
 *    D = Data (24 bits)
 */

/** build a cell from X, J and D parts
 */
#define cell_build(X, J, D) (((X) << 28) | ((J) << 24) | D)

/** "cross" a C cell.
 * if C cell is a zero cell then it becomes a a stuffed cell (crossover = 1)
 * else whatever it is a stuffed cell or a used cell,
 * one simply increments its crossover counter
 */
#define cell_cross(C) (cell_isZero(C) ? 0x1C000000 : (cell_isOvercrossed(C) ? 0xFC000000 : (C + 0x10000000)))

/** change C cell into a freed cell.
 * if the cell is crossed over, it becomes a stuffed cell
 * else, it becomes a ZERO cell
 */
// TBD: use mask to stuff
#define cell_free(C) (cell_getCrossover(C) ? cell_build(cell_getCrossover(C), STUFFING, 0) : 0)

/* ________________________________________
 * Cell unbuilding macros
 */
#define cell_getCrossover(C) ((C) >> 28)
#define cell_getJump(C) (((C) >> 24) && 0xF)
#define cell_getData(C) ((C) & 0x00FFFFFF)

/* ________________________________________
 * Cell testing macros
 */
#define cell_isZero(C) ((C) == 0)
#define cell_isStuffed(C) (((C) & 0x0F000000) == 0x0C000000)
#define cell_isFree(C) (cell_isZero(C) || cell_isStuffed(C))
#define cell_isArrow(C) (((C) & 0x0F000000) >= 0x0E000000)
#define cell_isTag(C) (((C) & 0x0F000000) == 0x0D000000)
// #define cell_isBlob(C) (((C) & 0x0F000000) == 0x0C000000)
#define cell_isFollower(C) (((C) & 0x0F000000) < 0x0C000000)
#define cell_isRooted(C) (((C) & 0x0F000000) == 0x0F000000)
#define cell_isOvercrossed(C) (((C) & 0xF0000000) == 0xF0000000)


// The loose stack.
// This is a stack of all newly created arrows still in loose state.
static Address* looseStack = NULL;
static Address  looseStackMax = 0;
static Address  looseStackSize = 0;

static looseStackAdd(Address a) {
  geoalloc(&looseStack, &looseStackMax, &looseStackSize, sizeof(Address), looseStackSize + 1);
  looseStack[looseStackSize - 1] = a;
}

static looseStackRemove(Address a) {
  if (!looseStackSize) return;
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
 * * h1: the default cell address for a given arrow.
 *     h1 = hash(tail, head) % PRIM0 for a regular arrow
 *     h1 = hashString(string) for a tag arrow
 * * h2: the probe offset when h1 cell is busy and one looks for a free cell nearby.
 *     h2 = hash(tail, head) % PRIM1 for a regular arrow
 *  
 */

/* hash function to get H1 from a regular arrow definition */
uint32_t hash(Arrow tail, Arrow head) {
  return (tail << 20) ^ (tail >> 4) ^ head;
}

/* hash function to get H3 from a cell caracteristics */
uint32_t hashChain(Address address, CellData cell) { // Data chain hash offset
  // This hash mixes the cell address and its content
  return (cell << 20) ^ (cell >> 4) ^ address;
}

/* hash function to get H1 from a tag arrow definition */
uint32_t hashString(char *s) { // simple string hash
  uint32_t hash = 5381;
  int c;

  while (c = *s++)
    // 5 bits shift only because ASCII generally works within 5 bits
    hash = ((hash << 5) + hash) + c;

  return hash;
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
 *
 */
#define shift(H, N, O, S) (N = ((H + O) % SPACE_SIZE), assert(N != S))
#define _jump(J, H, N, O, S) (N = ((H + O * J) % SPACE_SIZE), assert(N != S))
#define jump(C, H, N, O, S) _jump(cell_getJump(C), H, N, O, S)

/** return Eve */ 
Arrow xl_Eve() { return Eve; }

/** retrieve tail->head arrow,
 * by creating the arrow singleton in the arrow space if not found.
 * except if locateOnly param set.
 */
static Arrow arrow(Arrow tail, Arrow head, int locateOnly) {
  uint32_t h, h1, h2, h3;
  Address a, next, stuff;
  Cell cell1, cell2;

  // Eve special case
  if (tail == Eve && head == Eve) {
    return Eve;
  }

  // Compute hashs
  h = hash(tail, head);
  h1 = h % PRIM0; // base address
  h2 = h % PRIM1; // probe offset
  if (!h2) h2 = 2; // offset can't be 0 

  // One searches for an existing copy of this arrow
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
      h3 = hashChain(a, tail) % PRIM1;
      if (!h3) h3 = 1; // offset can't be 0

      shift(a, next, h3, a);
      cell2 = space_get(next);
      if (cell_getData(cell2) == head)
        return a; // OK: arrow found!
    }
    // TBD:Bug here: I must not cross cell if I actually found the arrow
    space_set(a, cell_cross(cell1), DONTTOUCH);
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
  h3 = hashChain(a, tail) % PRIM1;
  if (!h3) h3 = 1; // offset can't be 0
  shift(a, next, h3, next);
  cell2 = space_get(next);

  while (!cell_isFree(cell2)) {
    // one stuffes/crosses unusable empty cell
    space_set(a, cell_cross(cell1), DONTTOUCH);
    shift(a, a, h2, h1);
    cell1 = space_get(a);
    while (!cell_isFree(cell1)) {
      // one stuffes/crosses unusable empty cell
      space_set(a, cell_cross(cell1), DONTTOUCH);
      shift(a, a, h2, h1);
    }
    h3 = hashChain(a, tail) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
    shift(a, next, h3, next);
    cell2 = space_get(next);
  }

  // (cell_isFree(cell1) && cell_isFree(cell2))
  cell1 = cell_build(cell_getCrossover(cell1), ARROW, tail);
  cell2 = cell_build(cell_getCrossover(cell2), 0, head);
  space_set(a, cell1, MEM1_LOOSE);
  space_set(next, cell2, 0);

  // loose log
  looseStackAdd(a);
  return a;
}

/** retrieve a tag arrow defined by str string,
 * by creating the arrow singleton if not found,
 * except if locateOnly param is set
 */
static Arrow tag(char* str, int locateOnly) {
  uint32_t h, h1, h2, h3;
  Address a, current, next, stuff;
  Cell cell1, cell2, content, cell;
  unsigned i;
  char c, *p;
  if (!str) {
    return Eve;
  }

  h = hashString(str);
  h1 = h % PRIM0;
  h2 = h % PRIM1;
  if (!h2) h2 = 2; // offset can't be 0

  a = h1;
  stuff = Eve;
  cell1 = space_get(a);
  while (!cell_isZero(cell1)) {
    if (cell_isStuffed(cell1)) {
      if (stuff == Eve) 
        stuff = a; // one remembers first stuffed cell while scannning
    } else if (cell_isTag(cell1) && cell_getData(cell1) == h1) {
      // good start
      // now one must compare the whole string, char by char
      h3 = hashChain(a, h1) % PRIM1; 
      if (!h3) h3 = 1; // offset can't be 0
      p = str;
      shift(a, next, h3, a);
      cell = space_get(next);
      i = 2; // The first content byte (cell_at_h3[1]) is reserved for a ref counter.
      while ((c = *p++) == ((char*)&cell)[i] && c != 0) {
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
      if (!c && !((char*)&cell)[i])
        return a; // found arrow

      // One crosses the busy cell in order to prevent it to be zero-ed
      // BUG TBD : one crosses only when creating the arrow
      space_set(a, cell_cross(cell1), DONTTOUCH);
      shift(a, a, h2, h1);

      cell1 = space_get(a);
    } // isTag

  } // !zero

  if (locateOnly)
    return Eve;

  // miss, so let's put the arrow into the space.
  // but we need 2 consecutive free cells in a chain
  if (stuff) a = stuff;

  // h3 is the chain offset, h3 is not h1
  h3 = hashChain(a, h1) % PRIM1; 
  if (!h3) h3 = 1; // offset can't be 0
  shift(a, next, h3, next);
  cell2 = space_get(next);

  while (!cell_isFree(cell2)) {
    // one stuffes/crosses unusable empty cell one found
    space_set(a, cell_cross(cell1), DONTTOUCH);
    shift(a, a, h2, h1);
    cell1 = space_get(a);
    while (!cell_isFree(cell1)) {
      // one crosses every cell on the way
      space_set(a, cell_cross(cell1), DONTTOUCH);
      shift(a, a, h2, h1);
    }
    h3 = hashChain(a, h1) % PRIM1;
    if (!h3) h3 = 1; // offset can't be 0
    shift(a, next, h3, next);
    cell2 = space_get(next);
  }
  // 2 consecutive free cells in a h3 sequence found.
  
  // Now, one writes the TAG definition

  // First H2, then all chars
  cell1 = cell_build(cell_getCrossover(cell1), TAG, h2);
  space_set(a, cell1, 0);
  current = next;
  cell1 = cell2;
  // h3 already set
  p = str;
  i = 1;
  content = 0;
  c = 0;  /* first byte = ref counter = 0 */
  for (;;) {
    ((char*)&content)[i] = c; 
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
      space_set(current, cell1, 0);
      current = next;
      cell1 = cell2;
      i = 1;
      content = 0;
    }

    c = *p++;
    if (!c) break;
  }
  // useless : ((char*)&content)[i] = c; // 0 
  cell1 = cell_build(cell_getCrossover(cell1), 0, content);
  space_set(current, cell1, 0);

  // loose log
  looseStackAdd(a);

  return a;
}

/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the arrow singleton if not found,
 * except if locateOnly param is set
 */
static Arrow blob(uint32_t size, char* data, int locateOnly) {
  char h[20];
  crypto(size, data, h);

  // Prototype only: a BLOB is stored into a pair arrow from "blobTag" to the blog cryptographic hash. The blob data is stored separatly outside the arrows space.
  mem0_saveData(h, size, data);
  // TO DO: remove the data when cell at h is recycled.

  Arrow t = tag(h, locateOnly);
  if (t == Eve) return Eve;
  return arrow(blobTag, t, locateOnly);
}

Arrow xl_arrow(Arrow tail, Arrow head) { return arrow(tail, head, 0); }
Arrow xl_tag(char* s) { return tag(s, 0);}
Arrow xl_blob(uint32_t size, char* data) { return blob(size, data, 0);}

Arrow xl_arrowMaybe(Arrow tail, Arrow head) { return arrow(tail, head, 1); }
Arrow xl_tagMaybe(char* s) { return tag(s, 1);}
Arrow xl_blobMaybe(uint32_t size, char* data) { return blob(size, data, 1);}

Arrow xl_headOf(Arrow a) {
  Arrow tail = xl_tailOf(a);
  uint32_t h3 = hashChain(a, tail) % PRIM1;
  if (!h3) h3 = 1; // offset can't be 0
  Address a_h3;
  shift(a, a_h3, h3, a);
  return (Arrow)cell_getData(space_get(a_h3));
}

Arrow xl_tailOf(Arrow a) {
  return (Arrow)cell_getData(space_get(a));
}

char* xl_tagOf(Arrow a) {
  Cell cell = space_get(a);
  assert(cell_isTag(cell));
  uint32_t h1 = cell_getData(cell);
  uint32_t h3 = hashChain(a, h1) % PRIM1;
  if (!h3) h3 = 1; // offset can't be 0
  Address next;

  shift(a, next, h3, a);
  cell = space_get(next);
  unsigned i = 2; // not 1, byte #1 contains ref counter
  char* tag =  NULL;
  uint32_t size = 0;
  uint32_t max = 0;
  geoalloc(&tag, &max, &size, sizeof(char), 1);
  while ((tag[size] = ((char*)&cell)[i])) {
    geoalloc(&tag, &max, &size, sizeof(char), size + 1);
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
  return tag;
}

char* xl_blobOf(Arrow a, uint32_t* lengthP) {
  char* h = xl_tagOf(xl_headOf(a));
  char* data = mem0_loadData(h, lengthP);
  free(h);
  return data;
}


int xl_isEve(Arrow a) { return (a == Eve); }


enum e_xlType xl_typeOf(Arrow a) {
  if (a == Eve) return XL_EVE;
  
  Cell cell = space_get(a);
  if (cell_isArrow(cell)) {
    if (xl_headOf(a) == blobTag)
      return XL_BLOB;
    else
      return XL_ARROW;
  }
  else if (cell_isTag(cell))
    return XL_TAG;
  else
    return XL_UNDEF;
}

/** connect a child arrow to its parent arrow
 * * if the parent arrow is a tag, one increments the ref counter at a+h3
 * * *  Exception: if the tag ref counter reached its max value, one can't move it anymore. It means the tag will never be removed from the storage.
 * * if the parent arrow is a pair, one add the child back-ref in the "h3 jump list" starting from a+h3
 * * * if the parent arrow was in LOOSE state (eg. a newly defined pair with no other child arrow), one connects the parent arrow to its head and tail (recursive calls) and one removes the LOOSE mem1 flag attached to 'a' cell
 */
static void connect(Arrow a, Arrow child) {
  if (a == Eve) return; // One doesn't store Eve connectivity.
  Cell cell = space_get(a);
  if (cell_isTag(cell)) {
    if (space_getAdmin(a) == MEM1_LOOSE) {
      // One removes the Mem1 LOOSE flag at "a" address.
      space_set(a, cell, 0);
      // (try to) remove 'a' from the loose log
      looseStackRemove(a);
    }

    // One doesn't attach back reference to tags. One only increments a ref counter.
    // If you want connectivity data, think on building pairs of tag.
    // the ref counter is located in the 1st byte of the h3-next cell content
    uint32_t h1 = cell_getData(cell);
    uint32_t h3 = hashChain(a, h1) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
    Address a_h3;
    shift(a, a_h3, h3, a);
    cell = space_get(a_h3);
    unsigned char c = ((char*)&cell)[1];
    if (!(++c)) c = 255;// Max value reached.
    ((char*)&cell)[1] = c;
    space_set(a_h3, cell, 0);
    
  } else { //  ARROW, ROOTED, BLOB...
    if (space_getAdmin(a) == MEM1_LOOSE) {
      // One removes the mem1 LOOSE flag attached to 'a' cell.
      space_set(a, cell, 0);
      // (try to) remove 'a' from the loose log
      looseStackRemove(a);
      // One recursively connects the parent arrow to its ancestors.
      connect(xl_tailOf(a), a);
      connect(xl_headOf(a), a);
    }

    // Children arrows are chained in the h3 jump list starting from a+h3.
    Arrow tail = xl_tailOf(a);
    uint32_t h3 = hashChain(a, tail) % PRIM1;
    if (!h3) h3 = 1; // offset can't be 0
    Arrow current;
    shift(a, current, h3, a);
    Cell cell = space_get(current);
    while (cell_getData(cell) != 0 && cell_getJump(cell) != 0) {
        jump(cell, current, current, h3, a);
        cell = space_get(current);
    }

    if (cell_getData(cell) != 0) {
      // if we went up to the h3/jump list end

      // free cell search with jump computing
      Arrow next;
      Cell cell2;
      unsigned jump = 1;
      shift(current, next, h3, a);
      cell2 = space_get(next);
      while (!cell_isFree(cell2)) {
        jump++;
        shift(next, next, h3, a);
        cell2 = space_get(next);
      }
      // TBD: what if jump > 15
      assert(jump <= 15);
      cell2 = cell_build(cell_getCrossover(cell2), 0, child);
      space_set(next, cell2, 0);
      cell = cell_build(cell_getCrossover(cell), jump && 15, cell_getData(cell));
      space_set(current, cell, 0);
    } else { // free cell found within the sequence.
      cell = cell_build(cell_getCrossover(cell), cell_getJump(cell), child);
      space_set(current, cell, 0);
    }

  }
    
}

/** disconnect a child arrow from its parent arrow
 * * if the parent arrow is a tag, one decrements the ref counter at a+h3
 * * *  Exception: if the tag ref counter reached its max value, one can't decrement it anymore. It means the tag will never be removed.
 * * if the parent arrow is a pair, one removes the child back-ref in the "h3 jump list" starting from a+h3
 * * * if the parent arrow is consequently unreferred (ie. a not rooted pair arrow with no more child arrow), the parent arrow is disconnected itself from its head and tail (recursive calls) and one raises the LOOSE mem1 flag attached to 'a' cell 
 */
static void disconnect(Arrow a, Arrow child) {
  if (a == Eve) return; // One doesn't store Eve connectivity.

  Cell cell = space_get(a);
  if (cell_isTag(cell)) { // parent arrow is a TAG

    // One decrements a ref counter in 1st data byte at a+h3 cell.
    uint32_t h1 = cell_getData(cell);
    uint32_t h3 = hashChain(a, h1) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
    Address  a_h3;
    shift(a, a_h3, h3, a);
    cell = space_get(a_h3);
    unsigned char c = ((char*)&cell)[1];
    if (c != 255) {
      c--;// One decrements the tag ref counter only if not stuck to its max value.
      ((char*)&cell)[1] = c; // reminder: cell[0] contains administrative bits.
      space_set(a_h3, cell, 0);
  
      if (!c) { // HE! This tag is totally unreferred
        // Let's switch on its LOOSE status.
        space_setAdmin(a, MEM1_LOOSE);
        // add 'a' to the loose log
        looseStackAdd(a);
      }
    }
    
  } else { //  ARROW, ROOTED, BLOB...
    uint32_t rooted = cell_isRooted(cell);
 
    // One removes the arrow from its parent h3-chain of back-references.
    Arrow tail = xl_tailOf(a);
    uint32_t h3 = hashChain(a, tail) % PRIM1;
    if (!h3) h3 = 1; // offset can't be 0

    // back-ref search loop, jumping from A+H3 (actually, A+H3 contains head)
    Arrow current;
    shift(a, current, h3, a);
    cell = space_get(current);

    Arrow previous;
    uint32_t stillRemaining = 0;
    while (cell_getJump(cell) != 0) {
      previous = current;
      jump(cell, previous, current, h3, a);
      cell = space_get(current);
      if (cell_getData(cell) == child) break;
      stillRemaining = 1; // we found at least one back-ref which is not "child"
    }

    assert(cell_getData(cell) == child);
    unsigned jump = cell_getJump(cell);
    
    // back-ref cell recycled either as a free or STUFFED cell
    uint32_t crossover = cell_getCrossover(cell);
    cell = (crossover ? cell_build(cell_getCrossover(cell), STUFFING, 0) : 0);
    space_set(current, cell, 0);

    // Now, one moves the jump amount of the erased cell to the previous cell in the chain
    if (jump == 0) { // specific case when the erased cell is the last chained cell
      cell = space_get(previous);
      cell = cell_build(cell_getCrossover(cell), 0, cell_getData(cell));
      space_set(previous, cell, 0);

      if (!rooted && !stillRemaining) { // we removed the only remaining back-ref
        // The parent arrow is unreferred. Let's switch on its LOOSE status.
        space_setAdmin(a, MEM1_LOOSE);
        // add 'a' to the loose log
        looseStackAdd(a);
        // One recursively disconnects the arrow from its ancestors.
        disconnect(xl_tailOf(a), a);
        disconnect(xl_headOf(a), a);
      }

    } else { // generic case
      uint32_t previousCell = space_get(previous);
      unsigned previousJump = cell_getJump(previousCell);
      previousJump += jump;
      if (previousJump <= 15) {
        previousCell = cell_build(cell_getCrossover(previousCell), previousJump,
                                  cell_getData(previousCell));
        space_set(previous, previousCell, 0);
 
      } else { // Damned! I can't jump up to the next cell
        // finally one just resets the back-ref in the freed cell
        space_set(current, cell_build(crossover, jump, 0), 0);
      }
    }

  }
    
}

void xl_childrenOf(Arrow a, XLCallBack cb, char* context) {
  Arrow child;
  Cell cell = space_get(a);
  if (cell_isArrow(cell)) {
    Arrow tail = cell_getData(cell);
    uint32_t h3 = hashChain(a, tail) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
      
    Address current;
    shift(a, current, h3, a);
    cell = space_get(current);
    unsigned jump = cell_getJump(cell);
    while (jump) {
      _jump(jump, current, current, h3, a);
      cell = space_get(current);
      child = (Arrow)cell_getData(current);
      if (cb(child, context)) break;
      jump = cell_getJump(cell);
    }
  }  
}

/** root an arrow */
Arrow xl_root(Arrow a) {  
  Cell cell = space_get(a);
  if (!cell_isArrow(cell) || cell_isRooted(cell)) return Eve;
  Arrow tail = cell_getData(cell);

  int loose = (space_getAdmin(a) == MEM1_LOOSE); // checked before set to zero hereafter
  // change the arrow to ROOTED state + remove the Mem1 LOOSE state is any 
  space_set(a, cell_build(cell_getCrossover(cell), ROOTED, tail), 0);

  if (loose) { // if the arrow has just lost its LOOSE state, one connects it to its ancestor
    connect(tail, a);
    connect(xl_headOf(a), a);
    // (try to) remove 'a' from the loose log
    looseStackRemove(a);
  }
  return a;
}

/** unroot a rooted arrow */
void xl_unroot(Arrow a) {
  Cell cell = space_get(a);
  if (!cell_isRooted(cell)) return; // one can only unroot a rooted regular arrow


  // back-ref search loop, jumping from A+H3 (actually, A+H3 contains head)
  Arrow tail = cell_getData(cell);
  uint32_t h3 = hashChain(a, tail) % PRIM1;
  if (!h3) h3 = 1; // offset can't be 0
  Address a_h3;
  shift(a, a_h3, h3, a);
  Cell headCell = space_get(a_h3);
  // When the arrow is not referred, one switches on its LOOSE status.
  uint32_t loose = (cell_getJump(headCell) == 0);
  space_set(a, cell_build(cell_getCrossover(cell), ARROW, tail), (loose ? MEM1_LOOSE : 0));

  if (loose) {
    // If loose, one recusirvily disconnects the arrow from its ancestors.
    disconnect(tail, a);
    disconnect(cell_getData(headCell), a);
    // loose log
    looseStackAdd(a);
  }
}

/** return the root status */
int xl_isRooted(Arrow a) {
  return (cell_isRooted(space_get(a)));
}

int xl_isLoose(Arrow a) {
  if (a == Eve) return 0;
  if (space_getAdmin(a) == MEM1_LOOSE) return 1;

  Cell  cell = space_get(a);

  if (cell_isRooted(cell)) {
    return 0; // rooted ==> not loose!
  } else if (cell_isTag(cell)) {
    // the ref counter is located in the 1st byte of the h3-next cell content
    uint32_t h1 = cell_getData(cell);
    uint32_t h3 = hashChain(a, h1) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
    Address  a_h3;
    shift(a, a_h3, h3, a);
    cell = space_get(a_h3);
    unsigned char c = ((char*)&cell)[1];
    return (!c); // back-ref counter == 0 <==> loose!
  } else {
    // the arrow is loose if h3-next cell jump list is empty.
    Arrow tail = cell_getData(cell);
    uint32_t h3 = hashChain(a, tail) % PRIM1;
    if (!h3) h3 = 1; // offset can't be 0
    Address a_h3;
    shift(a, a_h3, h3, a);
    cell = space_get(a_h3);
    return (!cell_getJump(cell)); // no jump ==> loose!
  }
}


int xl_equal(Arrow a, Arrow b) {
  return (a == b);
}

static void forget(Arrow a) {
  Cell cell = space_get(a);
  // As a matter of fact, this is exactly the same code for tag or arrow
  // I let the both cases for tracability
  if (cell_isTag(cell)) {
    uint32_t h1 = cell_getData(cell);
    space_set(a, cell_free(cell), 0);

    uint32_t h3 = hashChain(a, h1) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
    Address  current;
    shift(a, current, h3, a);
    cell = space_get(current);
    while (1) {
      unsigned jump = cell_getJump(cell);
      space_set(current, cell_free(cell), 0);
      if (!jump) break;
      _jump(jump, current, current, h3, a);
      cell = space_get(current);
    }
    
  } else if (cell_isArrow(cell)) {
    Arrow tail = cell_getData(cell);
    space_set(a, cell_free(cell), 0);

    uint32_t h3 = hashChain(a, tail) % PRIM1; 
    if (!h3) h3 = 1; // offset can't be 0
    Address  current;
    shift(a, current, h3, a);
    cell = space_get(current);
    while (1) {
      unsigned jump = cell_getJump(cell);
      space_set(current, cell_free(cell), 0);
      if (!jump) break;
      _jump(jump, current, current, h3, a);
      cell = space_get(current);
    }
  }
}

void xl_commit() {
  unsigned i;
  for (i = 0; i < looseStackSize ; i++) { // loose stack scanning
    Arrow a = looseStack[i];
    if (xl_isLoose(a)) { // a loose arrow is removed NOW
      forget(a);
    }
  }
  zeroalloc(&looseStack, &looseStackMax, &looseStackSize);
  space_commit();
}

/** initialize the Entrelacs system */
void xl_init() {
  if (space_init()) { // very first start 
    // Eve
    space_set(0, cell_build(0, XL_ARROW, 0), 0);
    space_set(1, 0, 0);
    space_commit();
  }
  blobTag = xl_tag(BLOB_TAG);

  geoalloc(&looseStack, &looseStackMax, &looseStackSize, sizeof(Address), 0);
}
