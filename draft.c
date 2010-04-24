/* This scrap file is used to think about design issues
*/

Loose state is useless for regular arrow, where loose <=> head.jump == 0
it is useless for tag arrow as well, where loose <=> ref#==0

I'm going to remove this concept.


// Je viens d'imaginer une façon excellente d'accélerer le parcours d'un cache a la recherche des modifications
// on fait comme le petit jeu des enfants avec les bateaux en papier, c'est à dire: on stocke dans 8 bits à coté de chaque cellule modifiée l'adresse modulo 255 de la cellule modifiée suivante.
// Le problème c'est de trouver la cellule modifiée qui précède celle qu'on modifie. Flute.
 

// About connectivity, garbage collector, new arrow, unrooted arrow

// A freshly defined arrow won't be connected with child arrows at first.
// It stays in a "loose state" as long as it doesn't take part of a rooted arrow definition. All in all, such an arrow got the same status as a bound-to-be-recycled non-connected arrow.
// the connection process takes place while rooting a loose arrow
// root(a) ==> if (loose(a)) { unloose(a); connect(headOf(a),a) ; connect(tailOf(a),a); }
// connect(a,child) ==>
//       - add child back-ref to h3 based a sequence.
//       - if (loose(a)) { unloose(a) ; connect(head(a),a) & connect(tail(a), a); }
// New (and initialy loose) arrows and unrooted arrows are logged into a GC FIFO
// What the GC does:
// - consumme one arrow in the log (FIFO)
// - check back-ref. If no back-ref nor root-flag.
//   - Unconnect(head(a),a) & addToGCFIFO(head(a)) 
//        & unconnect(tail(a),a) & addToGCFIFO(tail(a))
// - repeat until one goes under some thresold
//

// Prototype constraints
// Cell size: 32 bits
// Address range: 0-2^24
// Only regular arrows can be rooted
// Blob are stored as a file in a traditional system while the #BLOB->SHA arrow is stored in the space.
// Tags connectivity can't be queried. Build regular arrows out of tags to leverage on Entrelacs connectivity system.

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

