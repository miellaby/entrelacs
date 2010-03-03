/* This scrap file is used to think about some design issues of a simple Entrelacs prototype
* It contains Snips of C source code for Entrelacs
*/

/* API */

define uint32 ArrowId; // ArrowId = <---TransactionId---><---MemRef--->

/* Defining */
const ArrowId Eve;
ArrowId string(char *);
ArrowId blob(char *, long);
ArrowId arrow(ArrowId, ArrowId);

/* Unbuilding */
ArrowId headOf(ArrowId);
ArrowId tailOf(ArrowId);
char*   stringOf(ArrowId);
char*   blobOf(ArrowId, long*);


/* Rooting */
ArrowId root(arrowId);
void unroot(arrowId);

/* Transaction */
void commit(); // ArrowId may be unvalidated

/* Browsing */
int (CallBack*)(ArrowId input, char *browseString, ArrowId stack, ArrowId cc);
FuzzyEnum incomingsOf(ArrowId);
FuzzyEnum outgoingsOf(ArrowId);
arrowId next(*FuzzyEnum);
void incomingsCB(ArrowId, CallBack);
void outgoingsCB(ArrowId, CallBack);
int rooted(ArrowId); // returns 0 if rooted.


// The Stack & Call By Continuation style "Browse Language" Machine
// MachineState = { currentArrow; browseString ; arrowStack ; continuationChain }
ArrowId continuation(CallBack);
int browse(char *browseString, ArrowId cc);
int browseFrom(char *browseString, ArrowId cc, ArrowId current, ArrowId stack);


// operators to set "current arrow" state variable:
// ----------------------------------------
// nnnnnnn   Number -> arrow
// #nnnn     arrowId -> arrow
// "foo"      const char* -> arrow
// xFFFF..FF blob -> arrow
// sFFFF..FF char* -> arrow
// bFFFF..FF blob& -> arrow
// (...)     closure -> arrow
// .         current continuation -> arrow
// t         current arrow tail ->arrow
// h         current arrow head -> arrow
// &         ( stack head ; current arrow ) pair -> arrow
// ~         ( stack head ; current arrow ) pair -> arrow if stored only, otherwhise Eve
// ==        current arrow set to Eve if not equal to stack head
// _         current arrow set to Eve if not rooted

// "current arrow" (un)stacking:
// ----------------------------------------
// < add current arrow to stack
// > unstack to current arrow

// continuation chain (un)stacking:
// ----------------------------------------
// : add current arrow to continuation chain (supposed to be closure but not necessary)
// / cut = unchain current continuation (won't be called)
// ; swap current continuation with first continuation in chain 

// branching
// ----------------------------------------
// ? if current arrow == eve then cut

// continuation mapping:
// ----------------------------------------
// i incomings of current arrow
// o outgoings ...

// syntaxic sugars
// ----------------------------------------
// 'foo (then white space) <==> "foo"
// $foo (then white space) <==> 'foo o_?h
// , <==> <




// Basic browse exemples:
// ----------------------------------------
// "'foo" ==> cb called with "foo" arrow
// "'foo + 'bar &" ==> cb called with 'foo->'bar arrow
// "'foo o" ==> cb called w/ each arrow outgoing from 'foo'
// "'foo oh" ==> cb called w/ the head of every known arrow outgoing from 'foo'
// "'foo i_?t" ==> cb called w/ each x verifying (x->'foo') rooted
// "'foo i_?<t<'bar ==?>>" ==> cb called w/ each rooted arrow matching ('bar'->*)->'foo' pattern
// (plan by "foo" incoming children)
// "'bar o<o<'foo'==?>>" ==> ditto, plan by "bar" outgoing children

// Branching exemples:
// ----------------------------------------
// "('bar):'foo_?42/" ==> cb w/ '42' if 'foo' rooted, 'bar' otherwise

// Continuation chaining:
// ----------------------------------------
// "(tt):'foo o_?t" ==>  cb w/ all x / (foo->(*->(*->(*->x)))) rooted
// "(t):'foo,'bar&;+42&" ==>  cb w/ ('foo'->42)


// Extending the browse language with your own operators:
// ----------------------------------------
struct MachineState { ArrowId current; char* browseString ; ArrowId stack ; ArrowId cc };
int (OperatorCallBack*)(struct MachineState* m);
ArrowId operator(OperatorCallBack continuation);

// don't forget link your operator to a term and root the link!
// exemple:
// int sumCB(MachineState* m) { 
//   m->current = string(i2s( s2i(stringOf(m->current)) + s2i( stringOf( headOf(m->stack)))));
//   m->stack = tailOf(m->stack)
//   m->cc = tailOf(m->cc)
//   return 0;
// }
// ArrowId sum = root(arrow(string("sum"),operator(sumCB)));
//
// usage: "$sum:2,2" or "2,$sum:2" or "2,2<$sum:>"
//
//
// other exemple: 
// int myOwnBranch(MachineState* m) { 
//   m->cc = tailOf(m->cc);
//   if (m->current == Eve) 
//       m->cc = tailOf(m->cc);
//   return 0;
// }


// ----------------------------------------
/* Internal */
uint16   _transactionId;
ArrowId* _dirtyArrows; // arrows to submit to garbage collector (new/unrooted)
