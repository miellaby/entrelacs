#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "log.h"

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "mem.h" // geoalloc

#define test_title(T) fprintf(stderr, T "\n")

static struct s_buffer { int size; int max; char* buffer; } buffer = {0, 0, NULL} ;

Arrow _printArrow(Arrow a) {

    if (xl_isRooted(a)) {
        int size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof(char), size + 1);
        sprintf(buffer.buffer + size - 1, "_");
    }

    enum e_xlType t = xl_typeOf(a);
    if (t == XL_ATOM) {
        int l;
        char* s = xl_memOf(a, &l);
        int size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof(char), size + l + 2);
        sprintf(buffer.buffer + size - 1, "\"%s\"", s);
        free(s);
    } else if (t == XL_PAIR) {
        int size;
        size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof(char), size + 1);
        sprintf(buffer.buffer + size - 1, "(");
        _printArrow(xl_tailOf(a));
        size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof(char), size + 2);
        sprintf(buffer.buffer + size - 1, ", ");
        _printArrow(xl_headOf(a));
        size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof(char), size + 1);
        sprintf(buffer.buffer + size - 1, ")");
    }
    return a;
}

Arrow printArrow(Arrow a, Arrow ctx) {
    geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof(char), 1);
    buffer.buffer[0] = '\0';
    _printArrow(a);
    fprintf(stderr, "%s\n", buffer.buffer);
    return 0;
}


int basic() {
    // assimilate arrows
    test_title("assimilate arrows");
    DEFATOM(hello); // Arrow hello = xl_atom("hello");
    DEFATOM(world);
    DEFATOM(more_bigger_string_11111111111111111111);
    DEFA(hello, world); // Arrow _hello_world = xl_pair(hello, world);
    

    // check regular pairs
    test_title("check regular pairs");
    assert(typeOf(_hello_world) == XL_PAIR);
    assert(tail(_hello_world) == hello);
    assert(head(_hello_world) == world);
    
    // Check atoms
    test_title("check atoms");
    assert(typeOf(hello) == XL_ATOM && typeOf(world) == XL_ATOM);
	char *s1 = str(hello);
	char *s2 = str(world);
	assert(!strcmp("hello", s1) && !strcmp("world", s2));
	free(s1);
	free(s2);
    
    // check rooting
    test_title("check rooting");
    root(_hello_world);
    root(more_bigger_string_11111111111111111111);
    assert(isRooted(_hello_world));
    assert(isRooted(more_bigger_string_11111111111111111111));

    // check GC
    test_title("check GC");
	DEFATOM(loose);
    DEFA(hello, loose);
    commit();
    assert(typeOf(loose) == XL_UNDEF);
    assert(typeOf(_hello_loose) == XL_UNDEF);
    
    // check rooting persistency
    test_title("check rooting persistency");
    assert(isRooted(_hello_world));
    
    // check deduplication
    test_title("check deduplication");
    Arrow original = _hello_world;
    Arrow original_big_string = more_bigger_string_11111111111111111111;
    {
       DEFATOM(more_bigger_string_11111111111111111111);
       assert(original_big_string == more_bigger_string_11111111111111111111);
       DEFATOM(hello);
       DEFATOM(world);
       DEFA(hello, world);
       assert(original == _hello_world);
    }

    // check uri assimilation
    test_title("check URI assimilation");
    {
        Arrow uri = uri("/hello.world");
        assert(uri == _hello_world);
    }

    // check natom/atom equivalency
    test_title("check natom/atom equivalency");
    {
        Arrow helloB = natom(5, "hello");
        Arrow worldB = natom(5, "world");
        DEFA(helloB, worldB);
        assert(original == _helloB_worldB);
   }

    // check natom dedup
    test_title("check natom dedup");
    Arrow fooB = atom("headOf");
    Arrow barB = atom("tailOf");
    DEFA(fooB, barB);
    Arrow originalB = _fooB_barB;
    {
        Arrow fooB = natom(6, "headOf");
        Arrow barB = natom(6, "tailOf");
            DEFA(fooB, barB);
            assert(originalB == _fooB_barB);
    }

    // check pair connection
    test_title("pair connection");
    DEFATOM(dude);
    DEFA(hello, dude);
    root(_hello_dude);
    
    childrenOfCB(hello, printArrow, Eve());
    
    // check unrooting
    test_title("check unrooting");
    unroot(_hello_world);
    assert(!isRooted(_hello_world));
    
    // check GC after unrooting
    test_title("check GC after unrooting");
    commit();
    assert(XL_UNDEF == typeOf(_hello_world));
    assert(XL_UNDEF == typeOf(world));
    assert(XL_ATOM == typeOf(hello));

    // check pair disconnection
    test_title("check pair disconnection");
    childrenOfCB(hello, printArrow, Eve());
    
    unroot(_hello_dude);
    commit();

    return 0;
}

