#include <stdlib.h>
#include "mem/geoalloc.h"

/** set-up or reset a tuple
 * {allocated memory, allocated size, significative  size}
 * used by geoalloc() function
 */
void zeroalloc(char **pp, uint32_t *maxp, uint32_t *sp) {
  if (*maxp) {
    free(*pp);
    *maxp = 0;
    *pp = 0;
  }
  *sp = 0;
}

/** geometricaly grow a heap allocated memory area by providing a tuple
 * {allocated memory, allocated size, current significative size} and a new
 * significative size */
void geoalloc(char **pp, uint32_t *maxp, uint32_t *sp, uint32_t us,
              uint32_t s) {
  if (!*maxp) {
    *maxp = (s < 128 ? 256 : s + 128);
    *pp = (char *)malloc(*maxp * us);
  } else {
    while (s > *maxp) {
      *maxp *= 2;
    }
    *pp = (char *)realloc(*pp, *maxp * us);
  }
  *sp = s;
  return;
}
