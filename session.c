
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
   /* $session =  /$s/session/$agent+$uuid */
   Arrow session = xls_root(s, a(atom("session"), a(agent, uuid)));

   return session;
}

Arrow xls_sessionMaybe(Arrow s, Arrow agent, Arrow uuid) {
   Arrow agentUuidMaybe = pairMaybe(agent, uuid);
   if (!agentUuidMaybe)
       return EVE;

   Arrow sessionMaybe = pairMaybe(atom("session"), agentUuidMaybe);
   if (!sessionMaybe)
       return EVE;

   Arrow rootedSession = xls_isRooted(EVE, sessionMaybe);
   if (!rootedSession)
       return EVE;

   return rootedSession;
}

/* TODO: FIXME: We could parameter a maximal depth of deep rooting (meta-level)
   when reached, we could break the recursion and root directly to context
   e.g. deepRoot(/s0+s1+s2+s3, a, depth=1) <=> root(/s0+s1+s2/s3+a) s0/s1/s2
   NO. IT DOESN'T WORK UNLESS ONE SETS UP COUNTERS OR TTL
   */
Arrow deepRoot(Arrow c, Arrow e) {
    Arrow parent = tailOf(c);
    if (parent != c) {
        return deepRoot(parent, a(headOf(c), e));
    } else if (c != EVE) {
        return root(pair(c, e));
    } else {
        return root(e);
    }
}

/* root in a context */
Arrow xls_root(Arrow c, Arrow e) {
    TRACEPRINTF("xls_root(%O,%O)", c, e);
    deepRoot(c, e);
    return root(a(c, e));
}

#if 0
// not used
Arrow deepIsRooted(Arrow c, Arrow e) {
    Arrow parent = tailOf(c);
    if (parent != c) {
        return deepIsRooted(parent, pair(headOf(c),e));
    } else if (c != EVE) {
        return isRooted(pair(c, e));
    } else {
        return isRooted(e);
    }
}
#endif

