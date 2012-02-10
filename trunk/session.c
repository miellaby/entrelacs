#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "log.h"
#include "session.h"

static Arrow session = EVE;

Arrow xls_session(Arrow agent, Arrow sessionUuid) {
   if (!session) session = tag("session");
   Arrow s = a(session, a(agent, sessionUuid));
   root(s);
   return s;
}

int xls_check(Arrow s, Arrow a) {
   return (tailOf(a) == s);
}

void xls_release(Arrow s, Arrow a) {
   unroot(a);
}

void xls_resetSession(Arrow s) {
   Arrow prec = EVE;
   XLEnum e = xl_childrenOf(s);
   while (xl_enumNext(e)) {
      Arrow child = xl_enumGet(e);
      unroot(prec); // only works if not Eve
      prec = child;
   }
   if (prec) unroot(prec);
   xl_freeEnum(e);
}

Arrow xls_closeSession(Arrow s) {
   xls_resetSession(s);
   unroot(s);
   return s;
}

static Arrow fromUrl(Arrow session, unsigned char* url, char** urlEnd, int locateOnly) {
    DEBUGPRINTF("BEGIN fromUrl(%06x, '%s')\n", session, url);
    Arrow a = xl_Eve();

    char c = url[0];
    if (c <= 32) { // Any control-caracters/white-spaces are considered as URI break
        a = xl_Eve();
        *urlEnd = url;
    } else switch (c) {
    case '.': // Eve
        a = xl_Eve();
        *urlEnd = url;
        break;
    case '$': {
        if (url[1] == 'H') {
            unsigned char c;
            uint32_t urlLength = 2;
            while ((c = url[urlLength]) > 32 && c != '.' && c != '/')
                urlLength++;
            assert(urlLength);
            a = xl_uri(url);
            if (xl_isEve(a)) {
                *urlEnd = NULL;
            } else {
                *urlEnd = url + urlLength;
            }
            break;
        } else {
            int ref;
            sscanf(url + 1, "%x", &ref);
            Arrow sa = ref;
            if (xl_tailOf(sa) == session) {
                a = xl_headOf(sa);
                *urlEnd = url + 7;
            } else {
                *urlEnd = NULL;
            }
        }
     }
    case '/': { // ARROW
        char *tailUriEnd, *headUriEnd;
        Arrow tail, head;
        tail = fromUrl(session, url + 1, &tailUriEnd, locateOnly);
        if (!tailUriEnd) {
            *urlEnd = NULL;
            break;
        }

        char* headURIStart = *tailUriEnd == '.' ? tailUriEnd + 1 : tailUriEnd;
        head = fromUrl(session, headURIStart, &headUriEnd, locateOnly);
        if (!headUriEnd) {
            *urlEnd = NULL;
            break;
        } else {
            *urlEnd = headUriEnd;
        }

        a = (locateOnly ? xl_arrowMaybe(tail, head) : xl_arrow(tail, head));
        if (xl_isEve(a) && !(xl_isEve(tail) && xl_isEve(head))) {
            *urlEnd = NULL;
        }
        break;
    }
    default: { // TAG
        unsigned char c;
        uint32_t urlLength = 0;

        while ((c = url[urlLength]) > 32  && c != '.' && c != '/')
            urlLength++;
        assert(urlLength);

        a = (locateOnly ? xl_uriMaybe(url) : xl_uri(url));
        if (xl_isEve(a)) {
            *urlEnd = NULL;
        } else {
            *urlEnd = url + urlLength;
        }
        break;
    }
    }

    DEBUGPRINTF("END fromUrl(%06x, '%s') = %06x\n", session, url, a);
    return a;
}

static Arrow url(Arrow session, char *url, int locateOnly) {
    char c, *urlEnd;
    Arrow a = fromUrl(session, url, &urlEnd, locateOnly);
    if (!urlEnd) return xl_Eve();


    while ((c = *urlEnd) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
           // white spaces are tolerated and ignored here
           urlEnd++;
    }

    if (*urlEnd) {
       DEBUGPRINTF("urlEnd = >%s<\n", urlEnd);

       Arrow b = fromUrl(session, urlEnd, &urlEnd, locateOnly);
       if (!urlEnd) return xl_Eve();
       a = (locateOnly ? arrowMaybe(a, b) : arrow(a, b)); // TODO: document actual design
    }

    return a;
}

Arrow xls_url(Arrow session, char* aUrl) {
   return url(session, aUrl, 0);
}

Arrow xls_urlMaybe(Arrow session, char* aUrl) {
   return url(session, aUrl, 1);
}

static char* toURL(Arrow session, Arrow a, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        Arrow sa = xl_arrow(session, a);
        xl_root(sa);
        sprintf(url, "%%%06x", (int)sa);
        *l = 7;
        return url;
    } else if (xl_typeOf(a) == XL_ARROW) { // TODO tuple
        uint32_t l1, l2;
        char *tailUrl = toURL(session, xl_tailOf(a), depth - 1, &l1);
        char *headUrl = toURL(session, xl_headOf(a), depth - 1, &l2);
        char *url = malloc(2 + l1 + l2 + 1) ;
        assert(url);
        sprintf(url, "/%s.%s", tailUrl, headUrl);
        free(tailUrl);
        free(headUrl);
        *l = 2 + l1 + l2;
        return url;
    } else {
        char* url = xl_uriOf(a);
        *l = strlen(url);
        return url;
    }
}


char* xls_urlOf(Arrow session, Arrow a, int depth) {
    uint32_t l;
    return toURL(session, a, depth, &l);
}
