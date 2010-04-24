#include <stdio.h>
#include <stdlib.h>
#include "_entrelacs.h"
#include "mem0.h"
#include "space.h"
#define SUCCESS 0

/*
 * Eve
 */
const Arrow Eve = 0;

/*
 * _blobTag : A system defined tag-arrow used to store BLOB
 */
static Arrow _blobTag = Eve;
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
#define cell_cross(C) (cell_isZero(C) ? 0x1C000000 : (cell_isOvercrossed(C) ? 0xFC000000 :(C + 0x10000000))

/** change C cell into a freed cell.
 * if the cell is crossed over, it becomes a stuffed cell
 * else, it becomes a ZERO cell
 */
// TBD: use mask to stuff
#define cell_free(C) (cell_getCrossover(C) ? cell_build(cell_getCrossover(c), STUFFING, 0) : 0)

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
#define cell_isArrow(C) ((C) & 0x0F000000) >= 0x0E000000)
#define cell_isTag(C) ((C) & 0x0F000000) == 0x0D000000)
// #define cell_isBlob(C) ((C) & 0x0F000000) == 0x0C000000)
#define cell_isFollower(C) ((C) & 0x0F000000) < 0x0C000000)
#define cell_isRooted(C) ((C) & 0x0F000000) == 0x0F000000)
#define cell_isOvercrossed(C) ((C) & 0xF0000000 == 0xF0000000)


/* ________________________________________
 * Hash functions
 * One computes 3 kinds of hashcode.
 * * h1: the default cell address for a given arrow.
 *     h1 = hash(tail, head) % PRIM1 for a regular arrow
 *     h1 = hashString(string) for a tag arrow
 * * h2: the probe offset when h1 cell is busy and one looks for a free cell nearby.
 *     h2 = hash(tail, head) % PRIM2 for a regular arrow
 *  
 */

/* hash function to get H1 from a regular arrow definition */
uint32 hash(Arrow tail, Arrow head) {
  return (tail << 20) ^ (tail >> 4) ^ head;
}

/* hash function to get H3 from a cell caracteristics */
uint32 hashChain(Address address, CellData cell) { // Data chain hash offset
  // This hash mixes the cell address and its content
  return (cell << 20) ^ (cell >> 4) ^ address;
}

/* hash function to get H1 from a tag arrow definition */
uint32 hashString(char *s) { // simple string hash
  uint32 hash = 5381;
  int c;

  while (c = *str++)
    // 5 bits shift only because ASCII generally works within 5 bits
    hash = ((hash << 5) + hash) + c;

  return hash;
}

#define hash1(tail, head) (hash(tail, head) % PRIM0)
#define hash2(tail, head) (hash(tail, head) % PRIM1)
#define hash3(address, cellData) (hashChain(a, cellData) % PRIM2)



/* ________________________________________
 * The blob cryptographic hash computing fonction.
 * This function returns a string used to define a tag arrow.
 *
 */char sha_buff[256];
char* sha() { /* TBD ... */ return sha_buf; }


/* ________________________________________
 * Address shifting and jumping macros
 *
 */
#define shift(H,N,O,S) (N = ((H + O) % SPACE_SIZE), assert(N != S))
#define jump(C,H,N,O,S) (N = ((H + O * cell_getJump(C)) % SPACE_SIZE), assert(N != S))

/** return Eve */ 
Arrow Eve() { return Eve; }

/** retrieve tail->head arrow,
 * by creating the arrow singleton in the arrow space if not found.
 * except if locateOnly param set.
 */
Arrow _arrow(Arrow tail, Arrow head, int locateOnly) {
  uint32 h, h1, h2, h3;
  Address a, next, stuff;
  Cell cell1, cell2;

  // Eve special case
  if (tail == Eve && head == Eve) {
    return Eve;
  }

  // Compute hashs
  h = hash(tail, head);
  h1 = h % PRIM1; // base address
  h2 = h % PRIM2; // probe offset

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
      h3 = hashChain(a, tail) % PRIM2; 
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
  h3 = hashChain(a, tail) % PRIM2;
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
    h3 = hashChain(a, tail) % PRIM2; 
    shift(a, next, h3, next);
    cell2 = space_get(next);
  }

  // (cell_isFree(cell1) && cell_isFree(cell2))
  cell1 = cell_build(cell_getCrossover(cell1), ARROW, tail);
  cell2 = cell_build(cell_getCrossover(cell2), 0, head);
  space_set(a, cell1, MEM1_LOOSE);
  space_set(next, cell2, 0);

  return a; // we did it;
}

/** retrieve a tag arrow defined by str string,
 * by creating the arrow singleton if not found,
 * except if locateOnly param is set
 */
