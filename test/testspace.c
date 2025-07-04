#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "log/log.h"

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "mem/geoalloc.h" 

char* test_title;
#define test_title(T) fprintf(stderr, "%s BEGIN\n", (test_title = T))
#define test_ok()  fprintf(stderr, "%s: OK\n", test_title)
#define test_done() fprintf(stderr, "ALL TESTS FROM " __FILE__ " : OK\n")
static struct s_buffer {
    int size;
    int max;
    char* buffer;
} buffer = {0, 0, NULL};

Arrow _printArrow(Arrow a) {
    
    if (xl_isRooted(a)) {
        int size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), size + 1);
        sprintf(buffer.buffer + size - 1, "_");
    }

    enum e_xlType t = xl_typeOf(a);
    if (t == XL_ATOM) {
        int l;
        char* s = xl_memOf(a, &l);
        int size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), size + l + 2);
        sprintf(buffer.buffer + size - 1, "\"%s\"", s);
        free(s);
    } else if (t == XL_PAIR) {
        int size;
        size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), size + 1);
        sprintf(buffer.buffer + size - 1, "(");
        _printArrow(xl_tailOf(a));
        size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), size + 2);
        sprintf(buffer.buffer + size - 1, ", ");
        _printArrow(xl_headOf(a));
        size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), size + 1);
        sprintf(buffer.buffer + size - 1, ")");
    }
    return a;
}

