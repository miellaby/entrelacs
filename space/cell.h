#pragma once
#include "mem/mem0.h"
#include <stdint.h>

/*
 *   Memory device
 *
 *   -------+------+------+------+-------
 *      ... | cell | cell | cell | ...
 *   -------+------+------+------+-------
 *
 */


/**  Cell structure (24 bytes).
 *
 *   +---------------------------------------+---+---+
 *   |                  data                 | T | P |
 *   +---------------------------------------+---+---+
 *                       22                    1   1
 *
 *       P: "Pebble" count aka "More" counter (8 bits)
 *       T: Cell type ID (8 bits)
 *    Data: Data (22 bytes)
 *
 *
 */
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)     /* set alignment to 1 byte boundary */

typedef union u_cell {
  /** opaque for mem1 */
  CellBody u_body;
  
  struct u_full {

    char data[22];
    unsigned char pebble;
    unsigned char type;
  } full;

#define PEEBLE_MAX 0xFFu

/*
 * Cell content type
 */
#define CELLTYPE_EMPTY  0
#define CELLTYPE_PAIR   1
#define CELLTYPE_SMALL  2
#define CELLTYPE_TAG    3
#define CELLTYPE_BLOB   4
#define CELLTYPE_ARROWLIMIT CELLTYPE_BLOB
#define CELLTYPE_SLICE  5
#define CELLTYPE_LAST   6
#define CELLTYPE_SYNC   7
#define CELLTYPE_REATTACHMENT 7
#define CELLTYPE_CHILDREN 8

  /*
   * raw
  */
  struct s_uint {
    uint32_t data[6];
  } uint;

  /*   T = 0: empty cell.
   *   +---------------------------------------------------+
   *   |                      junk                         |
   *   +---------------------------------------------------+
   *                         22 bytes
   */
 
  /** T = 1, 2, 3, 4: arrow definition.
  *   +--------+----------------------+-----------+----------+----+----+
  *   |  hash  | full or partial def  | RWC0dWnCn |  Child0  | cr | dr |
  *   +--------+----------------------+-----------+----------+----+----+
  *       4                8                4          4        1    1
  *   where RWC0dWnCn = R|W|C0d|Wn|Cn
  *
  *   R = root flag (1 bit)
  *   W = weak flag (1 bit)
  *   C0d = child0 direction (1 bit)
  *   Wn = Weak children count (14 bits)
  *   Cn = Non-weak children count  (15 bits)
  *
  *   child0 = 1st child of this arrow
  *   cr = children revision
  *   dr = definition revision
  */
  struct s_arrow {
     uint32_t hash;
     char  def[8];
     uint32_t RWWnCn;
     uint32_t child0;
     unsigned char cr;
     unsigned char dr;
  } arrow;

/** RWWnCn flags.
 */
#define FLAGS_ROOTED           0x80000000u
#define FLAGS_WEAK             0x40000000u
#define FLAGS_C0D              0x20000000u
#define FLAGS_WEAKCHILDRENMASK 0x1FFF8000u
#define MAX_WEAKREFCOUNT       0x1FFFu
#define FLAGS_CHILDRENMASK     0X00007FFFu
#define MAX_REFCOUNT           0x7FFFu

  /* T = 1: regular pair.
  *   +--------+--------+--------+-----------+----------+----+----+
  *   |  hash  |  tail  |  head  | RWC0dWnCn |  Child0  | cr | dr |
  *   +--------+--------+--------+-----------+----------+----+----+
  *       4        4        4         4            4       1    1
  */
  struct s_pair {
     uint32_t hash;
     uint32_t tail;
     uint32_t head;
     uint32_t RWWnCn;
     uint32_t child0;
     unsigned char cr;
     unsigned char dr;
  } pair;

  /* T = 2: small.
  *   +---+-------+-------------------+-----------+----------+----+----+
  *   | s | hash3 |        data       | RWC0dWnCn |  Child0  | cr | dr |
  *   +---+-------+-------------------+-----------+----------+----+----+
  *     1    3               8      
  *   <---hash--->
  *
  *   s : small size (0 < s <= 11)
  *   hash3: (data 1st word ^ 2d word ^ 3d word) & 0xFFFFFFu
  *    ... if s > 8, one can get 3 additional bytes, by computing:(1st word ^ 2d word ^ hash) & 0xFFFFFFu
  */
  struct s_small {
    unsigned char s;
    char hash3[3];
    char data[8];
    uint32_t RWWnCn;
    uint32_t child0;
    unsigned char cr;
    unsigned char dr;
  } small;
     
  /* T = 3: tag or T = 4: blob footprint.
  *   +--------+------------+----+-----------+----------+----+----+
  *   |  hash  |   slice0   | J0 | RWC0dWnCn |  Child0  | cr | dr |
  *   +--------+------------+----+-----------+----------+----+----+
  *       4          7        1                     
  *   J = first slice jump, h-sequence multiplier (1 byte)
  */
  struct s_tagOrBlob {
    uint32_t hash;
    char slice0[7];
    unsigned char jump0;
    uint32_t RWWnCn;
    uint32_t child0;
    unsigned char cr;
    unsigned char dr;
  } tagOrBlob;

#define MAX_JUMP0 0xFFu

  /*   T = 5: intermediary binary string segment. Tag or footprint.
  *   +-----------------------------------------+---+
  *   |                   data                  | J |
  *   +-----------------------------------------+---+
  *                        21                     1
  */
  struct s_slice {
    char data[21];
    unsigned char jump;
  } slice;

#define MAX_JUMP 0xFFu

  /*   T = 6: last binary string segment.
  *   +-----------------------------------------+---+
  *   |                   data                  | s |
  *   +-----------------------------------------+---+
  *                        21                     1
  *   s = slice size
  */
  struct s_last {
    char data[21];
    unsigned char size;
  } last;

  /*   T = 7: reattachment sequence.
   *   +--------+--------+--------------+
   *   |  from  |   to   |  ...         |
   *   +--------+--------+--------------+
   *       4        4          
   */
  struct s_reattachment {
    uint32_t from;
    uint32_t to;
    char junk[14];
  } reattachment;

  /*   T = 8: children segment.
   *   +------+------+------+------+------+---+---+
   *   |  C4  |  C3  |  C2  |  C1  |  C0  | t | d |
   *   +------+------+------+------+------+---+---+
   *       4     4      4      4       4    1   1
   *   Ci : Child or list terminator
   *        (list terminator = parent address with flag)
   *   t: terminators (1 byte with 5 terminator bits).
   *   d: directions (1 byte with 5 flags: 0 incoming/1 outgoing)
   *   
   */
  struct s_children {
    uint32_t C[5];
    uint16_t directions;
  } children;

} Cell;