Arrow _tag(char* str, int locateOnly) {
  uint32 h, h1, h2, h3;
  Address a, next, stuff;
  Cell cell1, cell2, content;
  unsigned i;
  char c, *p;
  if (!str) {
    return Eve;
  }

  h = hashString(str);
  h1 = h % PRIM1;
  h2 = h % PRIM2;

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
      h3 = hashChain(a, h1) % PRIM2; 
      p = str;
      shift(a, next, h3, a);
      cell = space_get(next);
      i = 2; // The first byte is reserved for a ref counter.
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
  h3 = hashChain(a, h1) % PRIM2; 
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
    h3 = hashChain(a, h1) % PRIM2;
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
  return a;
}

/** retrieve a blob arrow defined by 'size' bytes pointed by 'data',
 * by creating the arrow singleton if not found,
 * except if locateOnly param is set
 */
Arrow _blob(uint32 size, char* data, int locateOnly) {
  char *h = sha(size, data);

  // Prototype only: a BLOB is stored into a pair arrow from "_blobTag" to the blog cryptographic hash. The blob data is stored separatly outside the arrows space.
  mem0_saveData(h, size, data);
  // TO DO: remove the data when cell at h is recycled.

  Arrow t = _tag(h, locateOnly);
  if (t == Eve) return Eve;
  return arrow(_blobTag, t, locateOnly);
}

Arrow arrow(tail, head) { return _arrow(tail, head, 0); }
Arrow tag(char* s) { return _tag(s, 0);}
Arrow blob(uint32 size, char* data) { return _blob(size, data, 0);}

Arrow locateArrow(tail, head) { return _arrow(tail, head, 1); }
Arrow locateTag(char* s) { return _tag(s, 1};}
Arrow locateBlob(uint32 size, char* data) { return _blob(size, data, 1);}

Arrow headOf(Arrow a) {
  Arrow next, tail = tailOf(a);
  uint32 h3 = hashChain(a, tail) % PRIM2;
  shift(a, next, h3, a);
  return cell_getData(space_get(next));
}

Arrow tailOf(Arrow a) {
  return (Arrow)cell_getData(space_get(a));
}

