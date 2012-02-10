
#define Eve() xl_Eve()
#define arrow(T, H)  xl_arrow(T, H)
#define tag(S) xl_tag(S)
#define btag(N, S) xl_btag(N, S)
#define blob(N, S) xl_blob(N, S)
#define uri(U)     xl_uri(U)

#define t(S) xl_tag(S)
#define a(T, H) xl_arrow(T, H)
#define DEFTAG(V) Arrow V = xl_tag(#V)
#define DEFA(T, H) Arrow T##_##H = a(T, H)


#define arrowMaybe(T, H)  xl_arrowMaybe(T, H)
#define tagMaybe(S) xl_tagMaybe(S)
#define btagMaybe(N, S) xl_btagMaybe(N, S)
#define blobMaybe(N, S) xl_blobMaybe(N, S)
#define uriMaybe(U)     xl_uriMaybe(U)

#define typeOf(A)    xl_typeOf(A)
#define headOf(A)    xl_headOf(A)
#define tailOf(A)    xl_tailOf(A)
#define tagOf(A)     xl_tagOf(A)
#define btagOf(A, N) xl_btagOf(A, N)
#define blobOf(A, N) xl_blobOf(A, N)
#define uriOf(U)     xl_uriOf(U)

#define head(A)    xl_headOf(A)
#define tail(A)    xl_tailOf(A)
#define str(A) xl_tagOf(A)
#define nstr(A, N) xl_btagOf(A, N)

#define root(A) xl_root(A)
#define unroot(A) xl_unroot(A)

#define commit() xl_commit()

#define isEve(A) xl_isEve(A)
#define isRooted(A) xl_isRooted(A)
#define equal(A,B) xl_equal(A, B)

#define childrenOf(A) xl_childrenOf(A)
#define childrenOfCB(A, CB, C) xl_childrenOfCB(A, CB, C)

#define operator(H, C) xl_operator(H, C)
#define continuation(H, C) xl_continuation(H, C)
#define run(R,  M) xl_run(R,  M)
#define eval(R, P) xl_eval(R, P)
