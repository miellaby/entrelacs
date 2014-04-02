/** @file ram.h
 Volatile memory device.
 */

#ifndef RAM_H
#define RAM_H

#include <stdint.h>

#define RAMSIZE (0x100000)
// 0x10000 cells

/** RAM Cell. A 32 bytes long bucket
  */
typedef struct s_ramCell {
    char raw[32];
} RamCell;

/** Cell address.
*/
typedef uint32_t RamAddress;


/** "RAM" initialization
 @return 1 if very first start, <0 if error, 0 otherwise
 */
int  ram_init();

/** set data into memory cell
 @param Address
 @param Cell Content Pointer
 */
int ram_set(RamAddress, RamCell*);

/** get data from memory cell
 @param Address
 @param Cell Content Pointer
 @return 0 if OK
 */
int ram_get(RamAddress, RamCell*);

/** "RAM" reset
*/
int ram_reset();

/** "RAM" release
 */
int  ram_destroy();

#endif /* RAM_H */

