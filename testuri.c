
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/mman.h> /* mmap() is defined in this header */

#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "mem.h" // geoalloc

#define test_title(T) fprintf(stderr, T "\n")

static struct s_buffer { int size; int max; char* buffer; } buffer = {0, 0, NULL} ;

Arrow blobFromFile(char *f) {
 Arrow a = Eve();
 int fd  = open(f, O_RDONLY); 
 if (fd < 0) return Eve();
 
 struct stat stat;
 int r = fstat (fd, &stat);
 assert (r >= 0);

 size_t size = stat.st_size;
 if (size > 0) {
  char *data = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
  assert (data != (void*) -1);
  
  a = blob(size, data);
  munmap(data, size);
 }

 close(fd);
 return a;
}

int main(int argc, char* argv[]) {
    xl_init();
    // assimilate arrows
    DEFTAG(hello); // Arrow hello = xl_tag("hello");
    DEFTAG(world);
    DEFA(hello, world); // Arrow hello_world = xl_arrow(hello, world);

    test_title("check /hello.world URI");
    {
        char *a = uriOf(hello_world);
        assert(a);
        fprintf(stderr, "%s\n", a);
        assert(uri(a) == hello_world);
        free(a);
    }

    test_title("check Blob URI");
    {
        Arrow someBlob = blobFromFile(__FILE__);
        char* a = uriOf(someBlob);
        assert(a);
        fprintf(stderr, "%s\n", a);
        assert(uri(a) == someBlob);
    }
    return 0;
}
