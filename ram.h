/** @file ram.h
 Volatile memory device.
 */

#ifndef RAM_H
#define RAM_H

#define RAMSIZE (0x100000)
// 0x10000 cells

/** Cell. A 24 bytes long bucket
  */
typedef struct s_ramCell {
    char raw[28];
} RamCell;


/** "RAM" initialization
 @return 1 if very first start, <0 if error, 0 otherwise
 */
int  ram_init();

/** set data into memory cell
 @param Cell Address
 @param Cell Content Pointer
 */
int ram_set(Address, RamCell*);

/** get data from memory cell
 @param Cell Address
 @param Cell Content Pointer
 @return 0 if OK
 */
int ram_get(Address, RamCell*);

/** "RAM" release
 */
int  ram_destroy();

#endif /* RAM_H */

