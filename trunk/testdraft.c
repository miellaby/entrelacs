#include "entrelacs/entrelacsm.h"
#include "mem.h" // for geoalloc
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "session.h"

static Arrow print(Arrow arrow, Arrow context) {
  char* url = xls_urlOf(EVE, arrow, -1);
  fprintf(stderr, "0x%6x %s\n", arrow, url);
  free(url);
  return EVE;
}

int main(int argc, char* argv[]) {
    xl_init();
    char output[255];
    DEFTAG(context);
    DEFTAG(hello);
    DEFTAG(world);
    xl_set(context, hello, world);
    fprintf(stderr, "1: xl_set\n");
    childrenOfCB(context, print, EVE);
    xl_unset(context, hello);
    fprintf(stderr, "2: xl_unset\n");
    childrenOfCB(context, print, EVE);
    xl_set(context, hello, world);
    fprintf(stderr, "3: xl_get\n");
    Arrow what = xl_get(context, hello);
    print(what, EVE);
    xl_commit();
    return 0;
}
