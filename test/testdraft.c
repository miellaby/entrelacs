#include "entrelacs/entrelacsm.h"
#include "mem/mem.h" // for geoalloc
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "machine/session.h"
#include "log/log.h"

static Arrow print(Arrow arrow, Arrow context) {
  char* url = xls_urlOf(EVE, arrow, -1);
  fprintf(stderr, "0x%6x %s\n", arrow, url);
  free(url);
  return EVE;
}

int main(int argc, char* argv[]) {
    xl_init();
    log_init(NULL, "server,session,space,machine=debug");

    char output[255];
    DEFATOM(context);
    DEFATOM(hello);
    DEFATOM(world);
    xls_set(context, hello, world);
    fprintf(stderr, "1: xl_set\n");
    childrenOfCB(context, print, EVE);
    xls_unset(context, hello);
    fprintf(stderr, "2: xl_unset\n");
    xl_commit();
    childrenOfCB(context, print, EVE);
    xls_set(context, hello, world);
    fprintf(stderr, "3: xl_get\n");
    Arrow what = xls_get(context, hello);
    print(what, EVE);
    xl_commit();
    return 0;
}
