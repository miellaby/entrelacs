#define MEM0_C
#define LOG_CURRENT LOG_MEM0
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "log.h"
#include "mem0.h"

static FILE* F = NULL;
int mem_is_out_of_sync = 0;
char* mem0_blobDirPath = NULL;

/** internal function */
int _mem0_set(Address address, CellBody* data);

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
  uint32_t n = strlen(d);
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
  uint32_t n = strlen(path);
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
  if (translatedPrePath) free(translatedPrePath);
  if (translatedPostPath) free(translatedPostPath);
}

// =============
// journal
// =============

char* mem0_journalFilePath = NULL;
static FILE* journalHandler = NULL;
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
    TRACEPRINTF("mem0_journalFilePath is '%s'", mem0_journalFilePath);
}

static void computeBlobDirPath() {
    if (mem0_blobDirPath != NULL) return;
    char* env = getenv(PERSISTENCE_ENV);
    if (env) {
       char* d = mem0_dirname(env);
       mem0_path(&mem0_blobDirPath, d, PERSISTENCE_BLOBDIR);
       free(d);
    } else {
       mem0_path(&mem0_blobDirPath, PERSISTENCE_DIR, PERSISTENCE_BLOBDIR);
    }
    assert(mem0_blobDirPath);
    WARNPRINTF("mem0_blobDirPath is '%s'", mem0_blobDirPath);

    mkdir(mem0_blobDirPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) ;
}

static int openJournal(int forWrite) {
    mem0_journalFilePath == NULL ? computeJournalFilePath() : (void)0;
    journalHandler = fopen(mem0_journalFilePath, forWrite == JOURNAL_WRITING ? "wb" : "rb");
    if (forWrite == JOURNAL_WRITING && !journalHandler) {
        LOGPRINTF(LOG_FATAL, "Can't open journal for writing");
    }
    return (journalHandler == NULL);
}

int mem0_initJournal() {
    return openJournal(JOURNAL_WRITING);
}

int mem0_addToJournal(Address r, CellBody *p) {
   size_t writen = fwrite(&r, sizeof(Address), 1, journalHandler);
   if (writen != 1) {
       LOGPRINTF(LOG_FATAL, "Can't write into journal");
       return -1;
   }
   writen = fwrite(p, sizeof(CellBody), 1, journalHandler);
   if (writen != 1) {
       LOGPRINTF(LOG_FATAL, "Can't write into journal");
       return -1;
   }
   return 0;
}

int mem0_terminateJournal() {
    // add terminator
    CellBody terminator;
    memset(&terminator, 0, sizeof(CellBody));
    mem0_addToJournal(0, &terminator);
    mem0_addToJournal(0, &terminator);

    if (fflush(journalHandler)
        /* || fsync(journalHandler) */
        || fclose(journalHandler)) {
        LOGPRINTF(LOG_FATAL, "Can't terminate journal");
      return -1;
    }
    journalHandler = NULL;
    return 0;
}

int mem0_dismissJournal() {
    // before removing the journal, one ensures persistence in disk
    fflush(F);
    /* fsync(F); */

    if (!mem0_journalFilePath || unlink(mem0_journalFilePath)) {
         LOGPRINTF(LOG_FATAL, "Can't remove journal");
         return -1;
    }
    return 0;
}

