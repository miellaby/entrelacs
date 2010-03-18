/* The really simple Entrelacs API */

/* Arrow */
typedef unsigned int uint32;
typedef uint32 Arrow; // Arrow = <---TransactionId---><---MemoryRef--->

/* Assimilation */
Arrow Eve();
Arrow string(char *);
Arrow blob(uint32 size, char *);
Arrow arrow(Arrow, Arrow);

/* Unbuilding */
Arrow headOf(Arrow);
Arrow tailOf(Arrow);
char* stringOf(Arrow);
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