Arrow printArrow(Arrow a, Arrow ctx) {
    geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), 1);
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
    DEFATOM(small12345);

    DEFATOM(more_bigger_string_11111111111111111111);
    DEFA(hello, world); // Arrow _hello_world = xl_pair(hello, world);
    test_ok();
     

    // check regular pairs
    test_title("check regular pairs");
    assert(typeOf(_hello_world) == XL_PAIR);
    assert(tail(_hello_world) == hello);
    assert(head(_hello_world) == world);
    test_ok();
 
    test_title("Test Adam special case");
    assert(A(EVE, EVE) != EVE);
    test_ok();
 
    // Check atoms
    test_title("check atoms");
    assert(typeOf(hello) == XL_ATOM && typeOf(world) == XL_ATOM
            && typeOf(more_bigger_string_11111111111111111111) == XL_ATOM
            && typeOf(small12345) == XL_ATOM);
    
    char *s1 = str(hello);
    assert(!strcmp("hello", s1));
    free(s1);
    
    char *s2 = str(world);
    assert(!strcmp("world", s2));
    free(s2);
    
    char *s3 = str(more_bigger_string_11111111111111111111);
    assert(!strcmp("more_bigger_string_11111111111111111111", s3));
    free(s3);

    char *s4 = str(small12345);
    assert(!strcmp("small12345", s4));
    free(s4);
    test_ok();
    
    // check rooting
    test_title("check rooting");
    root(_hello_world);
    root(more_bigger_string_11111111111111111111);

    assert(isRooted(_hello_world));
    assert(isRooted(more_bigger_string_11111111111111111111));
    test_ok();
 
    { // check very big string (blob)
        test_title("check very big string (blob)");

        char* bigStr = "11111111112222222222233333333333334444444444445555555555566666666666677777777777788888888888888999999999999999";
        Arrow bigAtom = atom(bigStr);
        char* bigStrBack = strOf(bigAtom);
        assert(0 == strcmp(bigStrBack, bigStr));
        assert(xl_isAtom(bigAtom));
        free(bigStrBack);
        test_ok();
        
        { //  digest computation
            test_title("check digest computation");
            char* digest = digestOf(bigAtom, NULL);
            assert(digest);
            fprintf(stderr, "digest: %s\n", digest);
            test_ok();
        
            { // check digest
            test_title("check digest-based arrow retrieval");
            Arrow byDigest = digestMaybe(digest);
            assert(byDigest == bigAtom);
            test_ok();
            }

            free(digest);

        }
    }

    // check GC
    test_title("check GC");
    DEFATOM(loose);
    DEFA(hello, loose);
    commit();
    assert(typeOf(loose) == XL_UNDEF);
    assert(typeOf(_hello_loose) == XL_UNDEF);
    test_ok();

    // check rooting persistency
    test_title("check rooting persistency");
    assert(isRooted(_hello_world));
    test_ok();
 
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
    test_ok();
 
    // check uri assimilation
    test_title("check URI assimilation");
    {
        Arrow uri = uri("/hello+world");
        assert(uri == _hello_world);
    }
    test_ok();
 
    // check natom/atom equivalency
    test_title("check natom/atom equivalency");
    {
        Arrow helloB = atomn(5, "hello");
        Arrow worldB = atomn(5, "world");
        DEFA(helloB, worldB);
        assert(original == _helloB_worldB);
    }
    test_ok();
 
    // check natom dedup
    test_title("check natom dedup");
    Arrow fooB = atom("headOf");
    Arrow barB = atom("tailOf");
    DEFA(fooB, barB);
    Arrow originalB = _fooB_barB;
    {
        Arrow fooB = atomn(6, "headOf");
        Arrow barB = atomn(6, "tailOf");
        DEFA(fooB, barB);
        assert(originalB == _fooB_barB);
    }
    test_ok();
 
    // check pair connection
    test_title("pair connection");
    DEFATOM(dude);
    DEFA(hello, dude);
    root(_hello_dude);

    childrenOfCB(hello, printArrow, Eve());
    test_ok();
 
    // check unrooting
    test_title("check unrooting");
    unroot(_hello_world);
    assert(!isRooted(_hello_world));
    test_ok();
 
    // check GC after unrooting
    test_title("check GC after unrooting");
    commit();
    assert(XL_UNDEF == typeOf(_hello_world));
    assert(XL_UNDEF == typeOf(world));
    assert(XL_ATOM == typeOf(hello));
    test_ok();
 
    // check pair disconnection
    test_title("check pair disconnection");
    childrenOfCB(hello, printArrow, Eve());
    test_ok();
 
    unroot(_hello_dude);
    commit();
    fprintf(stderr, "test done.\n");
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
        for (int i = 0; i < 200; i++) {
            snprintf(buffer, 50, "This is the tag #%d", i);
            atoms[i] = atom(buffer);
            if (i % 2) {
                int j = (i - 1) / 2;
                pairs[j] = A(atoms[i - 1], atoms[i]);
                printArrow(tailOf(pairs[j]), Eve());
                printArrow(headOf(pairs[j]), Eve());
                printArrow(pairs[j], Eve());
            }
        }

        for (int i = 0; i < 200; i++) {
            snprintf(buffer, 50, "This is the tag #%d", i);
            Arrow tagi = atom(buffer);
            assert(atoms[i] == tagi);
            if (i % 2) {
                int j = (i - 1) / 2;
                Arrow pairj = A(atoms[i - 1], atoms[i]);
                assert(pairs[j] == pairj);
            }
        }
    }
    test_ok();
 
    // rooting stress (also preserve this material from GC for further tests)
    test_title("rooting stress");
    {
        for (int i = 0; i < 200; i++) {
            root(atoms[i]);
            if (i % 2) {
                int j = (i - 1) / 2;
                root(pairs[j]);
            }
        }
        commit();
    }
    test_ok();
 
    DEFATOM(connectMe);

    // connection stress
    test_title("connection stress");
    {
        root(connectMe);
        for (int i = 0; i < 200; i++) {
            Arrow child = A(connectMe, atoms[i]);
            root(child);
        }
        for (int j = 0; j < 100; j++) {
            printArrow(pairs[j], Eve());
            Arrow child = A(connectMe, pairs[j]);
            root(child);
        }
        childrenOfCB(connectMe, printArrow, Eve());
    }
    test_ok();
 
    // disconnection stress
    test_title("disconnection stress");
    {
        for (int i = 0; i < 200; i++) {
            Arrow child = A(connectMe, atoms[i]);
            unroot(child);
        }
        for (int j = 0; j < 100; j++) {
            Arrow child = A(connectMe, pairs[j]);
            unroot(child);
        }
        childrenOfCB(connectMe, printArrow, Eve());
    }
    test_ok();
 
    // connecting stress (big depth)
    test_title("connecting stress (big depth)");
    {
        Arrow loose = atom("save me!");
        big = loose;
        for (int i = 0; i < 2; i++) {
            if (i % 2)
                big = A(big, atoms[i]);
            else
                big = A(atoms[i], big);
        }
        for (int j = 0; j < 100; j++) {
            if (j % 2)
                big = A(big, pairs[j]);
            else
                big = A(pairs[j], big);
        }
        root(big);
        commit();

        assert(typeOf(loose) == XL_ATOM);
        printArrow(big, Eve());
    }
    test_ok();
 
    // disconnecting stress (big depth)
    test_title("disconnecting stress (big depth)");
    {
        unroot(big);
        commit();
        assert(isEve(atomMaybe("save me!")));

        // unrooting stress
        test_title("unrooting stress");
        for (int i = 0; i < 200; i++) {
            unroot(atoms[i]);
            if (i % 2) {
                int j = (i - 1) / 2;
                unroot(pairs[j]);
            }
        }
        unroot(connectMe);

        commit();
    }
    test_ok();
 

    return 0;
}

int space_unitTest();

int main(int argc, char* argv[]) {
    log_init(NULL, "space=debug");
    xl_init();
    xl_begin();
    space_unitTest();
    basic();
    stress();
    xl_over();
    test_done();
    return 0;
}
