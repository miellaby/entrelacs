/* This scrap file is used to think about design issues
*/

/* API */


/* Defining */
Arrow string(char *);
Arrow blob(char *, uint32 size);
Arrow arrow(Arrow, Arrow);

/* Unbuilding */
Arrow headOf(Arrow);
Arrow tailOf(Arrow);
char* stringOf(Arrow);
char* blobOf(Arrow, long*);


/* Rooting */
Arrow root(Arrow);
void unroot(Arrow);
int rooted(Arrow); // returns 0 if rooted.

/* Transaction */
void commit(); // Arrow may be unvalidated

/* Browsing */
FuzzyEnum incomingsOf(Arrow);
FuzzyEnum outgoingsOf(Arrow);
Arrow next(*FuzzyEnum);

int (SimpleCallBack*)(Arrow arrow, char* context);
void incomingsCB(Arrow, SimpleCallBack);
void outgoingsCB(Arrow, SimpleCallBack);


// The Stack & Call By Continuation style "Entrelacs Language"
// Abstract Machine State Arrow = ( current, ( (stack program ), continuation ) )
int (CallBack*)(Arrow M);
Arrow continuation(CallBack);
Arrow program(char*);
Arrow machine(Arrow program, Arrow cc, Arrow current, Arrow stack);
int   run(Arrow M);
int   eval(char* programString, Callback cb);


// Extending the browse language with your own operators:
// ----------------------------------------
Arrow (OperatorCallBack*)(Arrow M);
Arrow operator(OperatorCallBack continuation);
// and don't forget link your operator to a term and root the link!


// exemple:
// return sumCB(Arrow M) { 
//   Arrow current = tailOf(M);
//   Arrow stack = tailOf(tailOf(headOf(M)));
//   Arrow cc = headOf(headOf(M));
//   current = string(i2s( s2i(stringOf(current)) + s2i(tailOf(stack)))));
//   stack = headOf(stack);
//   cc = headOf(m->cc);
//   return machine(program, cc, current, stack);
// }
// Arrow sum = root(arrow(string("sum"),operator(sumCB)));
//
// usage: "$sum:2,2" or "2,$sum:2" or "2,2<$sum:>"
//
//
// other exemple: 
// int myOwnBranch(Arrow m) { 
//   m->cc = tailOf(m->cc);
//   if (m->current == Eve) 
//       m->cc = tailOf(m->cc);
//   return 0;
// }


// ----------------------------------------
/* Internal */
uint16   _transactionId;
Arrow* _dirtyArrows; // arrows to submit to garbage collector (new/unrooted)
