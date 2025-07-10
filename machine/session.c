
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#define LOG_CURRENT LOG_SESSION
#include "log/log.h"
#include "machine/session.h"
#include "space/serial.h"

Arrow xs_open(char* agent) {
   /* $session =  /$s/session/$agent+$uuid */
   Arrow uuid = xl_anonymous();
   Arrow session = xs_root(Eve, A(atom("session"), A(atom(agent), uuid)));

   return session;
}

static ArrowValue eveValue = { 0 };
static Arrow eve = &eveValue;

Arrow xs_eve() {
    return eve;
}

static Arrow xs_arrow(Address id) {
    // TODO je pense qu'on peut faire des Arrow avec id non-résolues (type == UNDEF?)
    if (!id || id == XL_EVE) {
        return eve;
    }
    Arrow a = malloc(sizeof(ArrowValue));
    memset(a, 0, sizeof(ArrowValue));
    a->hash = xs_getHash(id);
    a->id = id;
    if (xl_isAtom(id)) {
        a->def.atom.raw = xl_memOf(id, &a->def.atom.size);
        a->type = XS_ATOM;
    } else {
        a->def.pair.tail = xs_arrow(xl_tailOf(id));
        a->def.pair.head = xs_arrow(xl_headOf(id));
        a->type = XS_PAIR;
    }       
}

Arrow xs_pair(Arrow tail, Arrow head) {
    Arrow a = malloc(sizeof(ArrowValue));
    memset(a, 0, sizeof(ArrowValue));
    a->def.pair.head = head;
    a->def.pair.tail = tail;
    a->type = XS_PAIR;
}

Arrow xs_atom(char* s) {
    size_t size = strlen(s);
    Arrow a = malloc(sizeof(ArrowValue));
    memset(a, 0, sizeof(ArrowValue));
    a->def.atom.size = size + 1;
    a->def.atom.raw = (uint8_t*) malloc(a->def.atom.size);
    assert(a->def.atom.raw);
    memcpy(a->def.atom.raw, s, a->def.atom.size);
    a->type = XS_ATOM;
}

Arrow xs_atomn(size_t size, uint8_t* s) {
    Arrow a = malloc(sizeof(ArrowValue));
    memset(a, 0, sizeof(ArrowValue));
    a->def.atom.size = size;
    a->def.atom.raw = (uint8_t*) malloc(a->def.atom.size);
    assert(a->def.atom.raw);
    memcpy(a->def.atom.raw, s, a->def.atom.size);
    a->type = XS_ATOM;
}

ArrowType xs_getType(Arrow a) {
    return a ? a->type : XS_EVE;
}

uint32_t xs_getHash(Arrow a) {
    if (!a->hash) {
        if (a->type == XS_ATOM) {
            a->hash = hash_raw(a->def.atom.raw, a->def.atom.size);
        } else if (a->type == XS_PAIR) {
            uint32_t hash_tail = xs_getHash(a->def.pair.tail);
            uint32_t hash_head = xs_getHash(a->def.pair.head);
            a->hash = hash_pair(hash_tail, hash_head);
        } else {
            a->hash = hash_eve();
        }
    }
    return a->hash;
}

Arrow xs_getTail(Arrow a) {
    if (!a || a->type != XS_PAIR) {
        return eve;
    }
    return a->def.pair.tail;
}

Arrow xs_getHead(Arrow a) {
    if (!a || a->type != XS_PAIR) {
        return eve;
    }
    return a->def.pair.tail;
}

Arrow xs_getId(Arrow a) {
    return a ? a->id : XL_EVE;
}

Arrow xs_resolve(Arrow a) {
    if (!a) {
        return eve;
    }
    if (a == eve || a->id != 0) {
        return a;
    }

    uint32_t hash = xs_getHash(a);
    if (a->type == XS_PAIR) {
        // FIXME essayer de tester immédiatement un candidat par le hash
        // il faut inventer xl_probeByHash(type, hash, callback) qui renvoie toutes les paires avec un hash
        Arrow tail = xs_resolve(a->def.pair.tail);
        Arrow head = xs_resolve(a->def.pair.head);
        if ((a->def.pair.tail == eve || tail->id)
            && (a->def.pair.head == eve || head->id)) {
            a->id = xl_pairMaybe(tail->id, head->id);
        }
    } else {
        a->id = xl_atomnMaybe(a->def.atom.size, a->def.atom.raw);
    }
    return a;
}