#pragma pack(pop)   /* restore original alignment from stack */

/// EVE
#define EVE (0)
#define EVE_HASH (0x8421A593U) 

/*
 * Size limit from where data is stored as "blob" or "tag" or "small"
 * 1-11: SMALL
 * 12-99: TAG
 * 100-..: BLOB
 */
#define TAG_MINSIZE 12
#define BLOB_MINSIZE 100

/** Memory corruption trigger */
#define MAX_CHILDCELLPROBE 32

Address cell_getTail(Address a);
Address cell_getHead(Address a);
Address cell_jumpToFirst(Cell* cell, Address address, Address offset);
Address cell_jumpToNext(Cell* cell, Address address, Address offset);
char* cell_getPayload(Address a, Cell* cellp, uint32_t* lengthP);
void cell_getSmallPayload(Cell *cell, char* buffer);
void cell_log(int logLevel, int line, char operation, Address address, Cell* cell);
uint32_t cell_getHash(Address a);
void cell_show(Address a);
void cell_showChildren(Address a);
int  cell_isLoose(Address a);

#define LOGCELL(operation, address, cell) cell_log(LOG_DEBUG, __LINE__, operation, address, cell)
/* Address shifting */
#define SHIFT_LIMIT 20
#define PROBE_LIMIT 40
#define ADDRESS_SHIFT(ADDRESS, NEW, OFFSET) \
         (NEW = (((ADDRESS) + ((OFFSET)++)) % (SPACE_SIZE)))
// FIXME ADDRESS_JUMP
#define ADDRESS_JUMP(ADDRESS, NEW, OFFSET, JUMP) \
         (NEW = (((((ADDRESS) + ((JUMP) * (OFFSET))) % (SPACE_SIZE)) + ((JUMP) * ((JUMP) - 1) / 2)) % (SPACE_SIZE)))

/* Cell testing */
#define CELL_CONTAINS_ARROW(CELL) ((CELL).full.type != CELLTYPE_EMPTY && (CELL).full.type <= CELLTYPE_ARROWLIMIT)
#define CELL_CONTAINS_ATOM(CELL) ((CELL).full.type == CELLTYPE_BLOB || (CELL).full.type <= CELLTYPE_TAG)
