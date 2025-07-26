#pragma once
#include "mem/mem0.h"
#include "space/space.h"
#include "log/log.h"
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
    *   |  hash  | full or partial def  | R.C0d.Cn  |  Child0  | cr | dr |
    *   +--------+----------------------+-----------+----------+----+----+
    *       4                8                4          4        1    1
    *   R = root flag (1 bit)
    *   C0d = child0 direction (1 bit)
    *   Cn = child count  (30 bits)
    *
    *   child0 = 1st child of this arrow
    *   cr = children revision
    *   dr = definition revision
    */
    struct s_arrow {
        uint32_t hash;
        char  def[8];
        uint32_t RC0dCn;
        uint32_t child0;
        unsigned char cr;
        unsigned char dr;
    } arrow;

    /** RC0dCn flags.
    */
    #define FLAGS_ROOTED           0x80000000u
    #define FLAGS_C0D              0x40000000u
    #define FLAGS_CHILDRENMASK     0X3FFFFFFFu
    #define MAX_REFCOUNT           0X3FFFFFFFu

    /* T = 1: regular pair.
    *   +--------+--------+--------+-----------+----------+----+----+
    *   |  hash  |  tail  |  head  | R.C0d.Cn  |  Child0  | cr | dr |
    *   +--------+--------+--------+-----------+----------+----+----+
    *       4        4        4         4            4       1    1
    */
    struct s_pair {
        uint32_t hash;
        uint32_t tail;
        uint32_t head;
        uint32_t RC0dCn;
        uint32_t child0;
        unsigned char cr;
        unsigned char dr;
    } pair;

    /* T = 2: small.
    *   +---+-------+-------------------+-----------+----------+----+----+
    *   | s | hash3 |        data       | R.C0d.Cn  |  Child0  | cr | dr |
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
        uint32_t RC0dCn;
        uint32_t child0;
        unsigned char cr;
        unsigned char dr;
    } small;

    /* T = 3: tag or T = 4: blob footprint.
    *   +--------+------------+----+-----------+----------+----+----+
    *   |  hash  |   slice0   | J0 | R.C0d.Cn  |  Child0  | cr | dr |
    *   +--------+------------+----+-----------+----------+----+----+
    *       4          7        1
    *   J = first slice jump, h-sequence multiplier (1 byte)
    */
    struct s_tagOrBlob {
        uint32_t hash;
        char slice0[7];
        unsigned char jump0;
        uint32_t RC0dCn;
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

/// @brief read a cell from an address
void cell_read(Cell* cell, Address address);

/// @brief write a cell to an address
void cell_write(Cell* cell, Address address);

/// check if cell contains pair/atom arrow
int  cell_isArrow(Cell* cell);

/// check if cell contains pair arrow
int  cell_isPair(Cell* cell);

/// check if cell contains atom arrow
int  cell_isAtom(Cell* cell);

/// check if cell contains blog/tag arrow
int  cell_isString(Cell* cell);

/// check if cell contains rooted arrow
int  cell_isRooted(Cell* cell);

/// check if cell contains a loose arrow
int  cell_isLoose(Cell *cell);

/// get tail of pair cell
Address cell_getTail(Cell* cell);

/// get head of pair cell
Address cell_getHead(Cell* cell);

/// get child count of arrow cell
uint32_t cell_getChildCount(Cell* cell);

Address cell_jumpToFirst(Cell* cell, Address address, Address offset);

Address cell_jumpToNext(Cell* cell, Address address, Address offset);

/// @brief return the payload of an arrow (blob/tag/small), NULL on error
/// @param cellp pointer to the cell structure
/// @param a address of the cell
/// @param lengthP pointer to store the length of the payload
uint8_t* cell_getPayload(Cell* cellp, Address a, uint32_t* lengthP);

void cell_getSmallPayload(Cell *cell, uint8_t* buffer);
void cell_log(int logLevel, char* file, int line, char operation, Address address, Cell* cell);
void cell_show(Address a);
void cell_showChildren(Address a);

#define CELL_LOG(operation, address, cell) cell_log(LOG_DEBUG, __FILE__, __LINE__, operation, address, cell)

#define CELL_READ(cell, address) \
(cell_read(cell, address), ONDEBUG(CELL_LOG('R', address, cell)))

#define CELL_WRITE(cell, address) \
(cell_write(cell, address), ONDEBUG(CELL_LOG('W', address, cell)))

/* Address shifting */
#define SHIFT_LIMIT 20
#define PROBE_LIMIT 40
#define ADDRESS_SHIFT(ADDRESS, NEW, OFFSET) \
    (NEW = (((ADDRESS) + ((OFFSET)++)) % (SPACE_SIZE)))

// S=jump*(2*offset+jump-1)/2
#define ADDRESS_JUMP(ADDRESS, NEW, OFFSET, JUMP) \
    (NEW = ((ADDRESS) + (((JUMP) * (2 * (OFFSET) + (JUMP) - 1))) / 2) % (SPACE_SIZE))
