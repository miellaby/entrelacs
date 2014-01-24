/** @file mem.h
 Orthogonally persistent memory device.
 */

#ifndef MEM_H
#define MEM_H

#include "mem0.h"

/** "mem" initialization
 @return 1 if very first start, <0 if error, 0 otherwise
 */
int  mem_init();

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
int mem_get_advanced(Address, CellBody*, uint16_t* stamp_p);

/** close current micro-transaction,
 by making all memory changes from last commit persistent
 */
int mem_commit();

/** yield current micro-transaction, allowing disk flush
 return !0 if disk flush occurs because MEMn resources are scare
 */
int mem_yield();

/** get a counter of write operation
 @return Pokes
 */
uint32_t mem_pokes();

/** setup or reinitialize a geometrically growing RAM area
 */
void zeroalloc(char** pp, size_t* maxp, size_t* sp);

/** change content size inside a geometrically growing RAM area
 */
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s);


/** "mem" release
 */
int  mem_destroy();

#endif /* MEM_H */

