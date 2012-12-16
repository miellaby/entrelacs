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

/** set binary content into memory cell
 @param Cell Address
 @param Cell Content
 */
void mem_set(Address, Cell);

/** get binary content from memory cell
 @param Cell Address
 @return Cell Content
 */
Cell mem_get(Address);

/** get binary content from memory cell
 @param Cell Address
 @return Cell Content
 */
Cell mem_get(Address);

/** get binary content from memory cell and related poke stamp
 @param Cell Address
 @param Cell Stamp Pointer
 @return Cell Content
 */
Cell mem_get_advanced(Address, uint16_t* stamp_p);

/** close current micro-transaction,
 by making all memory changes from last commit persistent
 */
void mem_commit();

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

