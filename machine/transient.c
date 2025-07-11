/**
 * manipulation de flèches éphémères
 * xl_... : flèches dans le Arrow Space (ref = adresse)
 * xs_... : flèches de travail transient/temporaire (ref = struct *)
 * xs_... fait de l'assimilisation paresseuse pour machine/session/server/repl
 * API d'après https://github.com/miellaby/entrelacs/blob/wiki/ArrowSpaceInterface.md
*/
#include "machine/transient.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <printf.h>

#include "entrelacs/entrelacs.h"
#include "entrelacs/entrelacsm.h"
#define LOG_CURRENT LOG_SESSION // TODO LOG_TRANSIENT
#include "log/log.h"
#include "machine/session.h"
#include "space/serial.h"


/// @brief  Transient Arrow Structure
typedef struct xs_arrow_s {
    uint32_t hash;
    Address  id;
    union xs_arrow_u {
        struct xs_atom_s {
            uint8_t* raw;
            uint32_t size;
        } atom;
        struct xs_pair_s {
            Arrow tail;
            Arrow head;
        } pair;
    } def;
    ArrowType type;
} ArrowValue;

// version "xs" de la flèche EVE 
static ArrowValue eveValue = { 0 };
static Arrow eve = &eveValue;

// A Pool of transient arrows
static Arrow *pool = NULL;
static uint32_t poolMax = 0;  ///< heap allocated log size
static uint32_t poolSize = 0; ///< significative log size

void pool_init() {
    geoalloc((char **)&pool, &poolMax, &poolSize, sizeof(ArrowValue), 0);
}

void pool_reset() {
    for (uint32_t i = 0; i < poolSize; i++) {
        Arrow a = pool[i];
        if (xs_getType(a) == XS_ATOM) {
            free (a->def.atom.raw);
            a->type = XS_UNDEF;
        }
    }
    pool_init();
}

static Arrow arrow_new() {
    Arrow a = &pool[poolSize];
    geoalloc((char **)&pool, &poolMax, &poolSize, sizeof(ArrowValue), poolSize + 1);
    memset(a, 0, sizeof(ArrowValue));
    return a;
}

static void arrow_free(Arrow a) {
    if (xs_getType(a) == XS_ATOM) {
        free (a->def.atom.raw);
    }
}

static char* xs_toURI(Arrow a, uint32_t *l);

Arrow xs_eve() {
    return eve;
}

Arrow xs_arrow(Address id) {
    // TODO je pense qu'on peut faire des Arrow avec id non-résolues (type == UNDEF?)
    if (id == XL_EVE) {
        return eve;
    }
    XLType type;
    uint32_t hash;
    Address tail, head;
    uint8_t raw;
    uint32_t size;
    if (xl_read(id, &type, &hash, &tail, &head, &raw, &size)) // bad id
        return NULL;

    Arrow a = arrow_new();
    a->id = id;
    a->type = type;
    a->hash = hash;
    if (type == XL_ATOM) {
        a->def.atom.raw = raw;
        a->def.atom.size = size;
    } else {
        a->def.pair.tail = xs_arrow(xl_tailOf(id));
        a->def.pair.head = xs_arrow(xl_headOf(id));
    }
    return a;      
}

Arrow xs_pair(Arrow tail, Arrow head) {
    Arrow a = arrow_new();
    a->def.pair.head = head;
    a->def.pair.tail = tail;
    a->type = XS_PAIR;
}

Arrow xs_atomn(size_t size, uint8_t* s) {
    Arrow a = arrow_new();
    memset(a, 0, sizeof(ArrowValue));
    a->def.atom.size = size;
    a->def.atom.raw = (uint8_t*) malloc(a->def.atom.size);
    assert(a->def.atom.raw);
    memcpy(a->def.atom.raw, s, a->def.atom.size);
    a->type = XS_ATOM;
}

Arrow xs_atom(char* s) {
    return xs_atomn(strlen(s) + 1, s);
}

Arrow xs_parseURI(uint32_t size, char *uri, uint32_t *uri_size_p);

Arrow xs_fromURI(char* uri) {
    return !uri ? eve : xs_parseURI(strlen(uri), uri, 0);   
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

char* xs_getDigest(Arrow a, uint32_t *l) {
    xs_assimilate(a);
    return xl_digestOf(a->id, l);
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

Address xs_getId(Arrow a) {
    return a ? a->id : XL_EVE;
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


char* xs_getUri(Arrow a) {
    return xs_toURI(a, NULL);
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

int xs_isRooted(Arrow a) {
    return (xs_isEve(a) || xs_resolve(a)->id) && xl_isRooted(a->id);
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

Arrow xs_root(Arrow e) {
    xs_assimilate(e);
    INFOPRINTF("xs_root(%O)", e->id);
    xl_root(e->id);
    return e;
}

Arrow xs_unroot(Arrow e) {
    xs_resolve(e);
    INFOPRINTF("xs_unroot(%O,%O)", XL_EVE, e->id);
    if (e == eve || e->id) {
        xl_unroot(e->id);
    }
    return e;
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

/** printf extension for arrow (%O specifier) */
static int printf_arrow_extension(FILE *stream,
        const struct printf_info *info,
        const void *const *args) {
    static const char* nilFakeURI = "(NULL)";
    Arrow arrow = *((Arrow*) (args[0]));
    char* uri = arrow != NULL ? xs_uriOf(arrow, NULL) : (char *)nilFakeURI;
    if (uri == NULL)
        uri = (char *)nilFakeURI;
    int len = fprintf(stream, "%*s", (info->left ? -info->width : info->width), uri);
    if (uri != nilFakeURI)
        free(uri);
    return len;
}

/** printf extension "arginfo" */
static int printf_arrow_arginfo_size(const struct printf_info *info, size_t n,
        int *argtypes, int *size) {
    (void) info; (void) size;
    /* We always take exactly one argument and this is a pointer to the
       structure.. */
    if (n > 0)
        argtypes[0] = PA_INT;
    return 1;
}

int xs_init() {
    if (xl_init())
        return 1;
    
    pool_init();

    // register a printf extension for arrow (glibc only!)
    register_printf_specifier('O', printf_arrow_extension, printf_arrow_arginfo_size);
}
