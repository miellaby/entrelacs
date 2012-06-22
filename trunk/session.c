
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#include "log.h"
#include "session.h"


Arrow xls_session(Arrow s, Arrow agent, Arrow uuid) {
   /* $session =  /$s.sessions./$agent.$uuid */
   Arrow session = xls_root(a(s, tag("sessions")), a(agent, uuid));

   return session;
}

/* TODO: FIXME: We could parameter a maximal depth of deep rooting (meta-level)
   when reached, we could break the recursion and root directly to context
   e.g. deepRoot(/s0.s1.s2.s3, a, depth=1) <=> root(/s0.s1.s2/s3.a) s0/s1/s2
   NO. IT DOESN'T WORK UNLESS ONE SETS UP COUNTERS OR TTL
   */
Arrow deepRoot(Arrow c, Arrow e) {
    Arrow parent = tailOf(c);
    if (parent != c) {
        return deepRoot(parent, a(headOf(c), e));
    } else if (c) {
        return root(arrow(c, e));
    } else {
        return root(e);
    }
}

/* root in a context */
Arrow xls_root(Arrow c, Arrow e) {
    if (c == EVE)
        return root(e);

    deepRoot(c, e);
    return root(a(c, e));
}

#if 0
// not used
Arrow deepIsRooted(Arrow c, Arrow e) {
    Arrow parent = tailOf(c);
    if (parent != c) {
        return deepIsRooted(parent, arrow(headOf(c),e));
    } else if (s != EVE) {
        return isRooted(arrow(c, e));
    } else {
        return isRooted(e);
    }
}
#endif

Arrow xls_isRooted(Arrow c, Arrow e) {
    if (c == EVE)
        return isRooted(e);

    Arrow m = arrowMaybe(c, e);
    if (m == EVE || !isRooted(m))
        return EVE;

    return m;
}

void deepUnroot(Arrow c, Arrow e) {
    Arrow parent = tailOf(c);

    if (parent != c) {
        deepUnroot(parent, a(headOf(c), e));
    } else if (c != EVE) {
        unroot(a(c, e));
    } else {
        unroot(e);
    }
}

Arrow xls_unroot(Arrow c, Arrow e) {
    if (c == EVE)
        return unroot(e);

    deepUnroot(c, e);
    return unroot(a(c, e));
}

/** reset a context
  Recursivly unroot any rooted arrow under a given context
 */
void xls_reset(Arrow c) {
    XLEnum childrenEnum = xl_childrenOf(c);
    Arrow next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : EVE);
    do {
        Arrow child = next;
        next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : EVE);

        if (tailOf(child) == c) { // Only outgoing arrow
            if (isRooted(child) != EVE) {
                xls_unroot(c, headOf(child));
            }
            xls_reset(child);
        }

    } while (next);

    xl_freeEnum(childrenEnum);
}

/** traditional "set-key-value".

     1) unroot any arrow from $c.$key context path
     2) root $value in /$c.$key context path
*/
Arrow xls_set(Arrow c, Arrow key, Arrow value) {
    xls_unset(c, key);
    return xls_root(a(c, key), value);
}

/** traditional "unset-key".

    reset $c.$key context path
*/
void xls_unset(Arrow c, Arrow key) {
    Arrow slotContext = arrowMaybe(c, key);
    if (slotContext == EVE) return;

    xls_reset(slotContext);
}

/** traditional "get-key".
    returns the rooted arrow in $c.$key context path
    (if several arrows, only one is returned)
*/
Arrow xls_get(Arrow c, Arrow key) {
    Arrow keyContext = arrowMaybe(c, key);
    if (keyContext == EVE)
        return EVE;

    Arrow value = EVE;
    XLEnum enumChildren = xl_childrenOf(keyContext);
    while (xl_enumNext(enumChildren)) {
        Arrow keyValue = xl_enumGet(enumChildren);
        if (tailOf(keyValue) != keyContext) continue; // incoming arrows are ignored
        value = headOf(keyValue);
        if (isRooted(keyValue))
            break;
    }
    xl_freeEnum(enumChildren);
    return value;
}

/** close a session $s */
Arrow xls_close(Arrow s) {
   /* $session =  /$s.sessions./$agent.$uuid */
   xls_reset(s);
   xls_unroot(tailOf(s), headOf(s));
   return s;
}

