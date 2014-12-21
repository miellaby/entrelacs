#define REPL_C
#define _GNU_SOURCE
#include <stdio.h> // fopen & co
#include <stdlib.h> // free
#include <assert.h>
#include <string.h>
#include "linenoise.h"
#include "log.h"

#include "entrelacs/entrelacs.h"
#include "session.h"

// TODO merge config with mem0.h
#define PERSISTENCE_ENV "ENTRELACS"
#define PERSISTENCE_DIR "~/.entrelacs"
char* repl_historyPath = NULL;

// =========
// file path handling (dirty copy/paste from mem0.c)
// =========

char* repl_dirname(char* path) {
  char* d = NULL;
  if (path[0] == '~') {
      asprintf(&d, "%s%s", getenv("HOME"), path + 1);
  } else {
      d = strdup(path);
  }
  assert(d);
  uint32_t n = strlen(d);
  if (n == 1 && *d == '/') return d; // special "/" case
  char* p = strrchr(d, '/'); // find last '/' char
  if (p == d) // special case: "/something" in root
    p[1] == '\0';
  else if (p) // /so/me/thing cases
    *p = '\0';
  else {
    // "something" case: directory is "."
    if (n > 0)
        strcpy(d, ".");
    else {
        free(d);
        d = strdup(".");
        assert(d);
    }
  }
  return d;
}

char* repl_basename(char* path) {
  uint32_t n = strlen(path);
  char* b = (char *)malloc(n + 1);
  assert(b);
  char* p = strrchr(path, '/'); // find last '/' char
  if (p) // "/something" or "/some/thing" or even "/"
    strcpy(b, p + 1);
  else
    strcpy(b, path); // other case: basename is path
  return b;
}

char* repl_path(char** target, char* prePath, char* postPath) {

  char *translatedPrePath = NULL;
  if (prePath[0] == '~') {
      asprintf(&translatedPrePath, "%s%s", getenv("HOME"), prePath + 1);
      assert(translatedPrePath);
      prePath = translatedPrePath;
  }

  char *translatedPostPath = NULL;
  if (postPath[0] == '~') {
      asprintf(&translatedPostPath, "%s%s", getenv("HOME"), postPath + 1);
      assert(translatedPostPath);
      postPath = translatedPostPath;
  } else if (!strcmp(postPath, ".")) {
      postPath = "";
  }
  *target = NULL;

  int prePathLength = strlen(prePath);
  if (postPath[0] == '/') {
      asprintf(target, "%s", postPath);
  } else if (prePathLength && prePath[prePathLength - 1] == '/') {
      asprintf(target, "%s%s", prePath, postPath);
  } else {
      asprintf(target, "%s/%s", prePath, postPath);
  }
  assert(*target);
  if (translatedPrePath) free(translatedPrePath);
  if (translatedPostPath) free(translatedPostPath);
}

// =============
// history
// =============

static void repl_computeHistoryFilePath() {
    if (repl_historyPath != NULL) return;

    char* env = getenv(PERSISTENCE_ENV);
    if (env) {
       char* d = repl_dirname(env);
       char* b = repl_basename(env);
       if (strlen(b)) {
         char* bDot = NULL;
         asprintf(&bDot, "%s.history", b);
         assert(bDot);
         repl_path(&repl_historyPath, d, bDot);
         free(bDot);
       } else
         repl_path(&repl_historyPath, d, "entrelacs.history");
       free(d);
       free(b);
    } else {
       repl_path(&repl_historyPath, PERSISTENCE_DIR, "entrelacs.history");
    }
    assert(repl_historyPath);
    TRACEPRINTF("repl_historyPath is '%s'", repl_historyPath);
}

void completion(const char *buf, linenoiseCompletions *lc) {

}

int main(int argc, char **argv) {
    char *line;
    char *prgname = argv[0];
    Arrow cwa = NIL;
    
    /* Parse options, with --multiline we enable multi line editing. */
    while(argc > 1) {
        argc--;
        argv++;
        if (!strcmp(*argv,"--multiline")) {
            linenoiseSetMultiLine(1);
            printf("Multi-line mode enabled.\n");
        } else if (!strcmp(*argv,"--keycodes")) {
            linenoisePrintKeyCodes();
            exit(0);
        } else {
            fprintf(stderr, "Usage: %s [--multiline] [--keycodes]\n", prgname);
            exit(1);
        }
    }
    
    //log_init(NULL, "server,session,machine=debug");
    log_init(NULL, "session=warn");

    xl_init();

    /* Set the completion callback. This will be called every time the
     * user uses the <tab> key. */
    linenoiseSetCompletionCallback(completion);

    repl_computeHistoryFilePath();
    
    /* Load history from file. The history file is just a plain text file
     * where entries are separated by newlines. */
    linenoiseHistoryLoad(repl_historyPath); /* Load the history at startup */

    /* Now this is the main loop of the typical linenoise-based application.
     * The call to linenoise() will block as long as the user types something
     * and presses enter.
     *
     * The typed string is returned as a malloc() allocated string by
     * linenoise, so the user needs to free() it. */
    while((line = linenoise("xl> ")) != NULL) {
        /* Do something with the string. */
        xl_begin();
        if (strcmp("pwd", line) == 0) {
            if (cwa == NIL)
                fprintf(stderr, "*global context*\n");
            else
                fprintf(stderr, "%O\n", cwa);
            
        } else if (strncmp("cd ", line, 3) == 0) {
            char *arg = line + 3;
            cwa = (arg[0] == '/' || cwa == NIL
                   ? xl_uri(arg) : xl_pair(cwa, xl_uri(arg)));

        } else if (strcmp("ls", line) == 0 || strncmp("ls ", line, 3) == 0) {
            XLEnum *e;
            char *arg = line + (line[2] == '\0' ? 2 : 3);
            e = xl_childrenOf(
                *arg == '\0'
                ? cwa == NIL ? EVE : cwa
                : (*arg == '/' || cwa == NIL
                   ? xl_uri(arg)
                   : xl_pair(cwa, xl_uri(arg))));
                    
            Arrow next = e && xl_enumNext(e) ? xl_enumGet(e) : EVE;
            while (next != EVE) {
                Arrow child = next;
                next = xl_enumNext(e) ? xl_enumGet(e) : EVE;
                fprintf(stderr, "%O\n", child);
            }
        } else if (line[0] != '\0') {
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(repl_historyPath);
            
            Arrow p = xl_uri(line);
            if (p == NIL) {
                fprintf(stderr, "Illegal input. Embedded URI may be wrong.\n");
            
            } else if (p == EVE) {
                fprintf(stderr, "EVE\n");
                
            } else {
                Arrow r = xl_eval(EVE, p, EVE);
                fprintf(stderr, "%O\n", r);
            }

        }
        xl_over();
        free(line);
    }

    return 0;
}
