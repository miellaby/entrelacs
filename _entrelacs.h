#include "entrelacs/entrelacs.h"
uint16   _transactionId;
Arrow*   _dirtyArrows; // arrows to submit to garbage collector (new/unrooted)


uint32 hash1();
uint32 hash2();
