/** @file geaolloc.h
 Geometricaly growing allocation.
 */

#ifndef GEOALLOC_H
#define GEOALLOC_H

/** setup or reinitialize a geometrically growing RAM area
 */
void zeroalloc(char** pp, size_t* maxp, size_t* sp);

/** change content size inside a geometrically growing RAM area
 */
void geoalloc(char** pp, size_t* maxp, size_t* sp, size_t us, size_t s);


#endif /* GEOALLOC_H */

