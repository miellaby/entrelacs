#ifndef SESSION_H
#define SESSION_H
/** @file
 *
 * session for agents interacting with the Entrelacs System
 *
 */

/** create a new session for a given agent $agent
*/
Arrow xs_open(char* agent);

/**
 * get a session id
 */
char* xs_session_getId(Arrow session);

/** return a previously defined session, Eve if not found
*/
Arrow xs_getSession(char* agent, char* uuid);

/**
 * commit a session
 * it empties the Arrow pool so no Arrow are valid anymore
 * But it returns a new arrow corresponding to the current session
 */
Arrow xs_commit(Arrow session);

/** reset and remove (unroot) a session.
*/
void xs_close(Arrow session);

#endif /* SESSION_H */
