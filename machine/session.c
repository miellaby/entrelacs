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
    Arrow uuid = xs_anonymous();
    Arrow session = xs_pair(xs_atom_session(), xs_pair(xs_atom(agent), uuid));
    xs_root(session);
    return session;
}

char* xs_session_getId(Arrow session) {
    if (!xs_equal(xs_getTail(session), xs_atom_session())) {
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
    TRACEPRINTF("BEGIN xs_close(%O)", session);
    xs_unroot(session);
    xl_close();
    pool_reset();
}
