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
    F=fopen(PERSISTANCE_DIR "/" PERSITANCE_FILE, "wa");
  assert(F);


}

Ref0 mem0_get(Ref0 r) {
   Ref0 result;
   fseek(F, r*sizeof(Ref0), SEEK_SET);
   fread(&result, sizeof(Ref0), 1, F);
   return result;
}

void mem0_set(Ref0 r, Ref0 v) {
   fseek(F, r*sizeof(Ref0), SEEK_SET);
   fwrite(&v, sizeof(Ref0), 1, F);
}

Cell0 mem0_openGet(Ref0 r, Ref0 offset) {
   unsigned i = 0, max = 8;
   Ref0* result = malloc(8 * sizeof(Ref0));
   Ref0 v = mem0_get(r);
   assert(result);
   result[i++] = v;
   while (flags(v)) {
      if (i == max) {
         max *= 2;
         result = realloc(result, max * sizeof(Ref0));
         assert(result);
      }
      r+= offset;
      v = mem0_get(r);
      result[i++] = v;
   }
   result[i-1] = 0 ;
   return result;
}

