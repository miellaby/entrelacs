#include <stdlib.h> // free & co
#include <stdio.h>  // sprintf & co
#include <assert.h>
#include <string.h>
#include "log/log.h"

#include "entrelacs/entrelacs.h"
#include "space/space.h"
#include "mem/geoalloc.h"

char* test_title;
#define test_title(T) fprintf(stderr, "%s BEGIN\n", (test_title = T))
#define test_ok()  fprintf(stderr, "%s: OK\n", test_title)
#define test_done() fprintf(stderr, "ALL TESTS FROM " __FILE__ " : OK\n")
static struct s_buffer {
    uint32_t size;
    uint32_t max;
    char* buffer;
} buffer = {0, 0, NULL};

Address _printArrow(Address a) {
    if (xl_isRooted(a)) {
        int size = buffer.size;
        geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), size + 1);
        sprintf(buffer.buffer + size - 1, "_");
    }

    enum e_xlType t = xl_typeOf(a);
    if (t == XL_ATOM) {
        uint32_t l;
        char* s = (char *)xl_memOf(a, &l);
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

Address printArrow(Address a, void* context) {
    (void) context;

    geoalloc(&buffer.buffer, &buffer.max, &buffer.size, sizeof (char), 1);
    buffer.buffer[0] = '\0';
    _printArrow(a);
    fprintf(stderr, "%s\n", buffer.buffer);
    return 0;
}

int basic() {
    // assimilate arrows
    test_title("assimilate arrows");
    DEFATOM(hello); // Address hello = xl_atom("hello");
    DEFATOM(world);
    DEFATOM(small12345);

    DEFATOM(more_bigger_string_11111111111111111111);
    DEFA(hello, world); // Address _hello_world = xl_pair(hello, world);
    test_ok();


    // check regular pairs
    test_title("check regular pairs");
    assert(xl_typeOf(_hello_world) == XL_PAIR);
    assert(xl_tailOf(_hello_world) == hello);
    assert(xl_headOf(_hello_world) == world);
    test_ok();

    test_title("Test Adam special case");
    assert(xl_pair(EVE, EVE) != EVE);
    test_ok();

    // Check xl_atoms
    test_title("check xl_atoms");
    assert(xl_typeOf(hello) == XL_ATOM && xl_typeOf(world) == XL_ATOM
            && xl_typeOf(more_bigger_string_11111111111111111111) == XL_ATOM
            && xl_typeOf(small12345) == XL_ATOM);

    char *s1 = xl_strOf(hello);
    assert(!strcmp("hello", s1));
    free(s1);

    char *s2 = xl_strOf(world);
    assert(!strcmp("world", s2));
    free(s2);

    char *s3 = xl_strOf(more_bigger_string_11111111111111111111);
    assert(!strcmp("more_bigger_string_11111111111111111111", s3));
    free(s3);

    char *s4 = xl_strOf(small12345);
    assert(!strcmp("small12345", s4));
    free(s4);
    test_ok();

    // check rooting
    test_title("check rooting");
    xl_root(_hello_world);
    xl_root(more_bigger_string_11111111111111111111);

    assert(xl_isRooted(_hello_world));
    assert(xl_isRooted(more_bigger_string_11111111111111111111));
    test_ok();

    { // check very big string (blob)
        test_title("check very big string (blob)");

        char* bigStr = "11111111112222222222233333333333334444444444445555555555566666666666677777777777788888888888888999999999999999";
        Address bigAtom = xl_atom(bigStr);
        char* bigStrBack = xl_strOf(bigAtom);
        assert(0 == strcmp(bigStrBack, bigStr));
        assert(xl_isAtom(bigAtom));
        free(bigStrBack);
        test_ok();

        { //  digest computation
            test_title("check digest computation");
            char* digest = xl_digestOf(bigAtom, NULL);
            assert(digest);
            fprintf(stderr, "digest: %s\n", digest);
            test_ok();

            { // check digest
            test_title("check digest-based arrow retrieval");
            Address byDigest = xl_digestMaybe(digest);

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
    xl_commit();
    assert(xl_typeOf(loose) == XL_UNDEF);
    assert(xl_typeOf(_hello_loose) == XL_UNDEF);
    test_ok();

    // check rooting persistency
    test_title("check rooting persistency");
    assert(xl_isRooted(_hello_world));
    test_ok();

    // check deduplication
    test_title("check deduplication");
    Address original = _hello_world;
    Address original_big_string = more_bigger_string_11111111111111111111;
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
        Address uri = xl_uri("/hello+world");
        assert(uri == _hello_world);
    }
    test_ok();

    // check nxl_atom/xl_atom equivalency
    test_title("check nxl_atom/xl_atom equivalency");
    {
        Address helloB = xl_atomn(5, (uint8_t *)"hello");
        Address worldB = xl_atomn(5, (uint8_t *)"world");
        DEFA(helloB, worldB);
        assert(original == _helloB_worldB);
    }
    test_ok();

    // check nxl_atom dedup
    test_title("check nxl_atom dedup");
    Address fooB = xl_atom("headOf");
    Address barB = xl_atom("tailOf");
    DEFA(fooB, barB);
    Address originalB = _fooB_barB;
    {
        Address fooB = xl_atomn(6, (uint8_t *)"headOf");
        Address barB = xl_atomn(6, (uint8_t *)"tailOf");
        DEFA(fooB, barB);
        assert(originalB == _fooB_barB);
    }
    test_ok();

    // check pair connection
    test_title("pair connection");
    DEFATOM(dude);
    DEFA(hello, dude);
    xl_root(_hello_dude);

    xl_childrenOfCB(hello, printArrow, NULL);
    test_ok();

    // check unrooting
    test_title("check unrooting");
    xl_unroot(_hello_world);
    assert(!xl_isRooted(_hello_world));
    test_ok();

    // check GC after unrooting
    test_title("check GC after unrooting");
    xl_commit();
    assert(XL_UNDEF == xl_typeOf(_hello_world));
    assert(XL_UNDEF == xl_typeOf(world));
    assert(XL_ATOM == xl_typeOf(hello));
    test_ok();

    // check pair disconnection
    test_title("check pair disconnection");
    xl_childrenOfCB(hello, printArrow, NULL);
    test_ok();

    xl_unroot(_hello_dude);
    xl_commit();
    fprintf(stderr, "test done.\n");
    return 0;
}

int stress() {
    char buffer[50];
    Address xl_atoms[1000];
    Address pairs[500];
    Address big;

    // deduplication stress
    test_title("deduplication stress");
    {
        for (int i = 0; i < 200; i++) {
            snprintf(buffer, 50, "This is the tag #%d", i);
            xl_atoms[i] = xl_atom(buffer);
            if (i % 2) {
                int j = (i - 1) / 2;
                pairs[j] = xl_pair(xl_atoms[i - 1], xl_atoms[i]);
                printArrow(xl_tailOf(pairs[j]), NULL);
                printArrow(xl_headOf(pairs[j]), NULL);
                printArrow(pairs[j], NULL);
            }
        }

        for (int i = 0; i < 200; i++) {
            snprintf(buffer, 50, "This is the tag #%d", i);
            Address tagi = xl_atom(buffer);
            assert(xl_atoms[i] == tagi);
            if (i % 2) {
                int j = (i - 1) / 2;
                Address pairj = xl_pair(xl_atoms[i - 1], xl_atoms[i]);
                assert(pairs[j] == pairj);
            }
        }
    }
    test_ok();

    // rooting stress (also preserve this material from GC for further tests)
    test_title("rooting stress");
    {
        for (int i = 0; i < 200; i++) {
            xl_root(xl_atoms[i]);
            if (i % 2) {
                int j = (i - 1) / 2;
                xl_root(pairs[j]);
            }
        }
        xl_commit();
    }
    test_ok();

    DEFATOM(connectMe);

    // connection stress
    test_title("connection stress");
    {
        xl_root(connectMe);
        for (int i = 0; i < 200; i++) {
            Address child = xl_pair(connectMe, xl_atoms[i]);
            xl_root(child);
        }
        for (int j = 0; j < 100; j++) {
            printArrow(pairs[j], NULL);
            Address child = xl_pair(connectMe, pairs[j]);
            xl_root(child);
        }
        xl_childrenOfCB(connectMe, printArrow, NULL);
    }
    test_ok();

    // disconnection stress
    test_title("disconnection stress");
    {
        for (int i = 0; i < 200; i++) {
            Address child = xl_pair(connectMe, xl_atoms[i]);
            xl_unroot(child);
        }
        for (int j = 0; j < 100; j++) {
            Address child = xl_pair(connectMe, pairs[j]);
            xl_unroot(child);
        }
        xl_childrenOfCB(connectMe, printArrow, NULL);
    }
    test_ok();

    // connecting stress (big depth)
    test_title("connecting stress (big depth)");
    {
        Address loose = xl_atom("save me!");
        big = loose;
        for (int i = 0; i < 2; i++) {
            if (i % 2)
                big = xl_pair(big, xl_atoms[i]);
            else
                big = xl_pair(xl_atoms[i], big);
        }
        for (int j = 0; j < 100; j++) {
            if (j % 2)
                big = xl_pair(big, pairs[j]);
            else
                big = xl_pair(pairs[j], big);
        }
        xl_root(big);
        xl_commit();

        assert(xl_typeOf(loose) == XL_ATOM);
        printArrow(big, NULL);
    }
    test_ok();

    // disconnecting stress (big depth)
    test_title("disconnecting stress (big depth)");
    {
        xl_unroot(big);
        xl_commit();
        assert(xl_isEve(xl_atomMaybe("save me!")));

        // unrooting stress
        test_title("unrooting stress");
        for (int i = 0; i < 200; i++) {
            xl_unroot(xl_atoms[i]);
            if (i % 2) {
                int j = (i - 1) / 2;
                xl_unroot(pairs[j]);
            }
        }
        xl_unroot(connectMe);

        xl_commit();
    }
    test_ok();


    return 0;
}

int space_unitTest();

int main(int argc, char* argv[]) {
    (void) argc; (void) argv;
    log_init(NULL, "space=debug,mem=debug,cell=debug");
    xs_init();
    xl_open();
    space_unitTest();
    basic();
    stress();
    xl_close();
    test_done();
    return 0;
}