int mem0_openPreviousJournal() {
    char check[2 * sizeof(Address) + 2 * sizeof(CellBody)];

    if (openJournal(JOURNAL_READING))
        goto corrupted;

    TRACEPRINTF("Previous journal found");

    fseek(journalHandler, 0, SEEK_END);
    TRACEPRINTF("journal size is %ld", ftell(journalHandler));

    fseek(journalHandler, - sizeof(check), SEEK_END);
    journalEnd = ftell(journalHandler);
    TRACEPRINTF("journal terminator at %ld", journalEnd);

    size_t read = fread(&check, sizeof(check), 1, journalHandler);
    if (read != 1) {
        LOGPRINTF(LOG_WARN, "Journal last bytes reading failed. probably truncated file");
        goto corrupted;
    }
    // journal terminator is in the form: Address=0,CellBody=0,Address=0,CellBody=0
    for (int i = 0; i < sizeof(check); i++) {
        if (check[i]) { // if one byte is not 0, then it's a truncated journal
            LOGPRINTF(LOG_WARN, "Journal is not properly terminated");
            goto corrupted;
        }
    }
    valid:
        DEBUGPRINTF("Previous journal is validated");
        return 0;

    corrupted:
        if (journalHandler) {
             fclose(journalHandler);
             journalHandler = NULL;
        }
        if (mem0_journalFilePath)
            unlink(mem0_journalFilePath);
        return -1;
}

int mem0_recoverFromJournal() {
    rewind(journalHandler);
    while (1) {
        Address address;
        CellBody cell;
        size_t addressRead = fread(&address, sizeof(Address), 1, journalHandler);
        size_t cellRead = fread(&cell, sizeof(CellBody), 1, journalHandler);
        if (!(addressRead == 1 && cellRead == 1)) {
            LOGPRINTF(LOG_FATAL, "Can't read Address/Cell pair from journal");
            return -1;
        }
        if (!address) break; // Terminator found
        if (_mem0_set(address, &cell)) {
          return -1;
        }
    }
    fclose(journalHandler);
    journalHandler = NULL;
    return mem0_dismissJournal();
}

// =========
// mem0
// =========

char* mem0_filePath = NULL;
char* mem0_dirPath = NULL;

