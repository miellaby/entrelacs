
#include "machine/context.h"
#define LOG_CURRENT LOG_SESSION
#include "log/log.h"
#include "space/space.h"

// TODO
// refondre la mémoire d'après https://docs.google.com/document/d/1h8U5LhEVQQN57zU0p97AMdx9LhVloxsMB97i0eB0e24/edit
// et implémenter les nouveaux algos xs_context_*...

Arrow xs_context_root(Arrow c, Arrow a) {
    TRACEPRINTF("xs_context_root(%O,%O)", c, a);
    if (xs_isEve(c)) {
        return xs_root(a);
    }
    /// first-root for indexation
    xs_root(xs_pair(c, a));
    if (xs_isEve(c) || xs_isAtom(c)) {
        return a; // no double rooting
    }

    // second root (c0+(c1+...(cn+a))) where c=c0+c1)+c2)...)+cn
    Arrow s = a;
    Arrow d = c;
    while (xs_isPair(d)) {
        s = xs_pair(xs_getHead(d), s);
        d = xs_getTail(d);
    }
    s = xs_pair(d, s);
    xs_root(s);
    return a;
}

/// unroot in a context
Arrow xs_context_unroot(Arrow c, Arrow a) {
    TRACEPRINTF("xs_context_unroot(%O,%O)", c, a);
    if (xs_isEve(c)) {
        return xs_unroot(a);
    }
    if (!xs_isKnown(c)) { // context not known
        return a;
    }
    Arrow idx = xs_pair(c, a); // double-rooting pair
    if (!xs_isRooted(idx)) { // idx not rooted
        return a; // not rooted
    }

    xs_unroot(idx);

    Arrow s = a;
    Arrow d = c;
    while (xs_isPair(d)) {
        s = xs_pair(xs_getHead(d), s);
        d = xs_getTail(d);
    }
    s = xs_pair(d, s);
    xs_unroot(s);
    return a;
}

int xs_context_isRooted(Arrow c, Arrow a) {
    TRACEPRINTF("xs_context_isRooted(%O,%O)", c, a);
    if (xs_isEve(c)) {
        return xs_isRooted(a);
    }
    Arrow idx = xs_pair(c, a);
    return xs_isRooted(idx);
}

Arrow xs_context_list(Arrow c) {
    TRACEPRINTF("BEGIN xs_context_list(%O)", c);
    if (!xs_isKnown(c)) {
        // if c or a not assimilated there can't be children
        TRACEPRINTF("END xs_context_list: c not assimilated");
        return xs_eve();
    }

    Arrow list = xs_eve();
    XLEnum childrenEnum = xl_childrenOf(xs_getId(c));
    while (xl_enumNext(childrenEnum)) {
        Address pair = xl_enumGet(childrenEnum);
        if (!xl_isRooted(pair)) {
            continue;
        }
        int outgoing = (xl_tailOf(pair) == xs_getId(c));
        if (outgoing) {
            Address arrow = xl_headOf(pair);
            list = xs_pair(xs_arrow(arrow), list);
        }
    }
    xl_enumFree(childrenEnum);
    TRACEPRINTF("END xs_context_list(%O) = %O", c, list);
    return list;
}

/** reset a context
  Recursively unroot any rooted arrow under a given context
 */
void xs_context_reset(Arrow c) {
    TRACEPRINTF("xs_context_reset(%O)", c);
    if (!xs_isKnown(c)) return;
    Address context = xs_getId(c);
    XLEnum childrenEnum = xl_childrenOf(context);
    Address next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : XL_EVE);
    while (next != XL_EVE) {
        Address c_child = next;
        next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : XL_EVE);
        if (xl_tailOf(c_child) != context) { // Only outgoing arrow
            continue;
        }
        Address child_head = xl_headOf(c_child);
        // process c_child as a sub context
        xs_context_reset(xs_arrow(c_child));
        // unroot child_head as "a rooted in c" arrow
        xs_context_unroot(c, xs_arrow(child_head));
    }

    xl_enumFree(childrenEnum);
}

/** reset a context
  Recursively unroot any rooted arrow BUT one (a) under a given context
 */
static int xs_context_reset_others(Arrow c, Arrow a) {
    TRACEPRINTF("xs_context_reset_others(%O)", c);
    if (!xs_isKnown(c)) return 0;
    Address context = xs_getId(c);
    Address preserved = xs_getId(xs_assimilate(a));
    XLEnum childrenEnum = xl_childrenOf(context);
    Address next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : XL_EVE);
    int found = 0;
    while (next != XL_EVE) {
        Address c_child = next;
        next = (xl_enumNext(childrenEnum) ? xl_enumGet(childrenEnum) : XL_EVE);
        if (xl_tailOf(c_child) != context) { // Only outgoing arrow
            continue;
        }
        Address child_head = xl_headOf(c_child);
        if (child_head == preserved) {
            found = 1;
            // skip preserved arrow
            continue;
        }
        // process c_child as a sub context
        xs_context_reset(xs_arrow(c_child));
        // unroot child_head as "a rooted in c" arrow
        xs_context_unroot(c, xs_arrow(child_head));
    }

    xl_enumFree(childrenEnum);
    return found;
}

/** regular "set"
     1) unroot any arrow from $c+$key sub-context
     2) root $value in /$c+$key sub-context
*/
Arrow xs_context_set(Arrow c, Arrow key, Arrow value) {
    TRACEPRINTF("xs_context_set(%O,%O,%O)", c, key, value);
    xs_assimilate(c);
    xs_assimilate(key);
    xs_assimilate(value);
    Arrow key_context = xs_isEve(c) ? key : xs_pair(c, key);

    if (xs_context_reset_others(key_context, value)) {
        TRACEPRINTF("xs_context_set(%O,%O,%O) already setted", c, key, value);
        return value;
    }

    return xs_context_root(key_context, value);
}

/** traditional unset.
    reset /$c+$key sub-context
*/
void xs_context_unset(Arrow c, Arrow key) {
    TRACEPRINTF("xs_context_unset(%O,%O)", c, key);
    xs_context_reset(xs_isEve(c) ? key : xs_pair(c, key));
}

/** regular "get".
    returns one suposedly unique rooted arrow in $c+$key sub-context
*/
Arrow xs_context_get(Arrow c, Arrow key) {
    TRACEPRINTF("BEGIN xs_context_get(%O,%O)", c, key);
    Arrow u = c;
    Arrow list = xs_context_list(xs_isEve(u) ? key : xs_pair(u, key));
    while (xs_isEve(list) && !xs_isEve(u) && !xs_isAtom(u)) {
        u = xs_getTail(u);
        list = xs_context_list(xs_isEve(u) ? key : xs_pair(u, key));
    }
    if (xs_isEve(list)) {
        // no value found
        TRACEPRINTF("END xs_context_get(%O,%O) = NULL", c, key);
        return NULL;
    }
    Arrow value = xs_getTail(list);

    TRACEPRINTF("END xs_context_get(%O,%O)=%O", c, key, value);
    return value;
}
