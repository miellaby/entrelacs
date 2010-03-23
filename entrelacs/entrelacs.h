/* The really simple Entrelacs API */

/* Arrow */
typedef unsigned int uint32;
typedef uint32 Arrow; // Arrow = <---TransactionId---><---MemoryRef--->

/* Assimilation */
Arrow Eve();
Arrow arrow(Arrow, Arrow);
Arrow tag(char *);
Arrow blob(uint32 size, char *);

/* Locating */
Arrow locateArrow(Arrow, Arrow);
Arrow locateTag(char *);
Arrow locateBlob(uint32 size, char *);

/* Unbuilding */
enum e_type { UNDEF=-1, TYPE_EVE=0, TYPE_ARROW=1, TYPE_TAG=2, TYPE_BLOB=3 } typeOf(Arrow);
Arrow headOf(Arrow);
Arrow tailOf(Arrow);
char* tagOf(Arrow);
char* blobOf(Arrow, long*);

/* Rooting */
Arrow root(Arrow);
void unroot(Arrow);

/* Transaction */
void commit(); // Arrow may be unvalidated

/* Testing */
int isEve(Arrow); // returns 0 if equals Eve
int isRooted(Arrow); // returns 0 if rooted.
int equal(Arrow, Arrow); // returns 0 if arrows are equal

/* Browsing */
typedef struct s_Enum* Enum;
Enum incomingsOf(Arrow);
Enum outgoingsOf(Arrow);
int  next(Enum*, Arrow*);

typedef int (SimpleCallBack*)(Arrow arrow, char* context);
void incomingsCB(Arrow, SimpleCallBack);
void outgoingsCB(Arrow, SimpleCallBack);

/* Special arrows for the EL */
Arrow program(char*); // EL string
typedef int (CallBack*)(Arrow);
Arrow operator(CallBack hook); // operator arrow with hook
Arrow continuation(CallBack hook); // continuation arrow with hook

/* The Entrelacs Machine */
Arrow machine(Arrow program, Arrow cc, Arrow current, Arrow stack);
int   run(Arrow M);
int   eval(char* programString, Callback cb);

