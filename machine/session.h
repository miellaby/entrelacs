#ifndef SESSION_H
#define SESSION_H
/** @file
 * 
 * user/agent session
 * 
 */

/** create a new session for a given agent $agent
*/
Arrow xs_open(char* agent);

/** return a previously defined session, Eve if not found
*/
Arrow xs_getSession(char* agent, char* uuid);

/** reset and remove (unroot) a session.
*/
Arrow xs_close(Arrow session);

#endif /* SESSION_H */
