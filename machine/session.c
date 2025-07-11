// TODO
// le pool devrait être dans la session
// - le contexte par défaut est la session, on peut en sortir
// - xs_enter rentrer dans un contexte
//   pourrait générer une clé pour sortir (à fournir dans xs_departure)
// - xs_departure accéder au niveau meta/supérieur
// - il existe un clé système, pour sortir de la session et devenir root
// 
// ================================================

#include "machine/session.h"

#define LOG_CURRENT LOG_SESSION
#include "log/log.h"
#include "machine/transient.h"
#include "space/serial.h"
#include "space/space.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

extern Arrow xs_atom_session();

Arrow xs_open(char* agent) {
    xl_open();

    // $session =  /$s/session/$agent+$uuid
    Arrow uuid = xs_arrow(xl_anonymous());
    Arrow session = xs_pair(xs_atom_session(), xs_pair(xs_atom(agent), uuid));
    xs_root(session);
    return session;
}
 
char* xs_session_getId(Arrow session) {
    if (xs_getTail(session) != xs_atom_session()) {
        return NULL;
    }
    Arrow uuid = xs_getHead(xs_getHead(session));
    if (!xs_isAtom(uuid)) {
        return NULL;
    }
    return xs_getStr(uuid);
}

Arrow xs_getSession(char* agent, char* uuid) {
    Arrow agent_uuid = xs_pair(xs_atom(agent), xs_atom(uuid));
    Arrow session = xs_pair(xs_atom_session(), agent_uuid);
    if (!xs_isRooted(session)) {
        return NULL;
    }
    return session;
}

Arrow xs_commit(Arrow session) {
    // pool vidé à chaque commit
    // récupération de l'addresse de la session
    Address s = xs_getId(xs_assimilate(session));
    xl_root(s);
    xl_commit();
    pool_reset();
    // on renvoie une nouvelle flèche session après vidage du pool
    return xs_arrow(s);
}

void xs_close(Arrow session) {
    TRACEPRINTF("BEGIN xs_close(%O)", xs_getId(session));
    xs_unroot(session);
    xl_close();
    pool_reset();
}

// get an URI path corresponding to an arrow
static char* xs_toURI(Arrow a, uint32_t *l) { // TODO: could be rewritten with geoallocs
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
            char* raw = xs_getMem(a, &size);
            if (size >= BLOB_MINSIZE) {
                uri = xs_digestOf(a, l);
                uri = malloc(3 * size + 1); // memory allocation for encoded content
                assert(uri);
                size_t uri_size;
                percent_encode(raw, size, uri, &uri_size);
                uri = realloc(uri, 1 + uri_size);
                if (l) *l = uri_size; // return length if asked
            }
            free(raw);
            return uri;
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

Arrow xs_parseURI(uint32_t size, char *uri, uint32_t *uri_size_p) {
    TRACEPRINTF("BEGIN serial_parseUri(%s)", uri);
    if (size == 0) {
        if (uri_size_p) {
            *uri_size_p = 0;
        }
        return xs_eve();
    }

    Arrow a = NULL;
    uint32_t uri_size = NIL;

    char c = uri[0];
    
    if (c <= 32 || !size) { // Any control-caracters/white-spaces are considered as URI break
        a = xs_eve();
        uri_size = 0;
    } else switch (c) {
        case '+': // Eve
            a = xs_eve();
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
            
            if (size != NIL) size--;
            
            tail = xs_parseURI(size, uri + 1, &tailUriSize);
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
            
            head = xs_parseURI(size, headUriStart, &headUriSize);
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