char* tagOf(Arrow a) {
  cell h1 = space_get(a);
  uint32 h3 = hashChain(a, h1) % PRIM2; 
  shift(a, next, h3, a);
  cell = space_get(next);

  char* tag =  NULL, p = NULL;
  uint32 size = 0;
  uint32 max = 0;
  geoalloc(&tag, &max, &size, sizeof(char), 1);
  while ((*p++ = ((char*)&cell)[i])) {
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
  *p = '\0';

  return tag;
}

char* blobOf(Arrow a, uint32* lengthP) {
  char* h = tagOf(headOf(a));
  char* data = mem0_loadData(h, lengthP);
  free(h);
  return data;
}


int isEve(Arrow a) { return (a == Eve); }


enum e_type typeOf(Arrow a) {
  if (a == Eve) return TYPE_EVE;
  
  Cell cell = space_get(a);
  if (cell_isArrow(cell)) {
    if (headOf(a) == _blobTag)
      return TYPE_BLOB;
    else
      return TYPE_ARROW;
  }
  else if (cell_isTag(cell))
    return TYPE_TAG;
  else
    return TYPE_UNDEF;
}

/** connect a child arrow to its parent arrow
 * * if the parent arrow is a tag, one increments the ref counter at a+h3
 * * *  Exception: if the tag ref counter reached its max value, one can't move it anymore. It means the tag will never be removed from the storage.
 * * if the parent arrow is a pair, one add the child back-ref in the "h3 jump list" starting from a+h3
 * * * if the parent arrow was in LOOSE state (eg. a newly defined pair with no other child arrow), one connects the parent arrow to its head and tail (recursive calls) and one removes the LOOSE mem1 flag attached to 'a' cell
 */
void connect(Arrow a, Arrow child) {
  if (a == Eve) return; // One doesn't store Eve connectivity.
  Cell cell = space_get(a);
  if (cell_isTag(cell)) {
    if (space_getAdmin(a) == MEM1_LOOSE) {
      // One removes the Mem1 LOOSE flag at "a" address.
      space_set(a, cell, 0);
    }

    // One doesn't attach back reference to tags. One only increments a ref counter.
    // If you want connectivity data, think on building pairs of tag.
    // the ref counter is located in the 1st byte of the h3-next cell content
    uint32 h1 = cell_getData(cell);
    h3 = hashChain(a, h1) % PRIM2; 
    Arrow  current;
    shift(a, current, h3, a);
    cell = space_get(current);
    unsigned char c = ((char*)&cell)[1];
    if (!(++c)) c = 255;// Max value reached.
    ((char*)&cell)[1] = c;
    space_set(current, cell, 0);
    
  } else { //  ARROW, ROOTED, BLOB...
    if (space_getAdmin(a) == MEM1_LOOSE) {
      // One removes the mem1 LOOSE flag attached to 'a' cell.
      space_set(a, cell, 0);
      // One recursively connects the parent arrow to its ancestors.
      connect(tailOf(a), a);
      connect(headOf(a), a);
    }

    // Children arrows are chained in the h3 jump list starting from a+h3.
    Arrow tail = tailOf(a);
    uint32 h3 = hashChain(a, tail) % PRIM2;
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
void disconnect(Arrow a, Arrow child) {
  if (a == Eve) return; // One doesn't store Eve connectivity.

  Cell cell = space_get(a);
  if (cell_isTag(cell)) { // parent arrow is a TAG

    // One decrements a ref counter in 1st data byte at a+h3 cell.
    uint32 h1 = cell_getData(cell);
    h3 = hashChain(a, h1) % PRIM2; 
    Arrow  current;
    shift(a, current, h3, a);
    cell = space_get(current);
    unsigned char c = ((char*)&cell)[1];
    if (c != 255) {
      c--;// One decrements the tag ref counter only if not stuck to its max value.
      ((char*)&cell)[1] = c; // reminder: cell[0] contains administrative bits.
      space_set(current, cell, 0);
  
      if (!c) { // HE! This tag is totally unreferred
        // Let's switch on its LOOSE status.
        space_setAdmin(a, MEM1_LOOSE);
      }
    }
    
  } else { //  ARROW, ROOTED, BLOB...
    uint32 rooted = cell_isRooted(cell);
 
    // One removes the arrow from its parent h3-chain of back-references.
    Arrow tail = tailOf(a);
    uint32 h3 = hashChain(a, tail) % PRIM2;

    // back-ref search loop, jumping from A+H3 (actually, A+H3 contains head)
    Arrow current;
    shift(a, current, h3, a);
    cell = space_get(current);

    Arrow previous;
    uint32 stillRemaining = 0;
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
    uint32 crossover = cell_getCrossover(cell);
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
        // One recursively disconnects the arrow from its ancestors.
        disconnect(tailOf(a), a);
        disconnect(headOf(a), a);
      }

    } else { // generic case
      uint32 previousCell = space_get(previous);
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

/** root an arrow */
Arrow root(Arrow a) {  
  Cell cell = space_get(a);
  if (!cell_isArrow(cell) || cell_isRooted(cell)) return Eve;
  Arrow tail = cell_getData(cell);

  int loose = (space_getAdmin(a) == MEM1_LOOSE); // checked before set to zero hereafter
  // change the arrow to ROOTED state + remove the Mem1 LOOSE state is any 
  space_set(a, cell_build(cell_getCrossover(cell), ROOTED, tail), 0);

  if (loose) { // if the arrow has just lost its LOOSE state, one connects it to its ancestor
    connect(tail, a);
    connect(headOf(a), a);
  }
  return a;
}

/** unroot an arrow */
void unroot(Arrow a) {
  Cell cell = space_get(a);
  if (!cell_isRooted(cell)) return;
  Arrow tail = cell_getData(cell);


  // back-ref search loop, jumping from A+H3 (actually, A+H3 contains head)
  uint32 h3 = hashChain(a, tail) % PRIM2;
  Arrow a_h3;
  shift(a, a_h3, h3, a);
  cell headCell = space_get(a_h3);
  // When the parent arrow is unreferred, one switches on its LOOSE status.
  uint32 loose = (cell_getJump(headCell) == 0);
  space_set(a, cell_build(cell_getCrossover(cell), ARROW, tail), (loose ? MEM1_LOOSE : 0));

  if (loose) {
    // If loose, one recusirvily disconnects the arrow from its ancestors.
    disconnect(tail, a);
    disconnect(cell_getData(headCell), a);
  }
}

/** return the root status */
int isRooted(Arrow a) {
  return (cell_isRooted(space_get(a)));
}

/** TBD */
int isLoose(Arrow a) {
  Cell  cell = space_get(a);
  Arrow data = cell_getData(a);

  if (cell_isTag(a)) {
  }
}


int equal(Arrow a, Arrow b) {
  return (a == b);
}

/** initialize the Entrelacs system */
void init() {
  if (space_init()) { // very first start 
    // Eve
    space_set(0, ARROW, 0, 0);
    space_set(1, 0, 0, 0);
    space_commit();
  }
  _blobTag = tag(BLOB_TAG);
}
