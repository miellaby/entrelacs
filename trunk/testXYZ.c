#include "entrelacs/entrelacs.h"
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
int main(int argc, char* argv[]) {
	xl_init();
    char output[255];
	Arrow helloWorld = xl_arrow(xl_tag("hello"), xl_tag("world"));
	char *s1 = xl_tagOf(xl_tailOf(helloWorld));
	char *s2 = xl_tagOf(xl_headOf(helloWorld));
	snprintf(output, 255, "%s %s", s1, s2);
    puts(output);
	assert(0 == strcmp(output, "hello world"));
	free(s1);
	free(s2);
    xl_root(helloWorld);
	xl_commit();
	assert(xl_isRooted(helloWorld));
	xl_unroot(helloWorld);
	assert(!xl_isRooted(helloWorld));
	
	return 0;
}