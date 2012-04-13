#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "log.h"
#include "session.h"

Arrow xls_session(Arrow superSession, Arrow agent, Arrow sessionUuid) {
   Arrow sessionTag = tag("session");
   Arrow context = a(sessionTag, a(agent, sessionUuid));
   xls_root(superSession, context);
   Arrow session = a(superSession, context);
   return session;
}

/* TODO: FIXME: We could parameter a maximal depth of deep rooting (meta-level)
   when reached, we could break the recursion and root directly to "buzz"
   e.g. deepRoot(/s0.s1.s2.s3, a, depth=2) <=> root(/"buzz"/s2/s3.a) s0/s1 super context forgotten
   NO. IT DOESN'T WORK UNLESS ONE SETS UP COUNTERS OR TTL
   */
Arrow deepRoot(Arrow s, Arrow a) {
    Arrow rest = tailOf(s);
    if (rest != s) {
        return deepRoot(rest, arrow(headOf(s),a));
    } else if (s) {
        return xl_root(arrow(s, a));
    } else {
        return xl_root(a);
    }
}

Arrow xls_root(Arrow s, Arrow a) {
    root(a(s, a));
    return deepRoot(s, a);
}

Arrow deepIsRooted(Arrow s, Arrow a) {
    Arrow rest = tailOf(s);
    if (rest != s) {
        return deepIsRooted(rest, arrow(headOf(s),a));
    } else if (s) {
        return xl_isRooted(arrow(s, a));
    } else {
        return xl_isRooted(a);
    }
}

Arrow xls_isRooted(Arrow s, Arrow a) {
    Arrow m = arrowMaybe(s, a);
    if (isEve(m)) return m;
    return deepIsRooted(m);
}

void deepUnroot(Arrow s, Arrow a) {
    Arrow rest = tailOf(s);
    if (rest != s) { // TODO : maybe
        deepUnroot(rest, arrow(headOf(s),a));
    } else if (s) {
        xl_unroot(arrow(s, a));
    } else {
        xl_unroot(a);
    }
}

Arrow xls_unroot(Arrow s, Arrow a) {
    unroot(a(s, a));
    return deepUnroot(s, a);
}

/**
    value indexing:
    Context stack S = /C0.C1.C2...Cn where C0 an entrelacs
    key = /C0.C1.C2...Cn.Slot = /S.Slot
*/
void xls_set(Arrow s, Arrow slot, Arrow value) {
    xls_unset(s, slot);
    xls_root(a(s, slot), value);
}

void xls_unset(Arrow s, Arrow slot) {
    Arrow context = arrowMaybe(s, slot);
    if (isEve(context)) return;
    xls_reset(context);
}

Arrow xls_get(Arrow s, Arrow slot) {
    Arrow value = xl_Eve();
    Arrow key = arrowMaybe(s, slot);
    if (isEve(key))
        return value;

    XLEnum e = xl_childrenOf(key);
    while (xl_enumNext(e)) {
        Arrow keyValue = xl_enumGet(e);
        if (tailOf(keyValue) != key) continue; // incoming arrows are ignored
        value = headOf(keyValue);
        if (xls_isRooted(key, value))
            break;
    }
    xl_freeEnum(e);
    return value;
}

void xls_reset(Arrow s) {
    XLEnum e = xl_childrenOf(s);
    Arrow next = (xl_enumNext(e) ? xl_enumGet(e) : EVE);
    do {
        Arrow child = next;
        next = (xl_enumNext(e) ? xl_enumGet(e) : EVE);

        if (tailOf(child) == s) {
            if (isRooted(child) == child) {
                xls_unroot(s, headOf(child));
            }
            xls_reset(child);
        }

    } while (next);

    xl_freeEnum(e);
}

Arrow xls_close(Arrow s) {
   xls_reset(s);
   xls_unroot(tailOf(s), headOf(s));
   return s;
}

static Arrow fromUrl(Arrow locked, unsigned char* url, char** urlEnd, int locateOnly) {
    DEBUGPRINTF("BEGIN fromUrl(%06x, '%s')\n", locked, url);
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
            // Security check: no way to forge %x ref which doesn't belong to the current session
            if (xl_tailOf(sa) == locked) {
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
        tail = fromUrl(locked, url + 1, &tailUriEnd, locateOnly);
        if (!tailUriEnd) {
            *urlEnd = NULL;
            break;
        }

        char* headURIStart = *tailUriEnd == '.' ? tailUriEnd + 1 : tailUriEnd;
        head = fromUrl(locked, headURIStart, &headUriEnd, locateOnly);
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

    DEBUGPRINTF("END fromUrl(%06x, '%s') = %06x\n", locked, url, a);
    return a;
}

static Arrow url(Arrow locked, char *url, int locateOnly) {
    char c, *urlEnd;
    Arrow a = fromUrl(locked, url, &urlEnd, locateOnly);
    if (!urlEnd) return xl_Eve();


    while ((c = *urlEnd) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
           // white spaces are tolerated and ignored here
           urlEnd++;
    }

    if (*urlEnd) {
       DEBUGPRINTF("urlEnd = >%s<\n", urlEnd);

       Arrow b = fromUrl(locked, urlEnd, &urlEnd, locateOnly);
       if (!urlEnd) return xl_Eve();
       a = (locateOnly ? arrowMaybe(a, b) : arrow(a, b)); // TODO: document actual design
    }

    return a;
}

Arrow xls_url(Arrow session, char* aUrl) {
    Arrow locked = a(session, tag("locked"));
    return url(locked, aUrl, 0);
}

Arrow xls_urlMaybe(Arrow session, char* aUrl) {
    Arrow locked = a(session, tag("locked"));
    return url(locked, aUrl, 1);
}

static char* toURL(Arrow locked, Arrow a, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        xls_root(locked, a);
        Arrow sa = arrow(locked, a);
        sprintf(url, "%%%06x", (int)sa);
        *l = 7;
        return url;
    } else if (xl_typeOf(a) == XL_ARROW) { // TODO tuple
        uint32_t l1, l2;
        char *tailUrl = toURL(locked, xl_tailOf(a), depth - 1, &l1);
        char *headUrl = toURL(locked, xl_headOf(a), depth - 1, &l2);
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
    Arrow locked = a(session, tag("locked"));
    return toURL(locked, a, depth, &l);
}
