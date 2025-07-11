#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>
#include "log/log.h"

#include "entrelacs/entrelacsm.h"
#include "machine/session.h"
int main(int argc, char **argv) {
  char buffer[1024];
  //log_init(NULL, "server,session,machine,space=debug");
  log_init(NULL, "server,session,machine=debug");

  xs_init();
  while (fgets(buffer, 1024, stdin) != NULL) {
    xl_open();
//   Arrow p = xs_url(EVE, buffer);
    Arrow p = xl_uri(buffer);
    if (p == NIL) {
        fprintf(stderr, "Illegal input. Embedded URI may be wrong.\n");
    
    } else if (p == EVE) {
        fprintf(stderr, "EVE\n");
        
    } else {
        Arrow r = xs_eval(EVE, p, EVE);
        fprintf(stderr, "eval %O =\n\t%O\n", p, r);
    }
    xl_close();
  }

  return EXIT_SUCCESS;
}
