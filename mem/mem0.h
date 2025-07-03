/** @file mem0.h
 Persistent and slightly journalized memory device.
 - initialize mem0 with mem0_init
   Uncomplete journal is removed
   Terminated journal is recovered (copied into mem0 then deleted)
 - open mem0 with mem0_open
 - read data from cells with mem0_get calls
 - write data into a serialized journal file with mem0_set calls
   mem0_set calls
   /!\ once a write operation is performed, one can't read data til the next commit
 - commit the journal with mem0_commit
      * the journal file is terminated (terminator),
      * then data is read back and copied into the persistence file
      * then the journal is deleted
 - mem0_save/load/deleteData handles blob
 */

#ifndef MEM0_H
#define MEM0_H

#include <stdint.h>
#include <sys/types.h>

/**
 * persisence file last modification time
 * updated after commit() or open()
 */
extern time_t mem0_lastModified;

/**
 * Cell. A 24 bytes long bucket
 */
typedef struct s_cellBody {
    char raw[24];
} CellBody;

/**
 * Cell address.
 */
typedef uint32_t Address;

/**
 * mem0 initialization.
 * @return 1 if very first start, <0 if error, 0 otherwise
 */
int mem0_init();

/**
 * open mem0
 * return 0 if OK
 */
int mem0_open();

/**
 * is mem0 openened?
 * return 1 if opened
 */
int mem0_isOpened();

/**
 * close mem0
 * return 0 if OK
 */
int mem0_close();

/**
 * mem0 release.
 */
void mem0_destroy();

///
/// A prim number used to limit arrow space size.
/// Twin Prims are said to prevent data clustering when performing open-addressing
/// The nearest twin prims lower than 2^23 (8388608) is 2^23-157 and 2^23-159
///     thanks to http://primes.utm.edu/lists/2small/0bit.html
/// Note: one obtains a 8MegaCell level-0 memory bank.
///       it should allow to store few billions of arrows
///
#define _PRIM0 ((1 << 23) - 157)


extern const Address PRIM0; ///< Biggest of twin prim numbers.
extern const Address PRIM1; ///< Smallest of twin prim numbers, that is PRIM0 - 2.
extern const Address SPACE_SIZE; ///< Space total size. It is basically PRIM0.
#ifdef MEM0_C
const Address PRIM0 = _PRIM0;
const Address PRIM1 = (_PRIM0 - 2);
const Address SPACE_SIZE = _PRIM0;
#endif


/** Persistence file path environment variable.
  Value may point either (a) an existing directory or (b) a possibly non-existent file path.
  In case of (a), the persistence file is located at $PERSISTENCE_ENV/$PERSISTENCE_FILE
*/
#define PERSISTENCE_ENV "ENTRELACS"
/** Persistence file path or name.
  */
#define PERSISTENCE_FILE "entrelacs.dat"

/** Persistence file directory path.
  */
#define PERSISTENCE_DIR "~/.entrelacs"

/** Persistence journal file name.
  */
#define PERSISTENCE_JOURNALFILE "entrelacs.journal"

/** Persistence blob storage dir. May be relative to PERSISTENCE_DIR
 */
#define PERSISTENCE_BLOBDIR "blob"

/** Actual value of the persitence directory.
*/
extern char* mem0_dirPath;

/** Actual value of the persitence file path.
*/
extern char* mem0_filePath;

/** Actual value of the journal file path.
*/
extern char* mem0_journalFilePath;

/** Actual value of the blob directory path.
 */
extern char* mem0_blobDirPath;

/** set by mem0_commit to notify mem cache it's out of sync with disk (concurrent process)
*/
extern int mem_is_out_of_sync;

/** set binary content into memory cell.
 Change is actually buffered into a journal file.
 The journal file is created if not existent.
  @param cell Address.
  @param Cell content pointer.
  @return 0 if success.
  */
int mem0_set(Address, CellBody*);

/** get binary content from memory cell.
 This call is forbidden if uncommited changes are pending in an existing journal file.
  @param cell Address.
  @param Cell content pointer.
  @return 0 if success.
  */
int mem0_get(Address, CellBody*);

/** commit changes.
  - read back the journal file content,
  - copy content into the persistence file,
  - remove the journal.
  nothing is done if no current journal.
  Persistence file lock is temporarily released so check mem_is_out_of_sync!
  @return 0 if success.
*/
int  mem0_commit();

// those are prototype only methods

/** create and fill up a data file inside persistence directory.
 @param unique cryptographic hash code.
 @param data size.
 @param pointer to binary data.
 */
void   mem0_saveData(char *h, size_t size, uint8_t * data);

/** read back a data file created by mem0_loadData.
 @param h unique cryptographic hash code.
 @param sizeP size_t pointer. Pointed value set to data size.
 @return pointer to heap allocated binary data (must be freed by caller).
 */
uint8_t*  mem0_loadData(char* h, size_t* sizeP);

/** remove a data file created by mem0_loadData.
 @param unique cryptographic hash code.
 */
void   mem0_deleteData(char* h);

#endif
