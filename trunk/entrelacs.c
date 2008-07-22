#include <stdio.h>
#include <stdlib.h>

// Entrelacs System deals only with "arrows". Its credo could be "Everything is an arrow".
// A regular arrow is an immutable oriented pairs of (references to some other) arrows.
// BLOB or C-String are other kinds of arrows. They are immutable atomistish data similar to "arrows entrelacs".
// The system top-level context is made of all arrows which have their "rooted" flag set (see hereafter).
// Those arrows are "rooted arrows". They are spared by the "arrows sweeper" process, and so are all their "ancesters".

// System dynamics occure into transactions. Transactions are identified by an incrementing short number.
// Transaction number range is short, so that old ressources may wrongly be linked with current transactions. But it's actually not a problem.
typedef static char TransactionId;

// Arrows are persistently stored into a memory level 0 device (disk-like) where they are identified in there by a ref # typed Ref0.
typedef void* Ref0; // Memory Level 0 address
// Level 0 storage consists in a single memory bank divided into cells. Each cell can store one reference so that an arrow level 0 reference corresponds actually to a cell address.
typedef struct Cell0Structure { Ref0 content } Cell0;

// The level 0 memory bank puts every aspects of stored arrows (definition & neighbourhood) into locations computed by hash functions.
// On hash conflict, the level 0 memory bank makes use of the "open adressing" trick which is occasionaly used in hash table design. "Open adressing" consists in managing adress-conflicts by putting elements all together in a memory block starting from the conflicting adress. Such a memory block is called an "open block" hereafter.

// As different kind of reference may be present in a cell, references are classified by 4 adjacent reserved bits (nammed "flags" hereafter).
// 0 0 0 0 = PAIR = first part of a regular arrow definition (ie. its queue)
// 0 0 0 1 = REMAINing = subsequent part of definition (head and more for a regular arrow)
// 0 0 1 0 = INcoming = reference of an arrow whose head is an arrow defined in the ++ part of the block.
// 0 0 1 1 = OUTgoing = reference of an arrow whose queue is an arrow defined in the -- part of the block.
// 0 1 0 0 = IDX = reference of an arrow whose content-hash hits the -- part of the block
// 0 1 0 1 = BLOB = reference to a BLOB (atomistish raw data) whose content-hash hits the -- part of the block.
// 0 1 1 0 = STRING = reference to a C-String whose content-hash hits the -- part of the block.
// 0 1 1 1 = ROOTED PAIR = like a PAIR but ROOTED. It means it belongs to a top-level root context.
#define PAIR (0<<15)
#define REMAIN (1<<15)
#define IN (2<<15)
#define OUT(3<<15)
#define IDX (4<<15)
#define BLOB (5<<15)
#define STRING (6<<15)
#define ROOTED (7<<15)

// To keep only the flags part of a cell, compute a bit-AND with its content and TFLAGS
#defines TFLAGS (15<<15)
#define flags(A) ((A) & TFLAGS)

// To reset (to zero) the flags part of a cell, compute a bit-AND with its content and a bit-NOT of TFLAGS
#define REFPART (|TFLAGS)
#define noflags(A) ((A) & REFPART)

// When needed, arrows data climb up to memory level 1 where they are identified by a potentialy different ref # typed Ref1.
// The level 1 memory works as a fine-grained cache of level 0 cells. So it's an array of cell-containers.
// When climbing up to level 1, a cell is put at "Ref0" modulo cache size (yet an other hash).
// Conflicts management makes use of the "open adressing" trick as well. Conflicting cached cells are putted one after an other.
// Each cell container contains: a cell content, its corresponding Ref0, its caching state, its last usage transaction number
typedef void* Ref1; // Memory Level 1 address
typedef struct { Ref0 ref0; Ref1 content; int state; TransactionId transactionId } Cell1; // a cell container
static const unsigned long mem1Size = 256 << 7 ; // mem level 1 size
static Cell1 memory1[mem1Size]; // here is the whole thing

// Ref1<>Ref0 translation
Ref1 locate(Ref0); // get a cell and put it into the cache, returning its cache location
Ref0 ref0(Ref1); // get the cell # stored in a given cache location

Ref0 get(Ref0); // get the cell content of the cell stored in a given cache location
void set(Ref0, Ref0); // modify the cell content. Flag it as modified. Changes are logged to prepare future commit into level 0.

Cell0[] openGet(Ref0*, int*); // get the ++ part of the open block at a given memory level 0 address
Cell0[] backOpenGet(Ref0*, int*); // get the -- part of an open block


// the hash2 function is used to compute various hash
Ref1 hash2(Ref0, Ref0);

// Note that arrows handling doesn't distinguish arrows fetching and arrows creation. Fetching an arrow simply creates it at first citation.
Ref0 string(char*);
Ref0 blob(unsigned long, char*);
Ref0 pair(Ref0, Ref0);
Ref0 root(Ref0);
Ref0 unroot(Ref0);

Ref0 pair(Ref0 head, Ref0 queue) {
 Ref0 h = hash2(head, queue), r0, R ;
 Ref1 r;
 int i, oSize;
 Ref0[] o = openGet(h, &oSize);
 for (i = 0; i < oSize - 2; i++) {
   if ((o[i] & TFLAGS) == PAIR
       && (o[i] & REFPART) == head
       && (o[i+1] & REFPART) == queue) break ;
 }
 if (i < oSize - 2)
    return (h + i); // the pair already exists

 // the pair doesn't exist
 r0 = h + oSize; // put it at the ++side of the open block
 set(r0,head ^ PAIR);
 set(r0+1,queue ^ REMAIN);
 R = hash(get(locate(head+2)) & REFPART, get(locate(queue+2)) & REFPART);
 set(locate(r0+2), R ^ REMAIN);
 //add O et I at r0--/++


}