Arrow xs_assimilate(Arrow a) {
    if (!a) {
        return eve;
    }
    if (a == eve || a->id != 0) {
        return a;
    }
    uint32_t hash = xs_getHash(a);
    // FIXME essayer de trouver un candidat par le hash
    
    if (a->type == XS_PAIR) {
        Arrow tail = xs_assimilate(a->def.pair.tail);
        Arrow head = xs_assimilate(a->def.pair.head);
        if ((a->def.pair.tail == eve || tail->id)
            && (a->def.pair.head == eve || head->id)) {
            a->id = xl_pair(tail->id, head->id);
        }
    } else {
        a->id = xl_atomn(a->def.atom.size, a->def.atom.raw);
    }
    return a;
}

int xs_isEve(Arrow* a) {
    return a && a == eve; // eve est automatiquement assimilée
}

int xs_isAtom(Arrow* a) {
    return a && xs_getType(a) == XS_ATOM;
}

int xs_isPair(Arrow* a) {
    return a && xs_getType(a) == XS_PAIR;
}

int xs_isKnown(Arrow a) {
    if (!a) {
        return 0;
    } else if (a == eve) {
        return 1;
    } else {
        return xs_resolve(a)->id != XL_EVE;
    }
}

Arrow xs_isRooted(Arrow session, Arrow a) {
    Arrow sa = (session == eve ? a : xs_pair(session, a));
    Arrow resolved = xs_resolve(sa);
    return resolved->id != XL_EVE ? xl_isRooted(resolved->id) : eve;
}

Arrow xs_equal(Arrow a, Arrow b) {
    if (!a || !b) {
        return eve;
    } else if (a == eve) {
        return eve;
    } else if (a == b) {
        return a;
    } else if (a->id && b->id) {
        return a->id == b->id ? a : eve;
    } else if (a->type != b->type) {
        return eve;
    } else if (a->id) {
        return xs_resolve(b)->id == a->id ? a : eve;
    } else if (b->id) {
        return xs_resolve(a)->id == b->id ? a : eve;
    } else if (a->type == XS_ATOM) {
        if (a->def.atom.size != b->def.atom.size) {
            return eve;  
        } else if (a->def.atom.raw == b->def.atom.raw) {
            return a;
        } else {
            return memcmp(a->def.atom.raw, b->def.atom.raw, a->def.atom.size) == 0 ? a : eve;
        }
    } else if (a->type == XS_PAIR) {
        Arrow tail_a = xs_getTail(a);
        Arrow head_a = xs_getHead(a);
        Arrow tail_b = xs_getTail(b);
        Arrow head_b = xs_getHead(b);
        int same_tail = (tail_a == eve ? tail_b == eve : xs_equal(tail_a, tail_b) != eve);
        int same_head = (head_a == eve ? head_b == eve : xs_equal(head_a, head_b) != eve); 
        return same_tail && same_head ? a : eve;
    } else {
        return eve;
    }
}

// TODO should be non-deterministic
Arrow xs_atom_session() {
    static ArrowValue arrow = {
        .hash = 0,
        .id = 0,
        .def = {
            .atom = {
                .raw = "session",
                .size = 7
            }
        },
        .type = XS_ATOM
    };
    return &arrow;
}

char* xs_session_getId(Arrow session) {
    if (xs_getTail(session) != xs_atom_session()) {
        return NULL;
    } else {
        Arrow uuid = xs_getHead(xs_getHead(session));
        if (xs_isAtom(uuid)) {
            return xs_getStr(uuid);
        } else {
            return NULL;
        }
    }
}

Arrow xs_getSession(char* agent, char* uuid) {
    
   Arrow agentUuidMaybe = pairMaybe(atomMaybe(agent), atomMaybe(uuid));

   Arrow sessionMaybe = pairMaybe(xs_atom_session(), agentUuidMaybe);

   Arrow rootedSession = xs_isRooted(eve, sessionMaybe);
   if (!rootedSession)
       return eve;

   return rootedSession;
}

char *xs_getStr(Arrow a) {
    if (!a || a->type != XS_ATOM) {
        return NULL;
    }
    char* str = malloc(a->def.atom.size + 1);
    assert(str);
    memcpy(str, a->def.atom.raw, a->def.atom.size);
    str[a->def.atom.size] = '\0';
    return str;
}

char *xs_getMem(Arrow a, size_t *size) {
    if (!a || a->type != XS_ATOM) {
        return NULL;
    }
    if (size) {
        *size = a->def.atom.size;
    }
    char* raw = malloc(a->def.atom.size + 1);
    assert(raw);
    memcpy(raw, a->def.atom.raw, a->def.atom.size);
    return raw;
}


