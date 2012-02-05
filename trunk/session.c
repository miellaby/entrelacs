#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include <uuid/uuid.h>

static Arrow session = EVE;

Arrow xls_session(Arrow agent, Arrow uuid) {
   if (!session) session = tag("session");
   Arrow s = a(session, a(agent, uuid));
   root(s);
   return s;
}

int xls_check(Arrow s, Arrow a) {
   return (tailOf(a) == s);
}

void xls_release(Arrow s, Arrow a) {
   unroot(a);
}

void xls_resetSession(Arrow s) {
   Arrow prec = EVE;
   XLEnum e = xl_childrenOf(s);
   while (xl_enumNext(e)) {
      Arrow child = xl_enumGet(e);
      unroot(prec); // only works if not Eve
      prec = child;
   }
   if (prec) unroot(prec);
   xl_freeEnum(e);
}

Arrow xls_closeSession(Arrow s) {
   xls_resetSession(s);
   unroot(s);
   return s;
}

Arrow xls_arrow(Arrow s, Arrow t, Arrow h) {
    Arrow a = a(s, a(headOf(t), headOf(h)));
    root(a);
    return a;
}
Arrow xls_tag(Arrow s, char* string) {
   Arrow t = a(s, tag(string));
   root(t);
   return t;
}

Arrow xls_btag(Arrow s, uint32_t size, char* pointer) {
   Arrow b = a(s, btag(size, pointer));
   root(b);
   return b;
}

Arrow xls_blob(Arrow s, uint32_t size, char* pointer) {
   Arrow b = a(s, blob(size, pointer));
   root(b);
   return b;
}

Arrow xls_arrowMaybe(Arrow s, Arrow t, Arrow h) {
    Arrow a = arrowMaybe(headOf(t), headOf(h));
    if (xl_isEve(a)) return xl_Eve();
    a = a(s, a);
    root(a);
    return a;
}

Arrow xls_tagMaybe(Arrow s, char* string) {
    Arrow a = tagMaybe(string);
    if (xl_isEve(a)) return xl_Eve();
    a = a(s, a);
    root(a);
    return a;
}

Arrow xls_btagMaybe(Arrow s, uint32_t size, char* pointer) {
    Arrow a = btagMaybe(size, pointer);
    if (xl_isEve(a)) return xl_Eve();
    a = a(s, a);
    root(a);
    return a;
}

Arrow xls_blobMaybe(Arrow s, uint32_t size, char* pointer) {
    Arrow a = blobMaybe(size, pointer);
    if (xl_isEve(a)) return xl_Eve();
    a = a(s, a);
    root(a);
    return a;
}

/* Unbuilding */
enum e_xlType xls_typeOf(Arrow s, Arrow a) {
    return typeOf(headOf(a));
}

Arrow xls_headOf(Arrow s, Arrow a) {
    return a(s, headOf(headOf(a)));
}

Arrow xls_tailOf(Arrow s, Arrow a) {
    return a(s, tailOf(headOf(a)));
}
    
char* xls_tagOf(Arrow s, Arrow a) {
    return tagOf(headOf(a));
}

char* xls_btagOf(Arrow s, Arrow a, uint32_t* size) {
    return btagOf(headOf(a), size);
}

char* xls_blobOf(Arrow s, Arrow a, uint32_t* size) {
    return blobOf(headOf(a), size);
}

Arrow xls_root(Arrow s, Arrow a) {
    return xl_root(headOf(a));
}

Arrow xls_unroot(Arrow s, Arrow a) {
    return xl_unroot(headOf(a));
}

void xls_commit(Arrow s) {
    return xl_commit();
}

int xls_isEve(Arrow s, Arrow a) {
    return isEve(a) || isEve(headOf(a));
}

int xls_isRooted(Arrow s, Arrow a) {
    return isRooted(headOf(a));
}

int xls_equal(Arrow s, Arrow a) {
    return (s == a) || equal(headOf(s), headOf(a));
}


XLEnum xls_childrenOf(Arrow s, Arrow a) {
    return xl_childrenOf(headOf(a));
}

void xls_childrenOfCB(Arrow s, Arrow a, XLCallBack cb, Arrow context) {
    return xl_childrenOfCB(headOf(a), cb, context);
}


Arrow xls_program(Arrow s, char* string) {
   Arrow a = a(s, program(string));
   root(a);
   return a;
}

char* xls_programOf(Arrow s, Arrow a) {
   return programOf(headOf(a));
}



Arrow xls_operator(Arrow s, XLCallBack hook, Arrow context) {
   Arrow a = a(s, xl_operator(hook, context));
   root(a);
   return a;
}

Arrow xls_continuation(Arrow s, XLCallBack hook, Arrow context) {
   Arrow a = a(s, xl_continuation(hook, context));
   root(a);
   return a;
}

Arrow xls_run(Arrow s, Arrow rootStack, Arrow M) {
   Arrow a = a(s, xl_run(headOf(rootStack), headOf(M)));
   root(a);
   return a;
}

Arrow xls_eval(Arrow s, Arrow rootStack, Arrow program) {
   Arrow a = a(s, xl_eval(headOf(rootStack), headOf(program)));
   root(a);
   return a;
}
