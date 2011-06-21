#include "entrelacs/entrelacs.h"
#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co

int main(int argc, char* argv[]) {
	xl_init();
	char *s1, *s2, *s3;
	Arrow helloWorld = xl_arrow(xl_tag("hello"), xl_tag("world"));
	sprintf("%s %s", s1 = xl_tagOf(xl_tailOf(helloWorld)), s2 = xl_tagOf(xl_headOf(helloWorld)));
	free(s1);
	free(s2);
	
	return 0;
}