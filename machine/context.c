
#include "machine/context.h"
#include "space/space.h"
// TODO
// refondre la mémoire d'après https://docs.google.com/document/d/1h8U5LhEVQQN57zU0p97AMdx9LhVloxsMB97i0eB0e24/edit
// et implémenter les nouveaux algos xs_context_*...

Arrow xs_context_root(Arrow c, Arrow a) {
    if (xs_isEve(c)) {
        return xs_root(a);
    }
    xs_assimilate(a);
    xs_assimilate(c);
    INFOPRINTF("xs_root(%O,%O)", xs_getId(c), xs_getId(a));
    xl_root(xl_pair(xs_getId(c), xs_getId(a))); /// double-rooting for indexation
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
    
    INFOPRINTF("xs_root(%O,%O)", xs_getId(c), xs_getId(a));
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

Arrow xs_context_isRooted(Arrow c, Arrow a) {
    if (xs_isEve(c)) {
        return xs_isRooted(a);  
    }
    Arrow idx = xs_pair(c, a);
    return xs_isRooted(idx);
}

Arrow _xs_context_childrenOf(Arrow c, Arrow a, Arrow list) {
    if (!xs_isKnown(c)) return;
    if (!xs_isKnown(a)) return;

    Arrow c_a = xs_pair(c, a);
    if (!xs_isKnown(a)) {
        return list;
    }
    Address contextPair = xs_getId(c_a);
    XLEnum childrenEnum = xl_childrenOf(contextPair);
    while (xl_enumNext(childrenEnum)) {
        Address pair = xl_enumGet(childrenEnum);
        int outgoing = (xl_getHead(pair) != contextPair);
        Address other = (outgoing ? xl_getHead(pair) : xl_tailOf(pair));
        if (xl_getTail(other) != xs_getId(c)) continue;
        if (!xl_isRooted(pair)) continue;
        Arrow value = xs_getHead(other);
        Arrow child = outgoing ? xs_pair(a, value) : xs_pair(value, a);
        list = xs_pair(child, list);
    }
    xl_enumFree(childrenEnum);

    if (xs_isAtom(c)) {
        return list;
    }
    return _xs_context_childrenOf(xs_getTail(c), a, list);
}

Arrow xs_context_childrenOf(Arrow c, Arrow a) {
    xs_resolve(c);
    xs_resolve(a);
    if (xs_isKnown(c) && xs_isKnown(a)) {
        TRACEPRINTF("BEGIN xs_childrenOf(%O, %O)", xs_getId(c), xs_getId(a));
        Arrow list = _xs_context_childrenOf(c, a, xs_eve());
        TRACEPRINTF("END xs_childrenOf(%O, %O) = %O", xs_getId(c), xs_getId(a), xs_getId(list));
        return list;
    } else {
        TRACEPRINTF("xs_childrenOf: c or a missing");
        return xs_eve();
    }  
}

/** reset a context
  Recursivly unroot any rooted arrow under a given context
 */
void xs_context_reset(Arrow c) {
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
        // process c_child as a sub context
        xs_context_reset(xs_arrow(c_child));
        // unroot c_child as "a rooted in c" arrow 
        xl_unroot(c_child);
    }

    xl_enumFree(childrenEnum);
}

/** regular "set"
     1) unroot any arrow from $c+$key sub-context
     2) root $value in /$c+$key sub-context
*/
Arrow xs_context_set(Arrow c, Arrow key, Arrow value) {
    xs_assimilate(c);
    xs_assimilate(key);
    xs_assimilate(value);
    INFOPRINTF("xs_set(%O,%O,%O)", xs_getId(c), xs_getId(key), xs_getId(value));
    Arrow sub_context = xs_pair(c, key);
    xs_context_reset(sub_context);
    return xs_context_root(sub_context, value);
}

/** traditional unset.
    reset /$c+$key sub-context
*/
void xs_context_unset(Arrow c, Arrow key) {
    // INFOPRINTF("xs_unset(%O,%O)", c, key);
    xs_context_reset(xs_pair(c, key));
}

/** regular "get".
    returns one suposedly unique rooted arrow in $c+$key sub-context
*/
Arrow xs_context_get(Arrow c, Arrow key) {
    Arrow value;
    Arrow context_key = xs_pair(c, key);
    if (!xs_isKnown(context_key)) {
        return xs_eve();
    }
    TRACEPRINTF("BEGIN xs_get(%O,%O)", xs_getId(c), xs_getId(key));
    Address c_k_id = xs_getId(c_k_id);
    XLEnum childrenEnum = xl_childrenOf(c_k_id);
    while (xl_enumNext(childrenEnum)) {
        Address keyValue = xl_enumGet(childrenEnum);
        if (xl_tailOf(keyValue) != c_k_id) continue; // incoming arrows are ignored
        if (xl_isRooted(keyValue)) {
            value = xs_arrow(xl_headOf(keyValue));
            break;
        }
    }
    xl_enumFree(childrenEnum);
    TRACEPRINTF("END xs_get(%O,%O) = %O", xs_getId(c), xs_getId(key), xs_getId(value));
    return value;
}
