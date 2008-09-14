#include <stdio.h>
#include <stdlib.h>
#include "entrelacs/types.h"
#include "mem0.h"

// Ref1<>Ref0 translation
Ref1 locate(Ref0); // get a cell and put it into the cache, returning its cache location
Ref0 ref0(Ref1); // get the cell # stored in a given cache location

Ref0 get(Ref0); // get the cell content of the cell stored in a given cache location
void set(Ref0, Ref0); // modify the cell content. Flag it as modified. Changes are logged to prepare future commit into level 0.

Cell0[] openGet(Ref0*, int*); // get the ++ part of the open block at a given memory level 0 address
Cell0[] backOpenGet(Ref0*, int*); // get the -- part of an open block

static const unsigned long mem1Size = 256 << 7 ; // mem level 1 size
static Cell1 memory1[mem1Size]; // here is the whole thing

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

