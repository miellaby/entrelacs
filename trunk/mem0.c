#define MEM0_C
#define LOG_CURRENT LOG_MEM0
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "log.h"
#include "mem0.h"

static FILE* F = NULL;
void _mem0_set(Address r, Cell v);

// =========
// file path handling
// =========

char* mem0_dirname(char* path) {
  char* d = NULL;
  if (path[0] == '~') {
      asprintf(&d, "%s%s", getenv("HOME"), path + 1);
  } else {
      d = strdup(path);
  }
  assert(d);
  size_t n = strlen(d);
  if (n == 1 && *d == '/') return d; // special "/" case
  char* p = strrchr(d, '/'); // find last '/' char
  if (p == d) // special case: "/something" in root
    p[1] == '\0';
  else if (p) // /so/me/thing cases
    *p = '\0';
  else {
    // "something" case: directory is "."
    if (n > 0)
        strcpy(d, ".");
    else {
        free(d);
        d = strdup(".");
        assert(d);
    }
  }
  return d;
}

char* mem0_basename(char* path) {
  size_t n = strlen(path);
  char* b = (char *)malloc(n + 1);
  assert(b);
  char* p = strrchr(path, '/'); // find last '/' char
  if (p) // "/something" or "/some/thing" or even "/"
    strcpy(b, p + 1);
  else
    strcpy(b, path); // other case: basename is path
  return b;
}

char* mem0_path(char** target, char* prePath, char* postPath) {

  char *translatedPrePath = NULL;
  if (prePath[0] == '~') {
      asprintf(&translatedPrePath, "%s%s", getenv("HOME"), prePath + 1);
      assert(translatedPrePath);
      prePath = translatedPrePath;
  }

  char *translatedPostPath = NULL;
  if (postPath[0] == '~') {
      asprintf(&translatedPostPath, "%s%s", getenv("HOME"), postPath + 1);
      assert(translatedPostPath);
      postPath = translatedPostPath;
  } else if (!strcmp(postPath, ".")) {
      postPath = "";
  }

  *target = NULL;

  int prePathLength = strlen(prePath);
  if (postPath[0] == '/') {
      asprintf(target, "%s", postPath);
  } else if (prePathLength && prePath[prePathLength - 1] == '/') {
      asprintf(target, "%s%s", prePath, postPath);
  } else {
      asprintf(target, "%s/%s", prePath, postPath);
  }
  assert(*target);
}

// =============
// journal
// =============

char* mem0_journalFilePath = NULL;
static FILE* JOURNAL = NULL;
static long journalEnd = 0;
#define JOURNAL_READING 0
#define JOURNAL_WRITING 1

static void computeJournalFilePath() {
    if (mem0_journalFilePath != NULL) return;

    char* env = getenv(PERSISTENCE_ENV);
    if (env) {
       char* d = mem0_dirname(env);
       char* b = mem0_basename(env);
       if (strlen(b)) {
         char* bDotJournal = NULL;
         asprintf(&bDotJournal, "%s.journal", b);
         assert(bDotJournal);
         mem0_path(&mem0_journalFilePath, d, bDotJournal);
         free(bDotJournal);
       } else
         mem0_path(&mem0_journalFilePath, d, PERSISTENCE_JOURNALFILE);
       free(d);
       free(b);
    } else {
       mem0_path(&mem0_journalFilePath, PERSISTENCE_DIR, PERSISTENCE_JOURNALFILE);
    }
    assert(mem0_journalFilePath);
    DEBUGPRINTF("mem0_journalFilePath is '%s'", mem0_journalFilePath);
}

static int openJournal(int forWrite) {
    computeJournalFilePath();
    JOURNAL = fopen(mem0_journalFilePath, forWrite == JOURNAL_WRITING ? "wb" : "rb");
    if (forWrite == JOURNAL_WRITING && !JOURNAL) {
        perror("");
        LOGPRINTF(LOG_FATAL, "Can't open journal for writing");
    }
    return (JOURNAL == NULL);
}

void mem0_initJournal() {
    (void)openJournal(JOURNAL_WRITING);
}

void mem0_addToJournal(Address r, Cell v) {
   size_t writen = fwrite(&r, sizeof(Address), 1, JOURNAL);
   if (writen != 1) {
       LOGPRINTF(LOG_FATAL, "Can't write into journal");
   }
   writen = fwrite(&v, sizeof(Cell), 1, JOURNAL);
   if (writen != 1) {
       LOGPRINTF(LOG_FATAL, "Can't write into journal");
   }
}

