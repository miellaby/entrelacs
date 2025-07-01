#pragma once
#include <stdint.h>
#include <stddef.h> // for NULL
#include <stdlib.h> // for free

/// setup or reinitialize a geometrically growing RAM area
void zeroalloc(char** pp, uint32_t* maxp, uint32_t* sp);

/// change content size inside a geometrically growing RAM area
void geoalloc(char** pp, uint32_t* maxp, uint32_t* sp, uint32_t us, uint32_t s);
