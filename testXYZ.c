#include "entrelacs/entrelacs.h"
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
int main(int argc, char* argv[]) {
	xl_init();
       char output[255];
	char *s1, *s2, *s3;
	Arrow helloWorld = xl_arrow(xl_tag("hello"), xl_tag("world"));
	snprintf(output, 255, "%s %s", s1 = xl_tagOf(xl_tailOf(helloWorld)), s2 = xl_tagOf(xl_headOf(helloWorld)));
       puts(output);
	assert(0 == strcmp(output, "hello world"));
	free(s1);
	free(s2);
       xl_root(helloWorld);
	xl_commit();
	return 0;
}