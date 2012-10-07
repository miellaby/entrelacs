#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>
#include "log.h"

#include "entrelacs/entrelacs.h"
#include "session.h"
int main(int argc, char **argv) {
  char buffer[1024];
  log_init(NULL, "server,session,machine,space=debug");

  xl_init();
  while (fgets(buffer, 1024, stdin) != NULL) {

//   Arrow p = xls_url(EVE, buffer);
    Arrow p = xl_uri(buffer);
    if (!p) continue;
    Arrow r = xl_eval(EVE, p);
    fprintf(stderr, "eval %O =\n\t%O\n", p, r);
  }

  return EXIT_SUCCESS;
}
