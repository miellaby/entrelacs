#include <stdio.h>
#include <stdlib.h>
#include <cassert.h>

#define MEM0_C
#include "mem0.h"

static FILE* F;

void mem0_init() {
  char* env;
  if (env = getenv("ENTRELACS_FILE")
    F=fopen(env);
  else
    F=fopen(PERSISTENCE_DIR "/" PERSITENCE_FILE, "wa");
  assert(F);
}

Address mem0_get(Address r) {
   Address result;
   fseek(F, r * sizeof(Address), SEEK_SET);
   fread(&result, sizeof(Address), 1, F);
   return result;
}

void mem0_set(Address r, Address v) {
   fseek(F, r * sizeof(Address), SEEK_SET);
   fwrite(&v, sizeof(Address), 1, F);
}

