
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#define LOG_CURRENT LOG_SESSION
#include "log.h"
#include "session.h"


Arrow xls_session(Arrow s, Arrow agent, Arrow uuid) {
   /* $session =  /$s/session/$agent.$uuid */
   Arrow session = xls_root(s, a(tag("session"), a(agent, uuid)));

   return session;
}

Arrow xls_sessionMaybe(Arrow s, Arrow agent, Arrow uuid) {
   Arrow agentUuidMaybe = arrowMaybe(agent, uuid);
   if (!agentUuidMaybe)
       return EVE;

   Arrow sessionMaybe = arrowMaybe(tag("session"), agentUuidMaybe);
   if (!sessionMaybe)
       return EVE;

   Arrow rootedSession = xls_isRooted(EVE, sessionMaybe);
   if (!rootedSession)
       return EVE;

   return rootedSession;
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
    } else if (c != EVE) {
        return root(arrow(c, e));
    } else {
        return root(e);
    }
}

/* root in a context */
Arrow xls_root(Arrow c, Arrow e) {
    DEBUGPRINTF("Rooting %O to %O", e, c);
    deepRoot(c, e);
    return root(a(c, e));
}

#if 0
// not used
Arrow deepIsRooted(Arrow c, Arrow e) {
    Arrow parent = tailOf(c);
    if (parent != c) {
        return deepIsRooted(parent, arrow(headOf(c),e));
    } else if (c != EVE) {
        return isRooted(arrow(c, e));
    } else {
        return isRooted(e);
    }
}
#endif

Arrow xls_isRooted(Arrow c, Arrow e) {
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
    DEBUGPRINTF("Unrooting %O from %O", e, c);
    deepUnroot(c, e);
    return unroot(a(c, e));
}

/** reset a context
  Recursivly unroot any rooted arrow under a given context
 */
void xls_reset(Arrow c) {
    if (c == EVE) return;

    XLEnum childrenEnum = xl_childrenOf(c);
    Arrow next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : EVE);
    while (next != EVE) {
        Arrow child = next;
        next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : EVE);

        if (tailOf(child) == c) { // Only outgoing arrow
            xls_reset(child);
            if (xl_isRooted(child) != EVE) {
                xls_unroot(c, headOf(child));
            }
        }
    }

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

    Arrow value = NIL;
    XLEnum enumChildren = xl_childrenOf(keyContext);
    while (xl_enumNext(enumChildren)) {
        Arrow keyValue = xl_enumGet(enumChildren);
        if (tailOf(keyValue) != keyContext) continue; // incoming arrows are ignored
        if (isRooted(keyValue)) {
            value = headOf(keyValue);
            break;
        }
    }
    xl_freeEnum(enumChildren);

    if (value == NIL && tailOf(c) != c)
        return xls_get(tailOf(c), key);
    else if (value == NIL)
        return EVE;
    else
        return value;
}

/** close a session $s */
Arrow xls_close(Arrow s) {
   /* $session =  /$s/session/$agent.$uuid */
   xls_reset(s);
   xls_unroot(tailOf(s), headOf(s));
   return s;
}

static Arrow _fromUrl(Arrow context, unsigned char* url, char** urlEnd, int locateOnly) {
    DEBUGPRINTF("BEGIN fromUrl(%06x, '%s')", context, url);
    Arrow a = EVE;

    char c = url[0];
    if (c <= 32) { // Any control-caracters/white-spaces are considered as URI break
        a = EVE;
        *urlEnd = url;
    } else switch (c) {
    case '.': // Eve
        a = EVE;
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
            if (a == NIL) {
                *urlEnd = NULL;
            } else if (a == EVE) {
                *urlEnd = NULL;
            } else {
                *urlEnd = url + urlLength;
            }
            break;
        } else {
            int ref;
            sscanf(url + 1, "%x", &ref);
            Arrow sa = ref;
            // Security check: no way to resolve a %x ref which hasn't been forged in the context
            if (xl_tailOf(sa) == context) {
                a = xl_headOf(sa);
                *urlEnd = url + 7;
            } else {
                a = NIL;
                *urlEnd = NULL;
            }
        }
    }
    case '/': { // ARROW
        char *tailUrlEnd, *headUrlEnd;
        Arrow tail, head;
        tail = _fromUrl(context, url + 1, &tailUrlEnd, locateOnly);
        if (!tailUrlEnd) {
            a = tail; // NIL or EVE
            *urlEnd = NULL;
        } else if (!*tailUrlEnd) { // no more char
            a = tail;
            *urlEnd = tailUrlEnd;
        } else {
            char* headUrlStart = *tailUrlEnd == '.' ? tailUrlEnd + 1 : tailUrlEnd;
            head = _fromUrl(context, headUrlStart, &headUrlEnd, locateOnly);
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
        if (a == NIL) {
            *urlEnd = NULL;
        } else if (a == EVE) {
            *urlEnd = NULL;
        } else {
            *urlEnd = url + urlLength;
        }
        break;
    }
    }

    DEBUGPRINTF("END fromUrl(%06x, '%s') = %06x", context, url, a);
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

static Arrow fromUrl(Arrow context, char *url, int locateOnly) {
    char c, *urlEnd;
    Arrow a = _fromUrl(context, url, &urlEnd, locateOnly);
    if (!urlEnd) return a; // NIL or EVE

    urlEnd = skeepSpacesAndOneDot(urlEnd);
    while (*urlEnd) {
        DEBUGPRINTF("urlEnd = >%s<", urlEnd);
        Arrow b = _fromUrl(context, urlEnd, &urlEnd, locateOnly);

        if (!urlEnd) return b; // NIL or EVE
        urlEnd = skeepSpacesAndOneDot(urlEnd);
        if (a == EVE && b == EVE) continue;
        a = (locateOnly ? arrowMaybe(a, b) : arrow(a, b)); // TODO: document actual design
        if (a == EVE) return EVE;
    }

    return a;
}

Arrow xls_url(Arrow s, char* aUrl) {
    Arrow locked = a(s, tag("locked"));
    return fromUrl(locked, aUrl, 0);
}

Arrow xls_urlMaybe(Arrow s, char* aUrl) {
    Arrow locked = a(s, tag("locked"));
    return fromUrl(locked, aUrl, 1);
}

static char* toURL(Arrow context, Arrow e, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        Arrow sa = xls_root(context, e);
        sprintf(url, "%%%06x", (int)sa);
        *l = 7;
        return url;
    } else if (xl_typeOf(e) == XL_ARROW) { // TODO tuple
        uint32_t l1, l2;
        char *tailUrl = toURL(context, xl_tailOf(e), depth - 1, &l1);
        char *headUrl = toURL(context, xl_headOf(e), depth - 1, &l2);
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
    Arrow locked = a(s, tag("locked"));

    return toURL(locked, e, depth, &l);
}
