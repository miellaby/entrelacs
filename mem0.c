#include <stdio.h>
#include <stdlib.h>
#include <cassert.h>

#define MEM0_C
#include "mem0.h"

static FILE* F = NULL;

char* dirname(char* file) {
  char* d = strdup(file);
  char* p = strrchr(d, '/');
  if (p == d)
    d[1] == '\0';
  else if (p)
    *p = '\0';
  else
    strcpy(d, ".");
  return d;
}

int mem0_init() {
  char* env;
  if (F) return;

  env = getenv(PERSISTENCE_ENV);
  if (env) {
    char* d = dirname(env);
    chdir(d);
    F = fopen(env);
    free(d);
  } else {
    chdir(PERSISTENCE_DIR);
    F = fopen(PERSITENCE_FILE, "wa");
  }
  assert(F);
  fseek(F, 0, SEEK_END);
  return (ftell(F) > 0);
}

Cell mem0_get(Address r) {
   Cell result;
   fseek(F, r * sizeof(Cell), SEEK_SET);
   fread(&result, sizeof(Cell), 1, F);
   return result;
}

void mem0_set(Address r, Cell v) {
   fseek(F, r * sizeof(Cell), SEEK_SET);
   fwrite(&v, 1, sizeof(Cell), F);
   fflush(F);
}
