#define MEM0_C
#define LOG_CURRENT LOG_MEM0
#define _GNU_SOURCE
#include "mem/mem0.h"
#include "log/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE *F = NULL;
time_t mem0_lastModified = 0;
char *mem0_blobDirPath = NULL;

/** internal function */
int _mem0_set(Address address, CellBody *data);

// =========
// file path handling
// =========

static char* home() {
  static char *home = NULL;
  if (!home) {
    home = getenv("HOME");
  }
  if (!home) {
    home = "/";
  }
  return home;
}

char *mem0_dirname(char *path) {
  char *d = NULL;
  if (path[0] == '~') {
    asprintf(&d, "%s%s", home(), path + 1);
  } else {
    d = strdup(path);
  }
  assert(d);
  uint32_t n = strlen(d);
  if (n == 1 && *d == '/')
    return d;                // special "/" case
  char *p = strrchr(d, '/'); // find last '/' char
  if (p == d)                // special case: "/something" in root
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

char *mem0_basename(char *path) {
  uint32_t n = strlen(path);
  char *b = (char *)malloc(n + 1);
  assert(b);
  char *p = strrchr(path, '/'); // find last '/' char
  if (p)                        // "/something" or "/some/thing" or even "/"
    strcpy(b, p + 1);
  else
    strcpy(b, path); // other case: basename is path
  return b;
}

char *mem0_path(char **target, char *prePath, char *postPath) {
  char *translatedPrePath = NULL;
  if (prePath[0] == '~') {
    asprintf(&translatedPrePath, "%s%s", home(), prePath + 1);
    assert(translatedPrePath);
    prePath = translatedPrePath;
  }

  char *translatedPostPath = NULL;
  if (postPath[0] == '~') {
    asprintf(&translatedPostPath, "%s%s", home(), postPath + 1);
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
  if (translatedPrePath)
    free(translatedPrePath);
  if (translatedPostPath)
    free(translatedPostPath);
}

// =============
// journal
// =============

char *mem0_journalFilePath = NULL;
static FILE *journalHandler = NULL;
static long journalEnd = 0;
#define JOURNAL_READING 0
#define JOURNAL_WRITING 1

static void computeJournalFilePath() {
  if (mem0_journalFilePath != NULL)
    return;

  char *env = getenv(PERSISTENCE_ENV);
  if (env) {
    char *d = mem0_dirname(env);
    char *b = mem0_basename(env);
    if (strlen(b)) {
      char *bDotJournal = NULL;
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
  if (mem0_blobDirPath != NULL)
    return;
  char *env = getenv(PERSISTENCE_ENV);
  if (env) {
    char *d = mem0_dirname(env);
    mem0_path(&mem0_blobDirPath, d, PERSISTENCE_BLOBDIR);
    free(d);
  } else {
    mem0_path(&mem0_blobDirPath, PERSISTENCE_DIR, PERSISTENCE_BLOBDIR);
  }
  assert(mem0_blobDirPath);
  INFOPRINTF("mem0_blobDirPath is '%s'", mem0_blobDirPath);

  mkdir(mem0_blobDirPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

static int openJournal(int forWrite) {
  mem0_journalFilePath == NULL ? computeJournalFilePath() : (void)0;
  journalHandler =
      fopen(mem0_journalFilePath, forWrite == JOURNAL_WRITING ? "wb" : "rb");
  if (forWrite == JOURNAL_WRITING && !journalHandler) {
    perror("openJournal");
    LOGPRINTF(LOG_FATAL, "Can't open journal for writing");
  }
  return (journalHandler == NULL);
}

int mem0_initJournal() { return openJournal(JOURNAL_WRITING); }

int mem0_addToJournal(Address r, CellBody *p) {
  size_t writen = fwrite(&r, sizeof(Address), 1, journalHandler);
  if (writen != 1) {
    perror("mem0_addToJournal fwrite address");
    LOGPRINTF(LOG_FATAL, "Can't write into journal");
    return -1;
  }
  writen = fwrite(p, sizeof(CellBody), 1, journalHandler);
  if (writen != 1) {
    perror("mem0_addToJournal fwrite body");
    LOGPRINTF(LOG_FATAL, "Can't write into journal");
    return -1;
  }
  return 0;
}

int mem0_finalizeJournal() {
  // add terminator
  CellBody terminator;
  memset(&terminator, 0, sizeof(CellBody));
  mem0_addToJournal(0, &terminator);
  mem0_addToJournal(0, &terminator);

  if (fflush(journalHandler)) {
    perror("mem0_finalizeJournal fflush");
  }
  // fsync(journalHandler);
  if (fclose(journalHandler)) {
    perror("mem0_finalizeJournal fclose");
    return -1;
  }
  journalHandler = NULL;
  return 0;
}

int mem0_dismissJournal() {
  if (!mem0_journalFilePath) {
    LOGPRINTF(LOG_FATAL, "No journal to dismiss");
    return -1;
  }
  if (unlink(mem0_journalFilePath)) {
    perror("mem0_dismissJournal unlink");
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

  fseek(journalHandler, -sizeof(check), SEEK_END);
  journalEnd = ftell(journalHandler);
  TRACEPRINTF("journal terminator at %ld", journalEnd);

  size_t read = fread(&check, sizeof(check), 1, journalHandler);
  if (read != 1) {
    if (ferror(journalHandler)) {
      perror("mem0_openPreviousJournal");
    }
    LOGPRINTF(LOG_WARN,
              "Journal last bytes reading failed. probably truncated file");
    goto corrupted;
  }
  // journal terminator is in the form:
  // Address=0,CellBody=0,Address=0,CellBody=0
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

int mem0_open() {
  if (F) {
    LOGPRINTF(LOG_DEBUG, "persistence file is already opened.");
    return 0;
  }

  F = fopen(mem0_filePath, "r+b");
  if (!F) {
    perror("mem0_open fopen");
    LOGPRINTF(LOG_FATAL, "Can't open persistence file '%s'", mem0_filePath);
    return -1;
  }

  if (flock(fileno(F), LOCK_EX)) {
    perror("mem0_open flock (probable concurrent access)");
    LOGPRINTF(LOG_FATAL, "Can't lock persistence file '%s'", mem0_filePath);
    return -1;
  }

  // read file modification time
  struct stat st;
  mem0_lastModified = (stat(mem0_filePath, &st) == 0 ? st.st_mtime : 0);

  return 0;
}

int mem0_isOpened() { return (F ? 1 : 0); }

int mem0_close() {
  if (!F) {
    LOGPRINTF(LOG_DEBUG, "mem0 file is already closed.");
    return 0;
  }

  if (flock(fileno(F), LOCK_UN)) {
    perror("mem0_close flock LOCK_UN");
  }
  if (fclose(F)) {
    perror("mem0_close");
    LOGPRINTF(LOG_FATAL, "Can't close mem0 file.");
    return -1;
  }
  F = NULL;
  return 0;
}

int mem0_recoverFromJournal() {
  rewind(journalHandler);
  while (1) {
    Address address;
    CellBody cell;
    size_t addressRead = fread(&address, sizeof(Address), 1, journalHandler);
    if (addressRead != 1) {
      if (ferror) {
        perror("mem0_recoverFromJournal fread address");
      }
      LOGPRINTF(LOG_FATAL, "Can't read Address from journal");
      return -1;
    }
    size_t cellRead = fread(&cell, sizeof(CellBody), 1, journalHandler);
    if (cellRead != 1) {
      perror("mem0_recoverFromJournal fread body");
      LOGPRINTF(LOG_FATAL, "Can't read Address/Cell pair from journal");
      return -1;
    }
    if (!address)
      break; // Terminator found
    if (_mem0_set(address, &cell)) {
      return -1;
    }
  }
  if (fclose(journalHandler)) {
    perror("mem0_recoverFromJournal fclose");
  }
  journalHandler = NULL;
  return mem0_dismissJournal();
}

// =========
// mem0
// =========

char *mem0_filePath = NULL;
char *mem0_dirPath = NULL;

int mem0_init() {
  if (mem0_filePath != NULL) {
    TRACEPRINTF("mem0_init as already been done");
    return 0;
  }

  // compute mem0 file path
  char *env = getenv(PERSISTENCE_ENV);
  if (env) {
    char *d = mem0_dirname(env);
    char *b = mem0_basename(env);
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

  // create mem0 file if non existant
  FILE *fd = fopen(mem0_filePath, "r");
  if (!fd) {
    fd = fopen(mem0_filePath, "w+b");
    if (!fd) {
      perror("mem0_init persistence file opening/creation failed");
      LOGPRINTF(LOG_FATAL, "Can't open persistence file '%s'", mem0_filePath);
      return -1;
    }

    // set it up to its max size
    CellBody lastCell;
    memset(&lastCell, 0, sizeof(CellBody));
    mem0_open();
    _mem0_set(SPACE_SIZE - 1, &lastCell);
    mem0_close();
  }

  if (fseek(fd, 0, SEEK_END)) {
    perror("mem0_init fseek");
  }
  long size = ftell(fd);
  if (size == -1) {
    perror("mem0_init ftell");
  }
  if (size != sizeof(CellBody) * SPACE_SIZE) {
    LOGPRINTF(LOG_FATAL, "mem0 wrong size. storage full?");
    return -1;
  }
  if (fclose(fd)) {
    perror("mem0_init fclose");
  }

  computeBlobDirPath();

  // recover from previous journal if any
  mem0_open();
  if (!mem0_openPreviousJournal()) {
    if (mem0_recoverFromJournal())
      return -1;
  }
  mem0_close();

  return 1;
}

int mem0_get(Address address, CellBody *pCellBody) {
  DEBUGPRINTF("mem0_get@%012x", address);
  assert(F);                    // mem0 open
  assert(!journalHandler);      // get when flushing
  assert(address < SPACE_SIZE); // range

  if (fseek(F, address * sizeof(CellBody), SEEK_SET)) {
    perror("mem0_get fseek");
  }

  size_t read = fread(pCellBody, sizeof(CellBody), 1, F);
  if (read != 1) {
    if (ferror(F)) {
      perror("mem0_get fread");
    }
    LOGPRINTF(LOG_FATAL, "Can't read from mem0 @%012x", address);
    return -1;
  }

  return 0;
}

int _mem0_set(Address address, CellBody *pCellBody) {
  DEBUGPRINTF("mem0_set@%012x", address);
  assert(F);                    // mem0 open
  assert(address < SPACE_SIZE); // range

  if (fseek(F, address * sizeof(CellBody), SEEK_SET)) {
    perror("_mem0_set fseek");
  }

  size_t write = fwrite(pCellBody, sizeof(CellBody), 1, F);
  if (write != 1) {
    perror("_mem0_set fwrite");
    LOGPRINTF(LOG_FATAL, "Can't write to mem0");
    return -1;
  }

  return 0;
}

/** mem0_set actually buffers data into journal */
int mem0_set(Address address, CellBody *pCellBody) {
  assert(F); // mem0 open

  if (!journalHandler) {
    DEBUGPRINTF("First mem0_set() call since last commit. Journal begin.");
    if (mem0_initJournal())
      return -1;
  }

  return mem0_addToJournal(address, pCellBody);
}

void mem0_saveData(char *h, size_t size, char *data) {
  TRACEPRINTF("saving %ld bytes as '%s' hash", size, h);
  // Prototype only: BLOB data are stored out of the arrows space
  if (!size)
    return;

  mem0_blobDirPath == NULL ? computeBlobDirPath() : (void)0;

  char *dirname = h + strlen(h) - 2; // FIXME escape binary codes here and there
  char *filename = h;                // FIXME escape binary codes here and there
  chdir(mem0_blobDirPath);
  mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  chdir(dirname);
  FILE *fd = fopen(filename, "w");
  if (!fd) {
    perror("mem0_saveData fopen");
    LOGPRINTF(LOG_FATAL, "Can't open blob file '%s' in '%s'", filename,
              dirname);
  }
  chdir("..");
  size_t written = fwrite(data, size, 1, fd);
  if (written != 1) {
    perror("mem0_saveData fwrite");
    LOGPRINTF(LOG_FATAL, "Can't write into blob file '%s'", filename);
  }
  if (fclose(fd)) {
    perror("mem0_saveData fclose");
    LOGPRINTF(LOG_FATAL, "Can't close loaded blob file '%s'", filename);
  }
}

void mem0_deleteData(char *h) {
  char *filename = h;
  char *dirname = h + strlen(h) - 2; // the 2 last chars

  mem0_blobDirPath == NULL ? computeBlobDirPath() : (void)0;
  chdir(mem0_blobDirPath);
  if (chdir(dirname)) {
    perror("mem0_deleteData chdir");
    LOGPRINTF(LOG_FATAL, "Can't move into '%s' directory", dirname);
    return;
  }

  if (unlink(filename)) {
    perror("mem0_deleteData unlink");
    LOGPRINTF(LOG_FATAL, "Can't delete blob file '%s' in '%s'", filename,
              dirname);
  }
  if (chdir("..")) {
    perror("mem0_deleteData chdir ..");
  }
}

char *mem0_loadData(char *h, size_t *sizeP) {
  *sizeP = 0;

  size_t size;
  char *filename = h;
  char *dirname = h + strlen(h) - 2;

  mem0_blobDirPath == NULL ? computeBlobDirPath() : (void)0;

  chdir(mem0_blobDirPath);
  if (chdir(dirname)) {
    perror("mem0_loadData chdir");
    LOGPRINTF(LOG_FATAL, "Can't move into '%s' directory", dirname);
    return NULL;
  }

  FILE *fd = fopen(filename, "r");
  if (!fd) {
    perror("mem0_loadData fopen");
    LOGPRINTF(LOG_FATAL, "Can't open blob file '%s' in '%s'", filename,
              dirname);
    return NULL;
  }
  if (chdir("..")) {
    perror("mem0_loadData chdir ..");
  }

  // retrieve file size
  if (fseek(fd, 0, SEEK_END)) {
    perror("mem0_loadData fseek");
  }

  size = ftell(fd);
  if (size == -1) {
    perror("mem0_loadData ftell");
  }
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

  if (fread(buffer, size, 1, fd) != 1) {
    if (ferror(fd)) {
      perror("mem0_loadData fread");
    }
    LOGPRINTF(LOG_FATAL, "Can't read blob file '%s' content", filename);
    free(buffer);
    fclose(fd);
    return NULL;
  }

  if (fclose(fd)) {
    perror("mem0_loadData fclose");
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
    mem0_finalizeJournal();
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
    TRACEPRINTF("Nothing to commit");
  }

  // save file modification time
  struct stat st;
  mem0_lastModified = (stat(mem0_filePath, &st) == 0 ? st.st_mtime : 0);

  return 0;
}

void mem0_destroy() {
  if (F)
    mem0_close();
}
