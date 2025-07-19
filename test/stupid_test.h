#pragma once
static char* test_title;
#define test_title(T) fprintf(stderr, "TEST %s BEGIN\n", (test_title = T))
#define test_ok()  fprintf(stderr, "TEST %s SUCCESS\n", test_title)
#define test_done() fprintf(stderr, "ALL TESTS FROM " __FILE__ " : OK\n")