Arrow xls_isRooted(Arrow c, Arrow e) {
    Arrow m = pairMaybe(c, e);
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
    TRACEPRINTF("xls_unroot(%O,%O)", c, e);
    deepUnroot(c, e);
    Arrow u = unroot(a(c, e));
    // DEBUGPRINTF("xls_unroot(%O,%O) done.", c, e);
    return u;
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

/** traditional edge storage
 *  root $destination in /$c+$source context path
 */
Arrow xls_link(Arrow c, Arrow source, Arrow destination) {
    return xls_root(a(c, source), destination);
}

/** traditional edge removal
 *  unroot $destination from /$c+$source context path
 */
Arrow xls_unlink(Arrow c, Arrow source, Arrow destination) {
    return xls_unroot(a(c, source), destination);
}


/** traditional "set-key-value".

     1) unroot any arrow from $c+$key context path
     2) root $value in /$c+$key context path
*/
Arrow xls_set(Arrow c, Arrow key, Arrow value) {
    TRACEPRINTF("xls_set(%O,%O,%O)", c, key, value);

    xls_unset(c, key);
    return xls_link(c, key, value);
}

/** traditional "unset-key".

    reset $c+$key context path
*/
void xls_unset(Arrow c, Arrow key) {
    Arrow slotContext = pairMaybe(c, key);
    if (slotContext == EVE) return;

    xls_reset(slotContext);
}

/** traditional "get-key".
    returns the rooted arrow in $c+$key context path
    (if several arrows, only one is returned)
*/
static Arrow get(Arrow c, Arrow key) {
    Arrow value = NIL;
    Arrow keyContext = pairMaybe(c, key);
    if (keyContext != EVE) {

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
    }

    if (value != NIL)
        return value;

    if (tailOf(c) == c)
        return NIL;

    return get(tailOf(c), key);
}

Arrow xls_get(Arrow c, Arrow key) {
    Arrow value = get(c, key);
    TRACEPRINTF("xls_get(%O,%O) returned %O", c, key, value);
    return value;
}

/** close a session $s */
Arrow xls_close(Arrow s) {
   /* $session =  /$s/session/$agent+$uuid */
   xls_reset(s);
   xls_unroot(tailOf(s), headOf(s));
   return s;
}

static Arrow _fromUrl(Arrow context, unsigned char* url, char** urlEnd, int locateOnly) {
    TRACEPRINTF("BEGIN _fromUrl(%06x, '%s')", context, url);
    Arrow a = EVE;

    char c = url[0];
    if (c <= 32) { // Any control-caracters/white-spaces are considered as URI break
        a = EVE;
        *urlEnd = url;
    } else switch (c) {
            case '+': // Eve
                a = EVE;
                *urlEnd = url;
                break;
            case '/':
            { // ARROW
                char *tailUrlEnd, *headUrlEnd;
                Arrow tail, head;
                tail = _fromUrl(context, url + 1, &tailUrlEnd, locateOnly);
                if (!tailUrlEnd) {
                    a = tail; // NIL or EVE
                    *urlEnd = NULL;
                    break;
                }
                
                if (!*tailUrlEnd) { // no more char
                    a = tail;
                    *urlEnd = tailUrlEnd;
                    break;
                }

                char* headUrlStart = *tailUrlEnd == '+' ? tailUrlEnd + 1 : tailUrlEnd;
                head = _fromUrl(context, headUrlStart, &headUrlEnd, locateOnly);
                if (!headUrlEnd) {
                    a = head;
                    *urlEnd = NULL;
                    break;
                }
                
                a = (locateOnly ? xl_pairMaybe(tail, head) : xl_pair(tail, head));
                if (a == EVE || a == NIL) {
                    *urlEnd = NULL;
                    break;
                }
                
                *urlEnd = headUrlEnd;
                break;
            }
            case '$':
            {
                if (url[1] != 'H') {
                    int ref = 0;
                    sscanf(url + 1, "%x", &ref);

                    Arrow sa = ref;
                    // Security check: no way to resolve a %x ref which hasn't been forged in the context
                    if (xl_isPair(sa) && xl_tailOf(sa) == context) {
                        a = xl_headOf(sa);
                        *urlEnd = url + 7;
                    } else {
                        a = NIL;
                        *urlEnd = NULL;
                    }
                    break;
                }
            }
            default:
            { // TAG, BLOB
                uint32_t urlLength = 0;

                while ((c = url[urlLength]) > 32 && c != '+' && c != '/')
                    urlLength++;
                assert(urlLength);

                a = (locateOnly ? xl_urinMaybe(urlLength, url) : xl_urin(urlLength, url));
                if (a == NIL || a == EVE) {
                    *urlEnd = NULL;
                    break;
                }
                
                *urlEnd = url + urlLength;
                break;
            }
        }

    TRACEPRINTF("END _fromUrl(%06x, '%s') = %06x", context, url, a);
    return a;
}

static char* skeepSpacesAndOnePlus(char* urlEnd) {
    char c;
    while ((c = *urlEnd) && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
        // white spaces are tolerated and ignored here
        urlEnd++;
    }
    if (c == '+') urlEnd++;
    return urlEnd;
}

static Arrow fromUrl(Arrow context, char *url, int locateOnly) {
    TRACEPRINTF("BEGIN fromUrl(%06x, '%s', %d)", context, url, locateOnly);
    char c, *nextUrl;
    Arrow a = _fromUrl(context, url, &nextUrl, locateOnly);
    if (!nextUrl) return a; // NIL or EVE

    nextUrl = skeepSpacesAndOnePlus(nextUrl);
    while (*nextUrl) {
        DEBUGPRINTF("nextUrl = >%s<", nextUrl);
        Arrow b = _fromUrl(context, nextUrl, &nextUrl, locateOnly);
        if (!nextUrl) return b; // NIL or EVE
        
        a = (locateOnly ? pairMaybe(a, b) : pair(a, b)); // TODO: document actual design
        if (a == EVE) return EVE;
        
        nextUrl = skeepSpacesAndOnePlus(nextUrl);
    }

    TRACEPRINTF("END fromUrl(%06x, '%s', %d) = %O", context, url, locateOnly, a);
    return a;
}

Arrow xls_url(Arrow s, char* aUrl) {
    Arrow locked = a(s, atom("locked"));
    return fromUrl(locked, aUrl, 0);
}

Arrow xls_urlMaybe(Arrow s, char* aUrl) {
    Arrow locked = a(s, atom("locked"));
    return fromUrl(locked, aUrl, 1);
}

static char* toURL(Arrow context, Arrow e, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        Arrow sa = xls_root(context, e);
        sprintf(url, "$%06x", (int)sa);
        *l = 7;
        return url;
    } else if (xl_isPair(e)) { // TODO tuple
        uint32_t l1, l2;
        char *tailUrl = toURL(context, xl_tailOf(e), depth - 1, &l1);
        char *headUrl = toURL(context, xl_headOf(e), depth - 1, &l2);
        char *url = malloc(2 + l1 + l2 + 1) ;
        assert(url);
        sprintf(url, "/%s+%s", tailUrl, headUrl);
        free(tailUrl);
        free(headUrl);
        *l = 2 + l1 + l2;
        return url;
    } else {
        return uriOf(e, l);
    }
}


char* xls_urlOf(Arrow s, Arrow e, int depth) {
    uint32_t l;
    Arrow locked = a(s, atom("locked"));

    return toURL(locked, e, depth, &l);
}

/** returns a list-arrow of all rooted arrows within context path "/$c+$key"
*/
Arrow xls_partnersOf(Arrow c, Arrow key) {
    Arrow value = NIL;
    Arrow keyContext = pairMaybe(c, key);
    Arrow list = EVE;
    if (keyContext != EVE) {
        XLEnum enumChildren = xl_childrenOf(keyContext);
        while (xl_enumNext(enumChildren)) {
            Arrow keyValue = xl_enumGet(enumChildren);
            if (tailOf(keyValue) != keyContext) continue; // incoming arrows are ignored
            if (isRooted(keyValue)) {
                value = headOf(keyValue);
            }
            list = a(value, list);
        }
        xl_freeEnum(enumChildren);
    }

    return list;
}