int stress() {
    char buffer[50];
    Arrow atoms[1000];
    Arrow pairs[500];
    Arrow big;

    // deduplication stress
    test_title("deduplication stress");
    {
        for (int i = 0 ; i < 200; i++) {
            snprintf(buffer, 50, "This is the tag #%d", i);
            atoms[i] = atom(buffer);
            if (i % 2) {
                int j = (i - 1) / 2;
                pairs[j] = pair(atoms[i - 1], atoms[i]);
                printArrow(tailOf(pairs[j]), Eve());
                printArrow(headOf(pairs[j]), Eve());
                printArrow(pairs[j], Eve());
            }
        }

        for (int i = 0 ; i < 200; i++) {
            snprintf(buffer, 50, "This is the tag #%d", i);
            Arrow tagi = atom(buffer);
            assert(atoms[i] == tagi);
            if (i % 2) {
                int j = (i - 1) / 2;
                Arrow pairj = pair(atoms[i - 1], atoms[i]);
                assert(pairs[j] == pairj);
            }
        }
    }

    // rooting stress (also preserve this material from GC for further tests)
    test_title("rooting stress");
    {
        for (int i = 0 ; i < 200; i++) {
            root(atoms[i]);
            if (i % 2) {
                int j = (i - 1) / 2;
                root(pairs[j]);
            }
        }
        commit();
    }

    DEFATOM(connectMe);

    // connection stress
    test_title("connection stress");
    {
        root(connectMe);
        for (int i = 0 ; i < 200; i++) {
            Arrow child = pair(connectMe, atoms[i]);
            root(child);
        }
        for (int j = 0 ; j < 100; j++) {
            printArrow(pairs[j], Eve());
            Arrow child = pair(connectMe, pairs[j]);
            root(child);
        }
        childrenOfCB(connectMe, printArrow, Eve());
    }

    // disconnection stress
    test_title("disconnection stress");
    {
        for (int i = 0 ; i < 200; i++) {
            Arrow child = pair(connectMe, atoms[i]);
            unroot(child);
        }
        for (int j = 0 ; j < 100; j++) {
            Arrow child = pair(connectMe, pairs[j]);
            unroot(child);
        }
        childrenOfCB(connectMe, printArrow, Eve());
    }

    // connecting stress (big depth)
    test_title("connecting stress (big depth)");
    {
        Arrow loose = atom("save me!");
        big = loose;
        for (int i = 0 ; i < 2; i++) {
            if (i % 2)
                big = pair(big, atoms[i]);
            else
                big = pair(atoms[i], big);
        }
        for (int j = 0 ; j < 100; j++) {
            if (j % 2)
                big = pair(big, pairs[j]);
            else
                big = pair(pairs[j], big);
        }
        root(big);
        commit();

        assert(typeOf(loose) == XL_ATOM);
        printArrow(big, Eve());
    }

    // disconnecting stress (big depth)
    test_title("disconnecting stress (big depth)");
    {
        unroot(big);
        commit();
        assert(isEve(atomMaybe("save me!")));

        // unrooting stress
        test_title("unrooting stress");
        for (int i = 0 ; i < 200; i++) {
            unroot(atoms[i]);
            if (i % 2) {
                int j = (i - 1) / 2;
                unroot(pairs[j]);
            }
        }
        unroot(connectMe);

        commit();
    }


    return 0;
}

int main(int argc, char* argv[]) {
    log_init(NULL, "space=debug");
    xl_init();
    basic();
    stress();
	return 0;
}
