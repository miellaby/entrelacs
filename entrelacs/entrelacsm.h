
#define Eve() xl_Eve()
#define arrow(A, B)  xl_arrow(A, B)
#define tag(S) xl_tag(S)
#define btag(N, S) xl_btag(N, S);
#define blob(N, S) xl_blob(N, S);

#define a(A, B) xl_arrow(A, B)

#define arrowMaybe(A, B)  xl_arrowMaybe(A, B)
#define tagMaybe(S) xl_tagMaybe(S)
#define btagMaybe(N, S) xl_tagMaybe(N, S)
#define blobMaybe(N, S) xl_blobMaybe(N, S)

#define typeOf(A)    xl_typeOf(A)
#define headOf(A)    xl_headOf(A)
#define tailOf(A)    xl_tailOf(A)
#define tagOf(A)     xl_tagOf(A)
#define btagOf(A, N) xl_btagOf(A, N)
#define blobOf(A, N) xl_blobOf(A, N)

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

#define childrenOf(A, CB, C) xl_childrenOf(A, CB, C)