int mem0_terminateJournal() {
    // add terminator
    mem0_addToJournal(0, 0);
    mem0_addToJournal(0, 0);

    if (fflush(JOURNAL)
        /* || fsync(JOURNAL) */
        || fclose(JOURNAL)) {
        LOGPRINTF(LOG_FATAL, "Can't terminate journal");
    }
    JOURNAL = NULL;
    return 0;
}

void mem0_dismissJournal() {
    // before removing the journal, one ensures persistence in disk
    fflush(F);
    /* fsync(F); */

    if (!mem0_journalFilePath || unlink(mem0_journalFilePath)) {
         LOGPRINTF(LOG_FATAL, "Can't remove journal");
    }
}

int mem0_openPreviousJournal() {
    char check[2 * sizeof(Address) + 2 * sizeof(Cell)];

    if (openJournal(JOURNAL_READING))
        goto corrupted;

    DEBUGPRINTF("Previous journal found");

    fseek(JOURNAL, 0, SEEK_END);
    DEBUGPRINTF("journal size is %ld", ftell(JOURNAL));

    fseek(JOURNAL, - sizeof(check), SEEK_END);
    journalEnd = ftell(JOURNAL);
    DEBUGPRINTF("journal terminator at %ld", journalEnd);

    size_t read = fread(&check, sizeof(check), 1, JOURNAL);
    if (read != 1) {
        LOGPRINTF(LOG_WARN, "Journal last bytes reading failed. probably truncated file");
        goto corrupted;
    }

    for (int i = 0; i < sizeof(check); i++) {
        if (check[i]) {
            LOGPRINTF(LOG_WARN, "Journal is not properly terminated");
            goto corrupted;
        }
    }
    valid:
        DEBUGPRINTF("Previous journal is validated");
        return 0;

    corrupted:
        if (JOURNAL) {
             fclose(JOURNAL);
             JOURNAL = NULL;
        }
        if (mem0_journalFilePath)
            unlink(mem0_journalFilePath);
        return -1;
}

void mem0_recoverFromJournal() {
    rewind(JOURNAL);
    while (1) {
        Address address;
        Cell cell;
        size_t addressRead = fread(&address, sizeof(Address), 1, JOURNAL);
        size_t cellRead = fread(&cell, sizeof(Cell), 1, JOURNAL);
        if (!(addressRead == 1 && cellRead == 1)) {
            LOGPRINTF(LOG_FATAL, "Can't read Address/Cell pair from journal");
        }
        if (!address && !cell) break; // Terminator found
        _mem0_set(address, cell);
    }
    fclose(JOURNAL);
    JOURNAL = NULL;
    mem0_dismissJournal();
}

// =========
// mem0
// =========

char* mem0_filePath = NULL;
char* mem0_dirPath = NULL;

int mem0_init() {
  if (F != NULL) {
     DEBUGPRINTF("mem0_init as already been done");
     return 0;
  }

  // compute mem0 file path
  char* env = getenv(PERSISTENCE_ENV);
  if (env) {
    char* d = mem0_dirname(env);
    char* b = mem0_basename(env);
    mem0_path(&mem0_dirPath, PERSISTENCE_DIR, d);
    if (!strlen(b)) {
      mem0_path(&mem0_filePath, mem0_dirPath, PERSISTENCE_FILE);
    } else {
      mem0_path(&mem0_filePath, mem0_dirPath, b);
    }
    free(d);
    free(b);
  } else {
    mem0_path(&mem0_dirPath, PERSISTENCE_DIR, ".");
    mem0_path(&mem0_filePath, mem0_dirPath, PERSISTENCE_FILE);
  }
  DEBUGPRINTF("mem0_filePath is %s", mem0_filePath);

  // open mem0 file (create it if non existant)
  F = fopen(mem0_filePath, "w+b");
  if (!F) {
      perror("");
      LOGPRINTF(LOG_FATAL, "Can't open persistence file '%s'", mem0_filePath);
      return -1;
  }

  // set it up to its max size
  _mem0_set(SPACE_SIZE - 1, 0);
  if (ftell(F) <= 0) {
      LOGPRINTF(LOG_FATAL, "mem0 not writable?");
  }

  // recover from previous journal if any
  if (!mem0_openPreviousJournal()) {
      mem0_recoverFromJournal();
  }
  return 1;
}