static Arrow _fromUrl(Arrow s, unsigned char* url, char** urlEnd, int locateOnly) {
    DEBUGPRINTF("BEGIN fromUrl(%06x, '%s')\n", s, url);
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

            // TODO a refaire
            char *sub = malloc(urlLength + 1);
            strncpy(sub, url, urlLength);
            sub[urlLength]='\0';
            a = (locateOnly ? xl_uriMaybe(sub) : xl_uri(sub));
            free(sub);

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
            // Security check: no way to resolve a %x ref which hasn't been forged in the current session
            if (xl_tailOf(sa) == s) {
                a = xl_headOf(sa);
                *urlEnd = url + 7;
            } else {
                *urlEnd = NULL;
            }
        }
    }
    case '/': { // ARROW
        char *tailUrlEnd, *headUrlEnd;
        Arrow tail, head;
        tail = _fromUrl(s, url + 1, &tailUrlEnd, locateOnly);
        if (!tailUrlEnd) {
            *urlEnd = NULL;
        } else if (!*tailUrlEnd) {
            a  = tail;
            *urlEnd = tailUrlEnd;
        } else {
            char* headUrlStart = *tailUrlEnd == '.' ? tailUrlEnd + 1 : tailUrlEnd;
            head = _fromUrl(s, headUrlStart, &headUrlEnd, locateOnly);
            if (!headUrlEnd) {
                *urlEnd = NULL;
            } else {
                a = (locateOnly ? xl_arrowMaybe(tail, head) : xl_arrow(tail, head));
                if (a == EVE && !(tail == EVE && head == EVE)) {
                    *urlEnd = NULL;
                } else {
                    *urlEnd = headUrlEnd;
                }
            }
        }
        break;
    }
    default: { // TAG
        unsigned char c;
        uint32_t urlLength = 0;

        while ((c = url[urlLength]) > 32  && c != '.' && c != '/')
            urlLength++;
        assert(urlLength);

        // TODO a refaire
        char *sub = malloc(urlLength + 1);
        strncpy(sub, url, urlLength);
        sub[urlLength]='\0';
        a = (locateOnly ? xl_uriMaybe(sub) : xl_uri(sub));
        free(sub);

        if (xl_isEve(a)) {
            *urlEnd = NULL;
        } else {
            *urlEnd = url + urlLength;
        }
        break;
    }
    }

    DEBUGPRINTF("END fromUrl(%06x, '%s') = %06x\n", s, url, a);
    return a;
}

static char* skeepSpacesAndOneDot(char* urlEnd) {
    char c;
    while ((c = *urlEnd) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
        // white spaces are tolerated and ignored here
        urlEnd++;
    }
    if (*urlEnd == '.') urlEnd++;
    return urlEnd;
}

static Arrow fromUrl(Arrow s, char *url, int locateOnly) {
    char c, *urlEnd;
    Arrow a = _fromUrl(s, url, &urlEnd, locateOnly);
    if (!urlEnd) return EVE;

    urlEnd = skeepSpacesAndOneDot(urlEnd);
    while (*urlEnd) {
        DEBUGPRINTF("urlEnd = >%s<\n", urlEnd);
        Arrow b = _fromUrl(s, urlEnd, &urlEnd, locateOnly);

        if (!urlEnd) return EVE; // Not assimilated
        urlEnd = skeepSpacesAndOneDot(urlEnd);

        a = (locateOnly ? arrowMaybe(a, b) : arrow(a, b)); // TODO: document actual design
    }

    return a;
}

Arrow xls_url(Arrow s, char* aUrl) {
    return fromUrl(s, aUrl, 0);
}

Arrow xls_urlMaybe(Arrow s, char* aUrl) {
    return fromUrl(s, aUrl, 1);
}

static char* toURL(Arrow s, Arrow e, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        Arrow sa = xls_root(s, e);
        sprintf(url, "%%%06x", (int)sa);
        *l = 7;
        return url;
    } else if (xl_typeOf(e) == XL_ARROW) { // TODO tuple
        uint32_t l1, l2;
        char *tailUrl = toURL(s, xl_tailOf(e), depth - 1, &l1);
        char *headUrl = toURL(s, xl_headOf(e), depth - 1, &l2);
        char *url = malloc(2 + l1 + l2 + 1) ;
        assert(url);
        sprintf(url, "/%s.%s", tailUrl, headUrl);
        free(tailUrl);
        free(headUrl);
        *l = 2 + l1 + l2;
        return url;
    } else {
        char* url = uriOf(e);
        *l = strlen(url);
        return url;
    }
}


char* xls_urlOf(Arrow s, Arrow e, int depth) {
    uint32_t l;
    return toURL(s, e, depth, &l);
}
