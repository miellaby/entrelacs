#include "log.h"

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
  
  a = atomn(size, data);
  munmap(data, size);
 }

 close(fd);
 return a;
}

int main(int argc, char* argv[]) {
    log_init(NULL, "server,session,machine,space=debug");
    xl_init();
    // assimilate arrows
    DEFATOM(hello); // Arrow hello = xl_tag("hello");
    DEFATOM(world);
    DEFA(hello, world); // Arrow _hello_world = xl_pair(hello, world);

    test_title("check /hello+world URI");
    {
        char *a = uriOf(_hello_world, NULL);
        assert(a);
        fprintf(stderr, "%s\n", a);
        assert(uri(a) == _hello_world);
        free(a);
    }

    test_title("check Blob URI");
    {
        Arrow someBlob = blobFromFile(__FILE__);
        char* a = uriOf(someBlob, NULL);
        assert(a);
        fprintf(stderr, "%s\n", a);
        assert(uri(a) == someBlob);
    }
    return 0;
}