Cell mem0_get(Address r) {
   DEBUGPRINTF("mem0_get@%012x ", r);
   assert(!JOURNAL);
   if (r >= SPACE_SIZE) {
       LOGPRINTF(LOG_FATAL, "mem0_get@%012x out of range", r);
       return 0;
   }
   Cell result;
   fseek(F, r * sizeof(Cell), SEEK_SET);
   //DEBUGPRINTF("Moved to %ld", ftell(F));
   size_t read = fread(&result, sizeof(Cell), 1, F);
   if (read != 1) {
       LOGPRINTF(LOG_FATAL, "Can't read from mem0 @%012x", r);
   }
   return result;
}

void _mem0_set(Address r, Cell v) {
   DEBUGPRINTF("mem0_set@%012x %016llx", r, v);
   if (r >= SPACE_SIZE) {
       LOGPRINTF(LOG_FATAL, "mem0_set@%012x out of range", r);
       return;
   }

   fseek(F, r * sizeof(Cell), SEEK_SET);
   size_t write = fwrite(&v, sizeof(Cell), 1, F);
   if (write != 1) {
       LOGPRINTF(LOG_FATAL, "Can't write to mem0");
   }
}

void mem0_set(Address r, Cell v) {
    if (!JOURNAL) {
        DEBUGPRINTF("First mem0_set() call since last commit. Journal begin.");
        mem0_initJournal();
    }
    mem0_addToJournal(r, v);
}

void mem0_saveData(char *h, size_t size, char* data) {
  DEBUGPRINTF("saving %ld bytes as '%s' hash", size, h);
  // Prototype only: BLOB data are stored out of the arrows space
  if (!size) return;

  char *dirname = h + strlen(h) - 2; // FIXME escape binary codes here and there
  char *filename = h; // FIXME escape binary codes here and there
  chdir(PERSISTENCE_DIR);
  mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) ;
  chdir(dirname);
  FILE* fd = fopen(filename, "w");
  if (!fd) {
     perror("");
     LOGPRINTF(LOG_FATAL, "Can't open blob file '%s' in '%s'", filename, dirname);
  }
  chdir("..");
  size_t written = fwrite(data, size, 1, fd);
  if (written != 1) {
     LOGPRINTF(LOG_FATAL, "Can't write into blob file '%s'", filename);
  }
  int rc = fclose(fd);
  if (rc) {
     LOGPRINTF(LOG_FATAL, "Can't close loaded blob file '%s'", filename);
  }
}

char* mem0_loadData(char* h, size_t* sizeP) {
  *sizeP = 0;

  size_t size;
  char *filename = h;
  char *dirname = h + strlen(h) - 2;
  chdir(PERSISTENCE_DIR);
  int rc = chdir(dirname);
  if (rc) {
     LOGPRINTF(LOG_FATAL, "Can't move into '%s' directory", dirname);
  }

  FILE* fd = fopen(filename, "r");
  chdir("..");
  if (!fd) {
      perror("");
      LOGPRINTF(LOG_FATAL, "Can't open blob file '%s' in '%s'", filename, dirname);
  }

  // retrieve file size
  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  rewind(fd);
  if (!size) {
      LOGPRINTF(LOG_FATAL, "Blob file '%s' is truncated", filename);
  }

  char *buffer = (char *)malloc(sizeof(char) * size);
  assert(buffer);

  rc = fread(buffer, size, 1, fd);
  if (rc != 1) {
       LOGPRINTF(LOG_FATAL, "Can't read blob file '%s' content", filename);
  }

  rc = fclose(fd);
  if (rc) {
      LOGPRINTF(LOG_FATAL, "Can't close blob file '%s'", filename);
  }

  *sizeP = size;
  return buffer;
}

int mem0_commit() {
    if (JOURNAL) {
        mem0_terminateJournal();
        int rc = mem0_openPreviousJournal();
        if (rc) {
            LOGPRINTF(LOG_FATAL, "Can't read back the journal file!");
        }
        mem0_recoverFromJournal();
    } else {
        DEBUGPRINTF("Nothing to commit");
    }
}
