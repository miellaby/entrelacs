#include "url.h"
#define LOG_CURRENT LOG_SERVER
#include "log/log.h"
#include "machine/uri.h"
#include "machine/context.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


static Arrow _fromUrl(Arrow context, char* url, char** urlEnd) {
    DEBUGPRINTF("BEGIN _fromUrl(%O, '%s')", context, url);
    Arrow eve = xs_eve();
    Arrow a = eve;

    char c = url[0];
    if (c >= 0 && c <= 32) { // Any control-caracters/white-spaces are considered as URI break
        *urlEnd = url;
    } else switch (c) {
            case '+': // Eve
                *urlEnd = url;
                break;
            case '/':
            { // ARROW
                char *tailUrlEnd, *headUrlEnd;
                Arrow tail, head;
                tail = _fromUrl(context, url + 1, &tailUrlEnd);
                if (!tailUrlEnd) {
                    a = tail; // NULL or eve
                    *urlEnd = NULL;
                    break;
                }

                if (!*tailUrlEnd) { // no more char
                    a = tail;
                    *urlEnd = tailUrlEnd;
                    break;
                }

                char* headUrlStart = *tailUrlEnd == '+' ? tailUrlEnd + 1 : tailUrlEnd;
                head = _fromUrl(context, headUrlStart, &headUrlEnd);
                if (!headUrlEnd) {
                    a = head;
                    *urlEnd = NULL;
                    break;
                }

                a = xs_pair(tail, head);
                *urlEnd = headUrlEnd;
                break;
            }
            case '$':
            {
                if (url[1] != 'H') {
                    int ref = 0;
                    sscanf(url + 1, "%x", &ref);
                    // Security check: no way to resolve a %x ref which hasn't been forged in the context
                    Arrow sa = xs_arrow(ref);
                    if (sa == NULL || !xs_isPair(sa) || !xs_equal(xs_getTail(sa), context)) {
                        a = NULL;
                        *urlEnd = NULL;
                    }
                    a = xs_getHead(sa);
                    *urlEnd = url + 7;
                    break;
                }
                __attribute__ ((fallthrough));
            }
            default:
            { // TAG, BLOB
                uint32_t urlLength = 0;

                while ((c = url[urlLength]) > 32 && c != '+' && c != '/')
                    urlLength++;
                assert(urlLength);

                a = xs_parseURI(urlLength, url, NULL);
                if (a == NULL || a == eve) {
                    *urlEnd = NULL;
                    break;
                }

                *urlEnd = url + urlLength;
                break;
            }
        }

    DEBUGPRINTF("END _fromUrl(%O, '%s') = %O", context, url, a);
    return a;
}

static char* skeepSpacesAndOnePlus(const char* urlEnd) {
    char c;
    while ((c = *urlEnd) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
        // white spaces are tolerated and ignored here
        urlEnd++;
    }
    if (c == '+') urlEnd++;
    return urlEnd;
}

static Arrow fromUrl(Arrow context, char *url) {
    DEBUGPRINTF("BEGIN fromUrl(%O, '%s')", context, url);
    char *nextUrl;
    Arrow a = _fromUrl(context, url, &nextUrl);
    if (!nextUrl) return a; // NIL or EVE

    nextUrl = skeepSpacesAndOnePlus(nextUrl);
    while (*nextUrl) {
        DEBUGPRINTF("nextUrl = >%s<", nextUrl);
        Arrow b = _fromUrl(context, nextUrl, &nextUrl);
        if (!nextUrl) return b; // NIL or EVE

        a = xs_pair(a, b); // TODO: document actual design

        nextUrl = skeepSpacesAndOnePlus(nextUrl);
    }

    DEBUGPRINTF("END fromUrl(%O, '%s') = %O", context, url, a);
    return a;
}

Arrow xs_url(Arrow context, char* aUrl) {
    TRACEPRINTF("BEGIN xs_url(%O, '%s')", context, aUrl);
    context = xs_pair(xs_const("locked"), context);
    Arrow arrow = fromUrl(context, aUrl);
    TRACEPRINTF("END xs_url(%O, '%s') = %O", context, aUrl, arrow);
    return arrow;
}


static char* toURL(Arrow context, Arrow e, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        Arrow sa = xs_context_root(context, e);
        sprintf(url, "$%06x", (int)xs_getId(sa));
        *l = 7;
        return url;
    } else if (xs_isPair(e)) { // TODO tuple
        uint32_t l1, l2;
        char *tailUrl = toURL(context, xs_getTail(e), depth - 1, &l1);
        char *headUrl = toURL(context, xs_getHead(e), depth - 1, &l2);
        char *url = malloc(2 + l1 + l2 + 1) ;
        assert(url);
        sprintf(url, "/%s+%s", tailUrl, headUrl);
        free(tailUrl);
        free(headUrl);
        *l = 2 + l1 + l2;
        return url;
    } else {
        return xs_getURI(e, l);
    }
}


char* xs_getURL(Arrow s, Arrow e, int depth) {
    TRACEPRINTF("BEGIN xs_getURL(%O, %O, %d)", s, e, depth);
    uint32_t l;
    Arrow context = xs_pair(xs_const("locked"), s);

    char* url = toURL(context, e, depth, &l);
    TRACEPRINTF("END xs_getURL(%O, %O, %d) = '%s'", s, e, depth, url);
    return url;
}
