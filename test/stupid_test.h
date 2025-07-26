#pragma once
static char* test_title;
#define test_title(T) fprintf(stderr, "\n=========== TEST %s ===========\n", (test_title = T))
#define test_ok()  fprintf(stderr, "          TEST %s SUCCESS\n", test_title)
#define test_done() fprintf(stderr, "\n\n\nALL TESTS FROM " __FILE__ " DONE\n\n\n")