int mem0_init() {
  if (F != NULL) {
     TRACEPRINTF("mem0_init as already been done");
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
  TRACEPRINTF("mem0_filePath is %s", mem0_filePath);

  // open mem0 file (create it if non existant)
  F = fopen(mem0_filePath, "r");
  if (F) {
    fclose(F);
    F = fopen(mem0_filePath, "r+b");
  } else {
    F = fopen(mem0_filePath, "w+b");
    if (F) {
      // set it up to its max size
      CellBody lastCell;
      memset(&lastCell, 0, sizeof(CellBody));
      _mem0_set(SPACE_SIZE - 1, &lastCell);
      if (ftell(F) <= 0) {
         LOGPRINTF(LOG_FATAL, "mem0 not writable?");
      }
    }
  }

  if (!F) {
      perror("mem0 file open failed. Probably access rights or read-only target device.");
      LOGPRINTF(LOG_FATAL, "Can't open persistence file '%s'", mem0_filePath);
      return -1;
  }
  
  if (flock(fileno(F), LOCK_EX)) { // FIXME
       perror("mem0 file lock failed. Probably concurrent access. Check processus list.");
      LOGPRINTF(LOG_FATAL, "Can't lock persistence file '%s'", mem0_filePath);
      return -1;      
  }
  
  computeBlobDirPath();

  // recover from previous journal if any
  if (!mem0_openPreviousJournal()) {
      if (mem0_recoverFromJournal())
        return -1;
  }
  return 1;
}

int mem0_get(Address address, CellBody* pCellBody) {
   DEBUGPRINTF("mem0_get@%012x", address);
   assert(!journalHandler); // get can't happen where flush in progress

   if (address >= SPACE_SIZE) {
       LOGPRINTF(LOG_FATAL, "mem0_get@%012x out of range", address);
       return -1;
   }

   fseek(F, address * sizeof(CellBody), SEEK_SET);
   
   size_t read = fread(pCellBody, sizeof(CellBody), 1, F);
   if (read != 1) {
       LOGPRINTF(LOG_FATAL, "Can't read from mem0 @%012x", address);
       return -1;
   }
   
   return 0;
}

int _mem0_set(Address address, CellBody* pCellBody) {
   DEBUGPRINTF("mem0_set@%012x", address);
   
   if (address >= SPACE_SIZE) {
       LOGPRINTF(LOG_FATAL, "mem0_set@%012x out of range", address);
       return -1;
   }

   fseek(F, address * sizeof(CellBody), SEEK_SET);
   
   size_t write = fwrite(pCellBody, sizeof(CellBody), 1, F);
   if (write != 1) {
       LOGPRINTF(LOG_FATAL, "Can't write to mem0");
       return -1;
   }
   
   return 0;
}

/** mem0_set actually buffers data into journal */
int mem0_set(Address address, CellBody* pCellBody) {
    if (!journalHandler) {
        DEBUGPRINTF("First mem0_set() call since last commit. Journal begin.");
        if (mem0_initJournal())
          return -1;
    }
   
    return mem0_addToJournal(address, pCellBody);
}

void mem0_saveData(char *h, size_t size, char* data) {
  TRACEPRINTF("saving %ld bytes as '%s' hash", size, h);
  // Prototype only: BLOB data are stored out of the arrows space
  if (!size) return;

  mem0_blobDirPath == NULL ? computeBlobDirPath() : (void)0;

  char *dirname = h + strlen(h) - 2; // FIXME escape binary codes here and there
  char *filename = h; // FIXME escape binary codes here and there
  chdir(mem0_blobDirPath);
  mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) ;
  chdir(dirname);
  FILE* fd = fopen(filename, "w");
  if (!fd) {
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
  
  mem0_blobDirPath == NULL ? computeBlobDirPath() : (void)0;

  chdir(mem0_blobDirPath);
  int rc = chdir(dirname);
  if (rc) {
     LOGPRINTF(LOG_FATAL, "Can't move into '%s' directory", dirname);
  }

  FILE* fd = fopen(filename, "r");
  chdir("..");
  if (!fd) {
      LOGPRINTF(LOG_FATAL, "Can't open blob file '%s' in '%s'", filename, dirname);
      return NULL;
  }

  // retrieve file size
  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  rewind(fd);
  if (!size) {
      LOGPRINTF(LOG_FATAL, "Blob file '%s' is truncated", filename);
      fclose(fd);
      return NULL;
  }

  char *buffer = (char *)malloc(sizeof(char) * (1 + size));
  if (!buffer) {
       LOGPRINTF(LOG_FATAL, "Can't allocate buffer");
       fclose(fd);
       return NULL;
  }

  rc = fread(buffer, size, 1, fd);
  if (rc != 1) {
       LOGPRINTF(LOG_FATAL, "Can't read blob file '%s' content", filename);
       free(buffer);
       fclose(fd);
       return NULL;
  }

  rc = fclose(fd);
  if (rc) {
      LOGPRINTF(LOG_FATAL, "Can't close blob file '%s'", filename);
       free(buffer);
      return NULL;
  }

  buffer[size] = 0; // add a trailing 0 (notice one mallocated size + 1)
  *sizeP = size;
  return buffer;
}

int mem0_commit() {
    TRACEPRINTF("mem0_commit");
    if (journalHandler) {
        mem0_terminateJournal();
        int rc = mem0_openPreviousJournal();
        if (rc) {
            LOGPRINTF(LOG_FATAL, "Can't read back the journal!");
            return rc;
        }
        rc = mem0_recoverFromJournal();
        if (rc) {
            LOGPRINTF(LOG_FATAL, "Can't recover from the journal!");
            return rc;
        }
    } else {
        DEBUGPRINTF("Nothing to commit");
    }

    // save file modification time before temporarly give the lock
    struct stat st;
    time_t last_mtime = 
     (stat(mem0_filePath, &st) == 0 ? st.st_mtime : 0);
 
    flock(fileno(F), LOCK_UN); // Give a chance to pending processes
    assert(!flock(fileno(F), LOCK_EX)); // Then get back the lock
    
    // Note if file has been modified by other processes
    time_t current_mtime = 
      (stat(mem0_filePath, &st) == 0? st.st_mtime : 0);
    mem_is_out_of_sync = (last_mtime != current_mtime);
    return 0;
}

void mem0_destroy() {
    funlockfile(F);
    fclose(F);
}
