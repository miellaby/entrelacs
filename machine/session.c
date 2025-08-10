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
#include "space/serial.h"
#include "space/space.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

extern Arrow xs_atom_session();

Arrow xs_session_open(char* agent) {
    xl_open();

    // $session =  /$s/session/$agent+$uuid
    Arrow uuid = xs_arrow(xl_anonymous());
    Arrow session = xs_pair(xs_atom(agent), uuid);
    xs_context_root(xs_atom_session(), session);
    return session;
}

char* xs_session_getId(Arrow session) {
    if (!xs_context_isRooted(xs_atom_session(), session)) {
        return NULL;
    }
    Arrow uuid = xs_getHead(session);
    if (!xs_isAtom(uuid)) {
        return NULL;
    }
    return xs_getStr(uuid);
}

Arrow xs_session_get(char* agent, char* uuid) {
    Arrow session = xs_pair(xs_atom(agent), xs_atom(uuid));
    if (!xs_context_isRooted(xs_atom_session(), session)) {
        return NULL;
    }
    return session;
}

Arrow xs_session_commit(Arrow session) {
    Address s = XL_EVE;
    if (!xs_context_isRooted(xs_atom_session(), session)) {
        WARNPRINTF("xs_session_commit: session is not valid");
    }
    // retrieve session arrow id
    s = xs_getId(xs_assimilate(session));

    // commit the arrow space changes
    xl_commit();

    // empty transient arrow pool
    xs_pool_reset();

    // return a new transient arrow for the session since the pool was emptied
    return xs_arrow(s);
}

void xs_session_close(Arrow session) {
    TRACEPRINTF("BEGIN xs_session_close(%O)", session);
    if (session != NULL) xs_unroot(session);
    xl_close();
    xs_pool_reset();
}
