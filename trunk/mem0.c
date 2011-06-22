#define MEM0_C
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "mem0.h"

#ifndef PRODUCTION
  #define DEBUG(w) w
#else
  #define DEBUG(w)
#endif

static FILE* F = NULL;

char* dirname(char* file) {
  
  char* d = (char *)malloc(strlen(file) + 1);
  strcpy(d, file);
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
  if (F) return 0;

  env = getenv(PERSISTENCE_ENV);
  if (env) {
    char* d = dirname(env);
    chdir(d);
    F = fopen(env, "w+");
    free(d);
  } else {
    chdir(PERSISTENCE_DIR);
    F = fopen(PERSISTENCE_FILE, "w+");
  }
  assert(F);
 
  mem0_set(SPACE_SIZE -1, 0);
  return (ftell(F) > 0);
}

Cell mem0_get(Address r) {
   DEBUG(fprintf(stderr, "mem0_get@%012x \n", r));

   Cell result;
   fseek(F, r * sizeof(Cell), SEEK_SET);
   size_t read=fread(&result, sizeof(Cell), 1, F);
   if (!read)
   assert(read);
   return result;
}

void mem0_set(Address r, Cell v) {
   DEBUG(fprintf(stderr, "mem0_set@%012x %016llx\n", r, v));
   fseek(F, r * sizeof(Cell), SEEK_SET);
   size_t write=fwrite(&v, sizeof(Cell), 1, F);
   assert(write);
   fflush(F);
}

void mem0_saveData(char *h, size_t size, char* data) {
  // Prototype only: BLOB data are stored out of the arrows space
  if (!size) return;

  char *dir = h + strlen(h) - 2;
  chdir(PERSISTENCE_DIR);
  mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) ;
  chdir(dir);
  FILE* fd = fopen(h, "w");
  chdir("..");
  fwrite(data, size, 1, fd);  
  fclose(fd);
}

char* mem0_loadData(char* h, size_t* sizeP) {
  *sizeP = 0;

  size_t size;
  char *dir = h + strlen(h) - 2;
  chdir(PERSISTENCE_DIR);
  int rc = chdir(dir);
  if (rc) return NULL;

  FILE* fd = fopen(h, "r");
  chdir("..");
  if (!fd) return NULL;

  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  rewind(fd);
  assert(size);

  char *buffer = (char *)malloc(sizeof(char) * size);
  assert(buffer);

  rc = fread(buffer, size, 1, fd);
  fclose(fd);

  assert(rc);

  *sizeP = size;
  return buffer;
}
