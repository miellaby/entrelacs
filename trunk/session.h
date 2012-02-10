#include "entrelacs/entrelacs.h"

Arrow xls_session(Arrow agent, Arrow sessionUuid);
int xls_check(Arrow s, Arrow a);
void xls_release(Arrow s, Arrow a);
void xls_resetSession(Arrow s);
Arrow xls_closeSession(Arrow s);

Arrow xls_url(Arrow s, char* url);
Arrow xls_urlMaybe(Arrow s, char* url);
char* xls_urlOf(Arrow s, Arrow a, int depth);
