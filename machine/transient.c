/**
 * manipulation de flèches éphémères
 */
#include "machine/transient.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <printf.h>

#include "entrelacs/entrelacs.h"
#define LOG_CURRENT LOG_TRANSIENT
#include "log/log.h"
#include "machine/session.h"
#include "space/serial.h"
#include "space/assimilate.h"
#include "machine/uri.h"
#include "mem/geoalloc.h"


/// @brief  Transient Arrow Structure
typedef struct xs_arrow_s {
    uint32_t hash;
    Address  id;
    union xs_arrow_u {
        struct xs_atom_s {
            uint8_t* raw;
            uint32_t size;
            int      borrowed;
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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Internal chunk structure
typedef struct chunk {
    ArrowValue *arrows;    // Array of arrows in this chunk
    size_t capacity;       // How many arrows this chunk can hold
    size_t count;          // How many arrows are currently used
    struct chunk *next;    // Next chunk in the list
} chunk_t;

// Global pool state
static struct {
    chunk_t *first_chunk;
    chunk_t *current_chunk;
    size_t   arrows_per_chunck;
    int initialized;
} g_pool = {0};

#define FIRST_CHUNCK_SIZE 128
void pool_init() {
    g_pool.arrows_per_chunck = FIRST_CHUNCK_SIZE;
    g_pool.first_chunk = malloc(sizeof(chunk_t));
    g_pool.first_chunk->arrows = malloc(sizeof(ArrowValue) * g_pool.arrows_per_chunck);
    g_pool.first_chunk->capacity = g_pool.arrows_per_chunck;
    g_pool.first_chunk->count = 0;
    g_pool.first_chunk->next = NULL;

    g_pool.current_chunk = g_pool.first_chunk;
    g_pool.initialized = 1;
}

static Arrow arrow_new() {
    // Check if current chunk has space
    if (g_pool.current_chunk->count < g_pool.current_chunk->capacity) {
        Arrow a = &g_pool.current_chunk->arrows[g_pool.current_chunk->count];
        g_pool.current_chunk->count++;
        a->type = 0;
        a->id = 0;
        a->hash = 0;
        memset(a, 0, sizeof(ArrowValue));
        return a;
    }

    // Need a new chunk - double the size for geometric growth
    size_t new_chunk_size = g_pool.arrows_per_chunck * 2;
    g_pool.arrows_per_chunck = new_chunk_size;

    chunk_t *new_chunk = malloc(sizeof(chunk_t));
    new_chunk->arrows = malloc(sizeof(ArrowValue) * new_chunk_size);
    new_chunk->capacity = new_chunk_size;
    new_chunk->count = 1;  // We're immediately using one object
    new_chunk->next = NULL;

    // Link the new chunk
    g_pool.current_chunk->next = new_chunk;
    g_pool.current_chunk = new_chunk;

    Arrow a = &new_chunk->arrows[0];
    a->type = 0;
    a->id = 0;
    a->hash = 0;
    memset(a, 0, sizeof(ArrowValue));
    return a;
}

void pool_reset() {
    // Free all chunks except the first one
    chunk_t *chunk = g_pool.first_chunk->next;
    while (chunk) {
        chunk_t *next = chunk->next;
        free(chunk->arrows);
        free(chunk);
        chunk = next;
    }

    // Reset the first chunk
    g_pool.first_chunk->count = 0;
    g_pool.first_chunk->next = NULL;
    g_pool.current_chunk = g_pool.first_chunk;
    g_pool.arrows_per_chunck = FIRST_CHUNCK_SIZE;  // Reset to initial size
}

// Optional: get pool statistics
typedef struct {
    size_t total_arrows;
    size_t total_capacity;
    size_t num_chunks;
    size_t memory_used;
} pool_stats_t;

pool_stats_t pool_get_stats() {
    pool_stats_t stats = {0};

    if (!g_pool.initialized) return stats;

    chunk_t *chunk = g_pool.first_chunk;
    while (chunk) {
        stats.total_arrows += chunk->count;
        stats.total_capacity += chunk->capacity;
        stats.num_chunks++;
        stats.memory_used += sizeof(chunk_t) + (sizeof(ArrowValue) * chunk->capacity);
        chunk = chunk->next;
    }

    return stats;
}

static void arrow_free(Arrow a) {
    if (xs_getType(a) == XS_ATOM) {
        if (!a->def.atom.borrowed) {
            free (a->def.atom.raw);
            a->type = XS_UNDEF;
            //DEBUGPRINTF("atom raw freed");
        }
    }
}

void xs_pool_reset() {
    //DEBUGPRINTF("pool reset");
    for (chunk_t *chunk = g_pool.first_chunk; chunk != NULL; chunk = chunk->next) {
        for (size_t i = 0; i < chunk->count; i++) {
            Arrow a = &chunk->arrows[i];
            arrow_free(a);
        }
    }
    pool_reset();
}

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
    uint8_t* raw;
    uint32_t size;
    if (xl_read(id, &type, &hash, &tail, &head, &raw, &size)) // bad id
        return NULL;

    Arrow a = arrow_new();
    a->id = id;
    a->hash = hash;
    if (type == XL_ATOM) {
        //DEBUGPRINTF("new atom %p from id %u = %.*s", a, id, size, raw);
        a->type = XS_ATOM;
        a->def.atom.raw = raw;
        a->def.atom.size = size;
    } else {
        a->type = XS_PAIR;
        //DEBUGPRINTF("new pair %p from ids %u %u", a, (unsigned) tail, (unsigned) head);
        a->def.pair.tail = xs_arrow(tail);
        a->def.pair.head = xs_arrow(head);
    }
    return a;
}

Arrow xs_pair(Arrow tail, Arrow head) {
    Arrow a = arrow_new();
    //DEBUGPRINTF("new pair %p=/%p+%p", a, tail, head);
    a->def.pair.head = head;
    a->def.pair.tail = tail;
    a->type = XS_PAIR;
    return a;
}

Arrow xs_atomn(size_t size, uint8_t* s) {
    Arrow a = arrow_new();
    //DEBUGPRINTF("new atom %p=%.*s", a, size, s);
    memset(a, 0, sizeof(ArrowValue));
    a->def.atom.size = size;
    a->def.atom.raw = (uint8_t*) malloc(a->def.atom.size);
    assert(a->def.atom.raw);
    memcpy(a->def.atom.raw, s, a->def.atom.size);
    a->type = XS_ATOM;
    return a;
}

Arrow xs_atom(char* s) {
    return xs_atomn(strlen(s), (uint8_t*) s);
}

Arrow xs_constn(size_t size, const uint8_t* buffer) {
    Arrow a = arrow_new();
    memset(a, 0, sizeof(ArrowValue));
    a->def.atom.size = size;
    a->def.atom.raw = (uint8_t*)buffer;
    a->def.atom.borrowed = 1;
    a->type = XS_ATOM;
    return a;
}

Arrow xs_const(const char* s) {
    return xs_constn(strlen(s), (uint8_t *)s);
}

Arrow xs_uri(char *aUri) {
    return xs_parseURI(NAN, aUri, NULL);
}

Arrow xs_urin(uint32_t aSize, char *aUri) {
    return xs_parseURI(aSize, aUri, NULL);
}

Arrow xs_digest(char *digest) {
    TRACEPRINTF("BEGIN xl_digest(%.*s)", DIGEST_SIZE, digest);
    Address address = probe_digest(digest);
    if (address != XL_NIL) {
        return xs_arrow(address);
    } else {
        return NULL;
    }
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
    //DEBUGPRINTF("getHash %p = %d", a, a->hash);
    return a->hash;
}

char* xs_getDigest(Arrow a, uint32_t *l) {
    xs_assimilate(a);
    return xl_digestOf(xs_getId(a), l);
}

Arrow xs_getTail(Arrow a) {
    if (a == NULL) {
        return a;
    }
    if (a->type != XS_PAIR) {
        return a;
    }
    return a->def.pair.tail;
}

Arrow xs_getHead(Arrow a) {
    if (a == NULL) {
        return a;
    }
    if (a->type != XS_PAIR) {
        return a;
    }
    return a->def.pair.head;
}

Address xs_getId(Arrow a) {
    return a ? a->id : XL_EVE;
}

char *xs_getStr(Arrow a) {
    if (a == NULL || a->type != XS_ATOM) {
        return NULL;
    }
    char* str = malloc(a->def.atom.size + 1);
    assert(str);
    memcpy(str, a->def.atom.raw, a->def.atom.size);
    str[a->def.atom.size] = '\0';
    return str;
}

uint8_t *xs_getMem(Arrow a, size_t *size) {
    if (a == NULL || a->type != XS_ATOM) {
        return NULL;
    }
    if (size) {
        *size = a->def.atom.size;
    }
    uint8_t* raw = malloc(a->def.atom.size + 1);
    assert(raw);
    memcpy(raw, a->def.atom.raw, a->def.atom.size);
    return raw;
}

uint8_t *xs_borrowMem(Arrow a, size_t *size) {
    if (a == NULL || a->type != XS_ATOM) {
        return NULL;
    }
    if (size) {
        *size = a->def.atom.size;
    }
    return a->def.atom.raw;
}

ssize_t xs_getSize(Arrow a) {
    if (a == NULL || a->type != XS_ATOM) {
        return -1;
    }
    return a->def.atom.size;
}

ssize_t xs_readMem(size_t size, uint8_t* buffer, Arrow a, size_t offset) {
    if (a == NULL || a->type != XS_ATOM || offset > a->def.atom.size) {
        return -1;
    }
    size_t read_size = a->def.atom.size - offset;
    if (size < read_size) {
        read_size = size;
    }
    if (read_size > 0)
        memcpy(buffer, a->def.atom.raw + offset, read_size);
    if (size > read_size)
        memset(buffer + read_size, 0, size - read_size);
    return read_size;
}

int xs_readStr(size_t size, uint8_t* buffer, Arrow a, size_t offset) {
    if (a == NULL || a->type != XS_ATOM || offset > a->def.atom.size) {
        return -1;
    }
    if (size > 0) {
        ssize_t read_size = xs_readMem(size - 1, buffer, a, offset);
        if (read_size == -1)
            return -1;
        buffer[size - 1] = '\0';
        return read_size;
    } else {
        return 0;
    }
}

char* xs_getUri(Arrow a) {
    return xs_getURI(a, NULL);
}

Arrow xs_resolve(Arrow a) {
    //DEBUGPRINTF("xs_resolve(%p)", a);
    if (a == NULL) {
        return NULL;
    }
    if (a == eve || a->id != 0) {
        return a;
    }

    // uint32_t hash = xs_getHash(a);
    if (a->type == XS_PAIR) {
        // FIXME essayer de tester immédiatement un candidat par le hash
        // il faut coder xl_probeByHash(type, hash, callback) qui renvoie toutes les paires avec un hash
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
    //DEBUGPRINTF("xs_assimilate(%p)", a);
    if (a == NULL) {
        return NULL;
    }
    if (a == eve || a->id != 0) {
        return a;
    }
    // uint32_t hash = xs_getHash(a);
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

int xs_isEve(Arrow a) {
    return a && a == eve; // eve est automatiquement assimilée
}

int xs_isAtom(Arrow a) {
    return a && xs_getType(a) == XS_ATOM;
}

int xs_isPair(Arrow a) {
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
    if (a == NULL) {
        return NULL;
    } else if (b == NULL) {
        return 0;
    } else if (a == b) {
        return 1;
    } else if (a->id && b->id) {
        return a->id == b->id;
    } else if (a->type != b->type) {
        return 0;
    } else if (a->id) {
        return xs_resolve(b)->id == a->id;
    } else if (b->id) {
        return xs_resolve(a)->id == b->id;
    } else if (a->type == XS_ATOM) {
        if (a->def.atom.size != b->def.atom.size) {
            return 0;
        } else if (a->def.atom.raw == b->def.atom.raw) {
            return 1;
        } else {
            return memcmp(a->def.atom.raw, b->def.atom.raw, a->def.atom.size) == 0;
        }
    } else if (a->type == XS_PAIR) {
        Arrow tail_a = xs_getTail(a);
        Arrow head_a = xs_getHead(a);
        Arrow tail_b = xs_getTail(b);
        Arrow head_b = xs_getHead(b);
        int same_tail = (tail_a == eve ? tail_b == eve : xs_equal(tail_a, tail_b));
        int same_head = (head_a == eve ? head_b == eve : xs_equal(head_a, head_b));
        return same_tail && same_head;
    } else {
        return 0;
    }
}

Arrow xs_root(Arrow e) {
    INFOPRINTF("xs_root(%p)", e);
    xs_assimilate(e);
    INFOPRINTF("xs_root(%O)", e);
    xl_root(e->id);
    return e;
}

Arrow xs_unroot(Arrow e) {
    if (e == NULL) {
        return NULL;
    }
    xs_resolve(e);
    INFOPRINTF("xs_unroot(%O,%O)", XL_EVE, e);
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
                .raw = (uint8_t *) "session",
                .size = 8,
                .borrowed = 1
            }
        },
        .type = XS_ATOM
    };
    return &arrow;
}

struct call_callback_closure { XSCallBack cb; void* context; };

static Address call_callback(Address a, void* context) {
    struct call_callback_closure* closure = context;
    closure->cb(xs_arrow(a), closure->context);
    return XL_EVE;
};

void xs_childrenOfCB(Arrow a, XSCallBack cb, void* context) {
    if (!xs_isKnown(a)) {
        return;
    }
    struct call_callback_closure closure = { cb, context };
    xl_childrenOfCB(a->id, call_callback, &closure);
}

/** printf extension for arrow (%O specifier) */
static int printf_arrow_extension(FILE *stream,
        const struct printf_info *info,
        const void *const *args) {
    static const char* nilFakeURI = "(NULL)";
    Arrow arrow = *((Arrow*) (args[0]));
    char* uri = arrow != NULL ? xs_getURI(arrow, NULL) : (char *)nilFakeURI;
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
        argtypes[0] = PA_POINTER;
    return 1;
}

int xs_init() {
    if (xl_init())
        return 1;

    pool_init();

    // register a printf extension for arrow (glibc only!)
    register_printf_specifier('O', printf_arrow_extension, printf_arrow_arginfo_size);
    return 0;
}
