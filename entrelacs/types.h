// TO BE DELETED

typedef static char TransactionId; // Transaction number

typedef struct Cell0Structure { Ref0 content } Cell0;

// each reference is typed by 4 adjacent reserved bits (nammed "flags").
// 0 0 0 0 = PAIR = first part of a regular arrow definition (ie. its queue)
// 0 0 0 1 = REMAINing = subsequent part of definition (head and more for a regular arrow)
// 0 0 1 0 = INcoming = reference of an arrow whose head is an arrow defined in the ++ part of the block.
// 0 0 1 1 = OUTgoing = reference of an arrow whose queue is an arrow defined in the -- part of the block.
// 0 1 0 0 = IDX = reference of an arrow whose content-hash hits the -- part of the block
// 0 1 0 1 = BLOB = reference to a BLOB (atomistish raw data) whose content-hash hits the -- part of the block.
// 0 1 1 0 = STRING = reference to a C-String whose content-hash hits the -- part of the block.
// 0 1 1 1 = ROOTED PAIR = like a PAIR but ROOTED. It means it belongs to a top-level root context.
#define PAIR (0<<15)
#define REMAIN (1<<15)
#define IN (2<<15)
#define OUT(3<<15)
#define IDX (4<<15)
#define BLOB (5<<15)
#define STRING (6<<15)
#define ROOTED (7<<15)

typedef void* Ref1; // Memory Level 1 address
typedef struct { Ref0 ref0; Ref1 content; int state; TransactionId transactionId } Cell1; // a cell container

