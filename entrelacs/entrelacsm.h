/* File:   entrelacsm.h
 * Handy macro to entrelacs C API
 */

#define Eve()       xl_Eve()
#define pair(T, H)  xl_pair(T, H)
#define atom(S)     xl_atom(S)
#define atomn(N, S) xl_atomn(N, S)
#define uri(U)      xl_uri(U)
#define urin(U)     xl_urin(N, U)
#define anonymous() xl_anonymous()
#define hook(P)     xl_hook(P)

#define t(S)    xl_atom(S)
#define a(T, H) xl_pair(T, H)
#define DEFATOM(V) Arrow V = xl_atom(#V)
#define DEFA(T, H) Arrow _##T##_##H = a(T, H)


#define pairIfAny(T, H)   xl_pairIfAny(T, H)
#define atomIfAny(S)      xl_atomIfAny(S)
#define natomIfAny(N, S)  xl_atomnIfAny(N, S)
#define uriIfAny(U)       xl_uriIfAny(U)
#define urinIfAny(U)      xl_urinIfAny(N, U)
#define digestIfAny(D)    xl_digestIfAny(D)

#define typeOf(A)    xl_typeOf(A)
#define headOf(A)    xl_headOf(A)
#define tailOf(A)    xl_tailOf(A)
#define strOf(A)     xl_strOf(A)
#define memOf(A, N)  xl_memOf(A, N)
#define uriOf(A, N)  xl_uriOf(A, N)
#define digestOf(A, N) xl_digestOf(A, N)
#define pointerOf(A) xl_pointerOf(A)

#define type(A)      xl_typeOf(A)
#define head(A)      xl_headOf(A)
#define tail(A)      xl_tailOf(A)
#define str(A)       xl_strOf(A)
#define mem(A, N)    xl_memOf(A, N)
#define pointer(A)   xl_pointerOf(A)

#define root(A) xl_root(A)
#define unroot(A) xl_unroot(A)

#define commit() xl_commit()

#define isEve(A) xl_isEve(A)
#define isRooted(A) xl_isRooted(A)
#define isAtom(A) xl_isAtom(A)
#define isPair(A) xl_isPair(A)
#define equal(A,B) xl_equal(A, B)

#define childrenOf(A) xl_childrenOf(A)
#define childrenOfCB(A, CB, C) xl_childrenOfCB(A, CB, C)

#define operator(H, C) xl_operator(H, C)
#define continuation(H, C) xl_continuation(H, C)
#define run(R,  M) xl_run(R,  M)
#define eval(R, P) xl_eval(R, P)
