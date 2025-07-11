/** @file mem.h
 Orthogonally persistent memory device.
 */

#ifndef MEM_H
#define MEM_H

#include "mem/mem0.h"

/**
 * last commit time
 */
extern time_t mem_lastCommitted;

/** "mem" initialization
 @return 1 if very first start, <0 if error, 0 otherwise
 */
int  mem_init();

/** opening mem
 * mem r/w op must occured between open/close
 * @return 0 on success
 */
int mem_open();

/** closing mem
 */
int mem_close();

/** set data into memory cell
 @param Cell Address
 @param Cell Content Pointer
 */
int mem_set(Address, CellBody*);

/** get data from memory cell
 @param Cell Address
 @param Cell Content Pointer
 @return 0 if OK
 */
int mem_get(Address, CellBody*);

/** get data from memory cell and its lastModifiedPoke
 @param Cell Address
 @param Cell Stamp Pointer
 @return 0 if OK
 */
int mem_get_advanced(Address, CellBody*, uint32_t* stamp_p);

/** close current micro-transaction,
 by making all memory changes from last commit persistent
 */
int mem_commit();

/** tell if a commit is needed (stale data, scare resources)
 */
int mem_commit_is_needed();

/** apply mem_commit() if needed()
 return 1 if commit occured, 0 if not, -1 on error
 */
int mem_yield();

/** get a counter incremented at each commit
 @return revision numver
 */
uint32_t mem_revision();

/** setup or reinitialize a geometrically growing RAM area
 */
void zeroalloc(char** pp, uint32_t* maxp, uint32_t* sp);

/** change content size inside a geometrically growing RAM area
 */
void geoalloc(char** pp, uint32_t* maxp, uint32_t* sp, uint32_t us, uint32_t s);


/** "mem" release
 */
int  mem_destroy();

#endif /* MEM_H */

