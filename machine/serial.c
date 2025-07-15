#include "machine/serial.h"
#include "space/cell.h"
#define LOG_CURRENT LOG_MACHINE
#include "log/log.h"
#include <stddef.h>

// get an URI path corresponding to an arrow
char* xs_getURI(Arrow a, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (xs_isEve(a)) { // Eve is identified by an empty path
        // allocate and return an empty string
        char *s = (char*) malloc(1);
        assert(s);
        s[0] = '\0';
        if (l) *l = 0; // if asked, return length = 0
        return s;
    }

    switch (xs_getType(a)) {
        case XS_ATOM: {
            char *uri;
            size_t size;
            uint8_t* raw = xs_borrowMem(a, &size);
            if (size >= BLOB_MINSIZE) {
                uri = xs_getDigest(a, l);
                uri = malloc(3 * size + 1); // memory allocation for encoded content
                assert(uri);
                size_t uri_size;
                percent_encode(raw, size, uri, &uri_size);
                uri = realloc(uri, 1 + uri_size);
                if (l) *l = uri_size; // return length if asked
            }
            return uri;
        }
        case XS_PAIR:
        { // concat tail and head identifiers into /tail+head style URI
            uint32_t l1, l2;
            char *tailUri = xs_getURI(xs_getTail(a), &l1);
            if (tailUri == NULL) return NULL;

            char *headUri = xs_getURI(xs_getHead(a), &l2);
            if (headUri == NULL) {
                free(tailUri);
                return NULL;
            }

            // allocate to save result
            char *uri = malloc(2 + l1 + l2 + 1);
            assert(uri);
            // concat identiers
            sprintf(uri, "/%s+%s", tailUri, headUri); // TODO no printf
            free(tailUri);
            free(headUri);
            if (l) *l = 2 + l1 + l2; // return length if asked
            return uri;
        }
        default:
            assert(0);
    } // switch
}

static uint32_t skeepSpacesAndOnePlus(uint32_t size, char* uriEnd) {
    char c;
    uint32_t l = 0;
    while ((size == NAN || l < size)
            && (c = uriEnd[l]) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
        // white spaces are tolerated and ignored here
        l++;
    }
    if (c == '+' && (size == NAN || l < size)) l++;
    return l;
}

Arrow xs_parseURI(uint32_t size, char *uri, uint32_t *uri_size_p) {
    TRACEPRINTF("BEGIN xs_parseUri(%s)", uri);
    if (uri == NULL) {
        return NULL;
    }
    if (size == 0) {
        if (uri_size_p) {
            *uri_size_p = 0;
        }
        return xs_eve();
    }

    Arrow a = NULL;
    uint32_t uri_size = 0;

    char c = uri[0];

    if (c <= 32 || !size) { // Any control-caracters/white-spaces are considered as URI break
        a = xs_eve();
    } else switch (c) {
        case '+': // Eve
            a = xs_eve();
            break;
        case '$': {
            if (size != NAN && size < DIGEST_SIZE) {
                TRACEPRINTF("xs_parseURI - not long enough for a digest: %d", size);
                break;
            }

            if (uri[1] != 'H') { // Only digest are allowed in URI
                break;
            }

            uri_size = 2;
            while ((size == NAN || uri_size < size)
                    && (c = uri[uri_size]) > 32 && c != '+' && c != '/')
                uri_size++;

            if (uri_size != DIGEST_SIZE) {
                TRACEPRINTF("xs_parseURI - wrong digest size %d", DIGEST_SIZE);
                break;
            }
            Arrow a = xs_digest(uri);
            if (a == NULL) {
                TRACEPRINTF("xs_parseURI - wrong digest %s", DIGEST_SIZE);
            }
            break;
        }
        case '/':
        { // Pair
            uint32_t tailUriSize, headUriSize;
            Address tail, head;

            if (size != NAN) size--;

            tail = xs_parseURI(size, uri + 1, &tailUriSize);
            if (tail ==  NULL) { // Issue
                return NULL;
            }

            char tailUriEnd = uri[1 + tailUriSize];
            if (tailUriEnd == '\0' || tailUriSize == size /* no more char */) {
                a = tail;
                uri_size = 1 + tailUriSize;
                break;
            }

            char* headUriStart;
            if (tailUriEnd == '+') {
                if (size != NAN)
                    size -= tailUriSize + 1;
                headUriStart = uri + 1 + tailUriSize + 1;
            } else {
                if (size != NAN)
                    size -= tailUriSize;
                headUriStart = uri + 1 + tailUriSize;
            }

            head = xs_parseURI(size, headUriStart, &headUriSize);
            if (head == NULL) { // issue
                return NULL;
            }

            a = xs_pair(tail, head);
            uri_size = 1 + tailUriSize + (tailUriEnd == '+' /* 1/0 */) + headUriSize;
            break;
        }
        default:
        { // ATOM
            uint32_t atomLength;

            // compute atom URI length
            uri_size = 0;
            while ((size == NAN || uri_size < size)
                    && (c = uri[uri_size]) > 32 && c != '+' && c != '/')
                uri_size++;
            assert(uri_size);

            uint8_t *atomStr = malloc(uri_size + 1);
            percent_decode(uri, uri_size, atomStr, &atomLength);
            a = xs_atomn(atomLength, atomStr);
            free(atomStr);
        }
    }

    if (uri_size_p) *uri_size_p = uri_size;
    return a;
}

Arrow xs_parseURIs(uint32_t size, char *uri, uint32_t *uri_size_p) { // TODO: document actual design
    char c;
    uint32_t uriLength, gap;
    Arrow a = xs_parseUri(size, uri, &uriLength);
    if (a == NULL)
        return NULL; // return NULL

    if (size != NAN) size -= uriLength;
    if (size == 0) {
        if (uri_size_p) *uri_size_p = uriLength;
        return a;
    }

    gap = skeepSpacesAndOnePlus(size, uri + uriLength);
    if (size != NAN)
       size = (gap > size ? 0 : size - gap);
    uriLength += gap;
    while ((size == NAN || size--) && (c = uri[uriLength])) {
        DEBUGPRINTF("nextUri = >%s<", uri + uriLength);
        uint32_t nextUriOffset;
        Arrow b = serial_parseUri(size, uri + uriLength, &nextUriOffset);
        if (b == NULL) {
            if (uri_size_p) *uri_size_p = uriLength;
            return NULL; // return NULL wrong URI
        }

        a = xs_pair(a, b);

        uriLength += nextUriOffset;
        if (size != NAN) size -= nextUriOffset;

        gap = skeepSpacesAndOnePlus(size, uri + uriLength);

        uriLength += gap;
        if (size != NAN) size -= gap;
    }

    if (uri_size_p) *uri_size_p = uriLength;
    return a;
}