// get an URI path corresponding to an arrow
char* xs_toURI(Arrow a, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (a == eve) { // Eve is identified by an empty path
        // allocate and return an empty string
        char *s = (char*) malloc(1);
        assert(s);
        s[0] = '\0';
        if (l) *l = 0; // if asked, return length = 0
        return s;
    }

    switch (a->type) {
        case XS_ATOM: {
            size_t size = a->def.atom.size;
            char* raw = a->def.atom.raw;
            if (size >= BLOB_MINSIZE) {
                xs_assimilate(a);
                return xl_digestOf(a->id, l);
            } else {
                char *uri = malloc(3 * size + 1); // memory allocation for encoded content
                assert(uri);
                size_t uri_size;
                percent_encode(raw, size, uri, &uri_size);
                uri = realloc(uri, 1 + uri_size);
                if (l) *l = uri_size; // return length if asked
                return uri;
            }
        }
        case XS_PAIR:
        { // concat tail and head identifiers into /tail+head style URI
            uint32_t l1, l2;
            char *tailUri = xs_toURI(xs_getTail(a), &l1);
            if (tailUri == NULL) return NULL;

            char *headUri = xs_toURI(xs_getHead(a), &l2);
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

char* xs_getUri(Arrow a) {
    uint32_t hash = xs_toURI(a, NULL);
}

static Arrow xs_urin(uint32_t size, char *uri, char *uri_size_p) {
    TRACEPRINTF("BEGIN serial_parseUri(%s)", uri);
    if (size == 0) {
        if (uri_size_p) {
            *uri_size_p = 0;
        }
        return eve;
    }

    Arrow a = NULL;
    uint32_t uri_size = NIL;

    char c = uri[0];
    
    if (c <= 32 || !size) { // Any control-caracters/white-spaces are considered as URI break
        a = eve;
        uri_size = 0;
    } else switch (c) {
        case '+': // Eve
            a = eve;
            uri_size = 0;
            break;
        case '$': {
            if (size != NIL && size < DIGEST_SIZE) {
                TRACEPRINTF("xl_urin - not long enough for a digest: %d", size);
                break;
            }
                
            if (uri[1] != 'H') { // Only digest are allowed in URI
                break;
            }

            uri_size = 2;
            while ((size == NIL || uri_size < size) 
                    && (c = uri[uri_size]) > 32 && c != '+' && c != '/')
                uri_size++;
                
            if (uri_size != DIGEST_SIZE) {
                TRACEPRINTF("xl_urin - wrong digest size %d", DIGEST_SIZE);
                break;
            }
            Address id = xl_digestMaybe(uri);
            if (id == NIL) {
                TRACEPRINTF("xl_urin - wrong digest %s", DIGEST_SIZE);
                return NIL;
            }
            a = xs_arrow(id);
            break;
        }
        case '/':
        { // Pair
            uint32_t tailUriSize, headUriSize;
            Address tail, head;
            
            if (size != NAN) size--;
            
            tail = xs_urin(size, uri + 1, &tailUriSize);
            if (tail ==  NIL) { // Issue
                return NIL;
            }
            
            char tailUriEnd = uri[1 + tailUriSize];
            if (tailUriEnd == '\0' || tailUriSize == size /* no more char */) {
                a = tail;
                uri_size = 1 + tailUriSize;
                break;
            }

            char* headUriStart;
            if (tailUriEnd == '+') {
                if (size != NIL)
                    size -= tailUriSize + 1;
                headUriStart = uri + 1 + tailUriSize + 1;
            } else {
                if (size != NIL)
                    size -= tailUriSize;
                headUriStart = uri + 1 + tailUriSize;
            }
            
            head = xs_urin(size, headUriStart, &headUriSize);
            if (head == NIL) { // issue
                return NIL;
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

Arrow xs_uri(char* uri) {
    return !uri ? eve : xs_urin(strlen(uri), uri, 0);   
}

 JEN SUIS LA
// Grand refactoring en cours sur les flèches "temporaires"/"transient"
// xl_... : flèche mappée dans le Arrow Space (ref = adresse)
// xs_... : flèche de travail transient/temporaire (ref = struct *)
//  assimilisation paresseuse
// machine/session/server/repl en xs_...
// d'après https://github.com/miellaby/entrelacs/blob/wiki/ArrowSpaceInterface.md
// à relire: finalement resolve/assimilate ne renvoie pas un singleton mais édite le .id
// à corriger: aucun free!!! compteur de référence?... ca va être trop pénible
// TODO
// - pool vidée à chaque commit donc pas besoin de free
// - session = objet avec attribut pool
// - pool vidée à chaque commit donc pas besoin de free
// - xs_commit
// - xs_close/over
// - xs_root/unroot avec une hiérarchie de contexte
//   (pour l'instant rooter 2 flèches cf. deepRoot dans branche main)
// - xs_isRooted avec une hiérarchie de contexte
// - xs_childrenOf avec une hiérarchie de contexte 
// - le contexte par défaut est la session, on peut en sortir
// - xs_enter rentrer dans un contexte
//   pourrait générer une clé pour sortir (à fournir dans xs_departure)
// - xs_departure accéder au niveau meta/supérieur
// - il existe un clé système, pour sortir de la session et devenir root
// 
// bien finir les flèches temporaires
// ensuite seulement refondre la mémoire d'après
//  https://docs.google.com/document/d/1h8U5LhEVQQN57zU0p97AMdx9LhVloxsMB97i0eB0e24/edit
// et implémenter les nouveaux algos xs_root/childrenOf/...
// ================================================




/// root in a session
Arrow xs_root(Arrow context, Arrow e) {
    INFOPRINTF("xs_root(%O,%O)", context, e);
    Arrow s;
    if (context == eve) {
        s = xs_resolve(e);
    } else {
        s = xs_resolve(xs_pair(context, e));
    }
    if (s->id) {
        return xl_unroot(s->id);
    }
    return s;
}

/// unroot in a context
Arrow xs_unroot(Arrow context, Arrow e) {
    INFOPRINTF("xs_unroot(%O,%O)", context, e);
    Arrow s;
    e = xs_resolve(e);
    if (context == eve) {
        s = e;
    } else {
        s = xs_resolve(xs_pair(context, e));
    }
    if (s->id) {
        xl_unroot(s->id);
    }
    return e;
}

/** reset a context
  Recursivly unroot any rooted arrow under a given context
 */
void xs_reset(Arrow c) {
    if (c == EVE) return;

    XLEnum childrenEnum = xl_childrenOf(c);
    Arrow next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : EVE);
    while (next != EVE) {
        Arrow child = next;
        next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : EVE);

        if (tailOf(child) == c) { // Only outgoing arrow
            xs_reset(child);
            if (xl_isRooted(child) != EVE) {
                xs_unroot(c, headOf(child));
            }
        } else {
            TRACEPRINTF("xs_reset left %O", child);
        }
    }

    xl_enumFree(childrenEnum);
}

/** traditional "set-key-value".

     1) unroot any arrow from $c+$key context path
     2) root $value in /$c+$key context path
*/
Arrow xs_set(Arrow c, Arrow key, Arrow value) {
    INFOPRINTF("xs_set(%O,%O,%O)", c, key, value);

    // unset
    Arrow slotContext = pairMaybe(c, key);
    if (slotContext != EVE) {
       xs_reset(slotContext);
    } else {
        slotContext = xl_pair(c, key);
    }
    
    return xs_root(slotContext, value);
}

/** traditional "unset-key".

    reset $c+$key context path
*/
void xs_unset(Arrow c, Arrow key) {
    INFOPRINTF("xs_unset(%O,%O)", c, key);
    
    Arrow slotContext = pairMaybe(c, key);
    if (slotContext == EVE) return;

    xs_reset(slotContext);
}

/** traditional "get-key".
    returns the rooted arrow in $c+$key context path
    (if several arrows, only one is returned)
*/
static Arrow get(Arrow c, Arrow key) {
    Arrow value = NIL;
    Arrow keyContext = pairMaybe(c, key);
    if (keyContext != EVE) {

        XLEnum childrenEnum = xl_childrenOf(keyContext);
        while (xl_enumNext(childrenEnum)) {
            Arrow keyValue = xl_enumGet(childrenEnum);
            if (tailOf(keyValue) != keyContext) continue; // incoming arrows are ignored
            if (isRooted(keyValue)) {
                value = headOf(keyValue);
                break;
            }
        }
        xl_enumFree(childrenEnum);
    }

    if (value != NIL)
        return value;

    if (tailOf(c) == c)
        return NIL;

    return get(tailOf(c), key);
}

Arrow xs_get(Arrow c, Arrow key) {
    TRACEPRINTF("BEGIN xs_get(%O,%O)", c, key);
    Arrow value = get(c, key);
    TRACEPRINTF("END xs_get(%O,%O) = %O", c, key, value);
    return value;
}


/** returns a list of all children of $a rooted within context path $c
 */ FIXME utiliser le haché
Arrow xs_childrenOf(Arrow c, Arrow a, Arrow list) {
    Arrow value = NIL;
    Arrow contextPair = xl_pairMaybe(c, a);
    if (contextPair == EVE) return list;
    
    XLEnum childrenEnum = xl_childrenOf(contextPair);
    while (xl_enumNext(childrenEnum)) {
        Arrow pair = xl_enumGet(childrenEnum);
        int outgoing = (xl_getHead(pair) != contextPair);
        Arrow other = (outgoing ? xs_getHead(pair) : xl_tailOf(pair));
        if (xs_isRooted(c, pair) && xs_getTail(other) == c) {
            value = xs_getHead(other);
            list = xs_pair(outgoing ? xs_pair(a, value) : xs_pair(value, a), list);
        }
    }
    xl_enumFree(childrenEnum);

    if (xs_isAtom(c))
        return list;
    else
        return xs_childrenOf(xs_getTail(c), a, list);
}


/** returns a list of all children of $a rooted within context path $c
    via link/unlink functions
 */
Arrow xs_childrenOf(Arrow c, Arrow a) {
    TRACEPRINTF("BEGIN xs_childrenOf(%O, %O)", c->id, a->id);
    Arrow list = xs_childrenOf(c, a, EVE);
    TRACEPRINTF("END xs_childrenOf(%O, %O) = %O", c->id, a->id, list);
    return list;
}

/** close a session $s */
Arrow xs_close(Arrow s) {
    TRACEPRINTF("BEGIN xs_close(%O)", s);
    /* $session =  /$s/session/$agent+$uuid */
    xs_reset(A(atom("locked"), s));
    xs_reset(s);
    xs_unroot(tailOf(s), headOf(s));
    TRACEPRINTF("END xs_close(%O)", s);
    return s;
}

#if 0
static Arrow _fromUrl(Arrow context, char* url, char** urlEnd, int locateOnly) {
    DEBUGPRINTF("BEGIN _fromUrl(%06x, '%s')", context, url);
    Arrow a = EVE;

    char c = url[0];
    if (c >= 0 && c <= 32) { // Any control-caracters/white-spaces are considered as URI break
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
                __attribute__ ((fallthrough));
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

    DEBUGPRINTF("END _fromUrl(%06x, '%s') = %06x", context, url, a);
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
    DEBUGPRINTF("BEGIN fromUrl(%06x, '%s', %d)", context, url, locateOnly);
    char *nextUrl;
    Arrow a = _fromUrl(context, url, &nextUrl, locateOnly);
    if (!nextUrl) return a; // NIL or EVE

    nextUrl = skeepSpacesAndOnePlus(nextUrl);
    while (*nextUrl) {
        DEBUGPRINTF("nextUrl = >%s<", nextUrl);
        Arrow b = _fromUrl(context, nextUrl, &nextUrl, locateOnly);
        if (!nextUrl) return b; // NIL or EVE
        
        a = (locateOnly ? pairMaybe(a, b) : A(a, b)); // TODO: document actual design
        if (a == EVE) return EVE;
        
        nextUrl = skeepSpacesAndOnePlus(nextUrl);
    }

    DEBUGPRINTF("END fromUrl(%06x, '%s', %d) = %O", context, url, locateOnly, a);
    return a;
}

Arrow xs_url(Arrow s, char* aUrl) {
    TRACEPRINTF("BEGIN xs_url(%O, '%s')", s, aUrl);
    Arrow locked = A(atom("locked"), s);
    Arrow arrow = fromUrl(locked, aUrl, 0);
    TRACEPRINTF("END xs_url(%O, '%s') = %O", s, aUrl, arrow);
    return arrow;
}

Arrow xs_urlMaybe(Arrow s, char* aUrl) {
    TRACEPRINTF("BEGIN xs_urlMaybe(%O, '%s')", s, aUrl);
    Arrow locked = A(atom("locked"), s);
    Arrow arrow = fromUrl(locked, aUrl, 1);
    TRACEPRINTF("END xs_urlMaybe(%O, '%s') = %O", s, aUrl, arrow);
    return arrow;
}

static char* toURL(Arrow context, Arrow e, int depth, uint32_t *l) { // TODO: could be rewritten with geoallocs
    if (depth == 0) {
        char* url = malloc(8);
        assert(url);
        Arrow sa = xs_root(context, e);
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


char* xs_urlOf(Arrow s, Arrow e, int depth) {
    TRACEPRINTF("BEGIN xs_urlOf(%O, %O, %d)", s, e, depth);
    uint32_t l;
    Arrow locked = A(atom("locked"), s);

    char* url = toURL(locked, e, depth, &l);
    TRACEPRINTF("END xs_urlOf(%O, %O, %d) = '%s'", s, e, depth, url);
    return url;
}
#endif
