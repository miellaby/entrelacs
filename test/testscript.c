#include "log/log.h"
#include <assert.h>
#include <stdio.h>  // fopen & co
#include <stdlib.h> // free

#include "entrelacs/entrelacs.h"

char *tests[] = {"/hello+childrenOf", "//hello+world+root", "//hello+dude+root",
                "/world+childrenOf", "/hello+childrenOf", NULL};

static Arrow printCB(Arrow arrow, Arrow context) {
  (void) context;
  fprintf(stderr, "%O\n", arrow);
  return arrow;
}

int main(int argc, char **argv) {
  (void) argc; (void) argv;
  log_init(NULL, "server,session,machine,space=debug");

  xs_init();

  for (int i = 0; tests[i] != NULL; i++) {
    char *buffer = tests[i];
    Arrow a = xs_uri(buffer);
    fprintf(stderr, "%s assimilated as %O\n", buffer, a);
    Arrow command = xs_getHead(a);
    Arrow arg = xs_getTail(a);

    if (xs_equal(command, xs_const("root"))) {
      fprintf(stderr, "rooting %O ...\n", arg);
      xs_root(arg);
    } else if (xs_equal(command, xs_const("unroot"))) {
      fprintf(stderr, "unrooting %O ...\n", arg);
      xs_unroot(arg);
    } else if (xs_equal(command, xs_const("childrenOf"))) {
      fprintf(stderr, "childrenOf %O\n", arg);
      fprintf(stderr, " ==> {\n");
      xs_childrenOfCB(arg, printCB, NULL);
      fprintf(stderr, "}\n");
    } else {
      fprintf(stderr, "Unknown command: %O", command);
      assert(1);
    }

    fprintf(stderr, "commiting ...\n");
    xs_commit(NULL);
  }
  xl_close();
  return EXIT_SUCCESS;
}
