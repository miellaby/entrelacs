#ifndef MEM_H
#define MEM_H
/** @file
 Orthogonally persistent memory device.
 */
#include "mem0.h"

/** "mem" initialization
 @return 1 if very first start, <0 if error, 0 otherwise
 */
int  mem_init();

/** set binary content into memory cell
 @param cell Address
 @param Cell content
 */
void mem_set(Address, Cell);

/** get binary content from memory cell
 @param cell Address
 @return Cell content
 */
Cell mem_get(Address);

/** close current micro-transaction,
 by making all memory changes from last commit persistent
 */
void mem_commit();

/** setup or reinitialize a geometrically growing RAM area
 */
void zeroalloc(char** pp, size_t* maxp, size_t* sp);

/** change content size inside a geometrically growing RAM area
 */
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s);
#endif /* MEM_H */

