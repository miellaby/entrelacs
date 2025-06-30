
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/mman.h> /* mmap() is defined in this header */

#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "log/log.h"

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "mem/geoalloc.h" 

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
    xl_begin();
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

    test_title("check generating and assimilating Blob URI");
    {
        Arrow someBlob = blobFromFile("web-terminal/help.wiki");
        char* u = uriOf(someBlob, NULL);
        assert(u);
        fprintf(stderr, "%s\n", u);
        assert(uri(u) == someBlob);
        char* s = xl_strOf(someBlob);
        char forgedUri[100];
        sprintf(forgedUri, "/%s+%s", u, u);
        assert(headOf(uri(forgedUri)) == someBlob);
        assert(s);
        // fprintf(stderr, "%s\n", s);
        free(s);
        free(u);
    }
    
    test_title("check assimilating complex URI");
    {
        Arrow a = uri("/partnersOf/escape+//Content-Typed+/text%2Fx-creole+%3D%3D%20Browse%20content%0A%0AUse%20%2F%2F%3F%2F%2F%20button%20to%20display%20children.%0ARepeat%20to%20%2F%2Fgo%20meta%2F%2F.%0A%0AEnter%20a%20seed%20arrow%20to%20get%20back%20connected%20information.%20Try%20it%20with%20%2F%2F42%2F%2F.%0A%0AUnwanted%20arrows%20%20may%20be%20hidden%20with%20%27%27x%27%27%20button.%0A%0A%3D%3D%20Add%20content%0A%0AClick%20background%20to%20open%20new%20prompts.%0A%0AUse%20the%20%2F%2FSplit%2F%2F%20button%20(or%20%2F%2FCtrl%2BP%2F%2F)%20to%20build%20pair%20structures.%0A%0AType%20into%20prompts%20and%20validate%20to%20create%20%2F%2Fatoms%2F%2F.%0A%0ADrag%20%26%20drop%20existing%20arrows%20into%20prompts%20to%20reuse%20them%20in%20new%20structures.%0AOr%20use%20%2F%2Fincoming%2F%2F%20and%20%2F%2Foutgoing%2F%2F%20buttons.%0A%0ARoot%20arrows%20to%20make%20them%20%2F%2Freal%2F%2F%2C%20that%20is%20persistant.%0AAvoid%20directly%20rooting%20non-repeatable%20atoms%20-like%20random%20data-%20or%0Aspace%20will%20be%20lost.%0A%0A%3D%3D%20Modify%20content%0A%0AUnroot%20arrows%20to%20remove%20them%20for%20good.%0A%0ADouble-click%20existing%20atoms%20to%20edit%20them.%0AChanges%20will%20be%20propaged%20to%20visible%20descendants.%0A%0A%3D%3D%20Advanced%20text%20editing%0A%0ATurn%20prompts%20into%20text%20areas%20with%20%2F%2Fwhale%20of%20text%2F%2F%20button%20(.___.).%0AThen%20choose%20between%20%2F%2Fraw%2F%2F%20and%20%2F%2Fwiki%2F%2F%20text%20format.%20Wiki%20syntax%20is%0A%5B%5Bhttp%3A%2F%2Fwikicreole.org%2Fwiki%2FCreole1.0%7CCreole%201.0%5D%5D.%0A%0AAlternatively%2C%20%2F%2FCtrl%2BEnter%2F%2F%20also%20helps%20unfold%20then%20submit%20a%20wall%20of%20text.%0A%0A%3D%3D%20File%20upload%0A%0AUse%20%2F%2F(...)%2F%2F%20button%20to%20upload%20content%20into%20a%20prompt.%0A%0A%3D%3D%20Auto%20display%0A%0AArrows%20linked%20to%20%2F%2Fhello%2F%2F%20atom%20are%20displayed%20at%20startup.%0ALinks%20must%20be%20%2F%2Frooted%2F%2F.%0A+hello");
        assert(a);
        char* s = xl_strOf(headOf(headOf(tailOf(headOf(headOf(a))))));
        assert(s);
        fprintf(stderr, "%s\n", s);
        free(s);
        
    }
    xl_over();
    return 0;
}
