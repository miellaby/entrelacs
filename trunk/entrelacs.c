#include <stdio.h>
#include <stdlib.h>

typedef void* Ref0; // Memory Level 0 address
typedef void* Ref1; // Memory Level 1 address
#define PAIR (0<<15)
#define REMAIN (1<<15)
#define IN (2<<15)
#define OUT(3<<15)
#define IDX (4<<15)
#define BLOB (5<<15)
#define STRING (6<<15)
#define ROOTED (8<<15)
#defines TFLAGS (15<<15)
#define REFPART (|(15<<15))
typedef struct { Ref0 r0; int state; Ref1 r0; } Cell;
static const unsigned long memSize = 256 << 7 ;
static Cell[memSize];

// orthogonally persistent arrows space with open-adressing like memory cache
Ref1 locate(Ref0);
Ref0 ref0(Ref1);
Ref0 get(Ref1);
Ref0[] openGet(Ref0*, int*);
Ref0[] reverseOpenGet(Ref0*, int*);
void set(Ref1, Ref0);

// hash
Ref1 hash2(Ref0, Ref0);

// arrows handling
Ref1 pair(Ref0, Ref0);
Ref1 string(char*);
Ref1 blob(unsigned long, char*);
Ref1 root(Ref0);
Ref1 unroot(Ref0);


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
    return (h + i);

 r0 = h + oSize;
 set(locate(r0),head ^ PAIR);
 set(locate(r0+1),queue ^ REMAIN);
 R = hash(get(locate(head+2)) & REFPART, get(locate(queue+2)) & REFPART);
 set(locate(r0+2), R ^ REMAIN);
 //Ajouter O et I at r0--/++


}

