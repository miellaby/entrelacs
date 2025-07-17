#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>
#include "log/log.h"

#include "entrelacs/entrelacs.h"
#include "machine/session.h"
int main(int argc, char **argv) {
  (void) argc; (void) argv;
  char buffer[1024];
  //log_init(NULL, "server,session,machine,space=debug");
  log_init(NULL, "server,session,machine=debug");

  xs_init();
  while (fgets(buffer, 1024, stdin) != NULL) {
    xl_open(); // Arrow session = xs_open("shell");
//   Arrow p = xs_url(EVE, buffer);
    Arrow p = xs_uri(buffer);
    if (p == NULL) {
        fprintf(stderr, "Illegal input. Embedded URI may be wrong.\n");

    } else if (xs_isEve(p)) {
        fprintf(stderr, "EVE\n");

    } else {
        Arrow r = xs_eval(xs_eve(), p, xs_eve() /* session */);
        fprintf(stderr, "eval %O =\n\t%O\n", p, r);
    }
    xs_close(NULL);
  }

  return EXIT_SUCCESS;
}
