// ****************************************
// Memory level 0 driver
// ****************************************

#ifndef MEM0_H
#define MEM0_H

#include "entrelacs/types.h"

// initialization
void mem0_init();

// ****************************************
// hashs definition section
// ****************************************

// Using twin Prims to define a twin hash used for double-hashing when open-addressing is good for anti-clustering
// According to some googled doc (http://primes.utm.edu/lists/2small/0bit.html), the nearest twin prims lower than 8388608 (2^23) is 2^23-157 and 2^23-159

#define PRIM0 (2^23-157)
#define PRIM1 (2^23-159)
#define MEM0SIZE PRIM0

Ref0 hash(Ref0, Ref0);
Ref0 hash_2d(Ref0, Ref0);
#ifdef MEM0_C
Ref0 hash(Ref0 r1, Ref0 r2) { return (r1 + r2) % PRIM0 ; }
Ref0 double_hash(Ref0 hash) { return hash % PRIM1 ; }
#endif

// ****************************************
// Persistance file
// ****************************************

#define PERSISTANCE_DIR "~/.entrelacs"
#define PERSISTANCE_FILE "entrelacs.dat"
#define PERSISTANCE_ENV "ENTRELACS_FILE"

// ****************************************
// Mem0 cell flags
// ****************************************

// To keep only the flags part of a cell, compute a bit-AND with its content and TFLAGS
#defines TFLAGS (15<<15)
#define flags(A) ((A) & TFLAGS)

// To reset (to zero) the flags part of a cell, compute a bit-AND with its content and a bit-NOT of TFLAGS
#define REFPART (|TFLAGS)
#define noflags(A) ((A) & REFPART)

// ****************************************
// Mem0 access
// ****************************************

// init
void mem0_init();

// get
Ref0 mem0_get(Ref0); // get the cell content of the cell stored in a given cache location

// set
void mem0_set(Ref0, Ref0); // modify the cell content. Flag it as modified. Changes are logged to prepare future commit into level 0.

// get open block
Cell0[] mem0_openGet(Ref0, Ref0 offset); // get the open block at a given memory level 0 address

#endif
