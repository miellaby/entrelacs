#ifndef SESSION_H
#define SESSION_H
#include "machine/transient.h"
/** @file
 *
 * sessions are transient contexts where/when agents interact with the Entrelacs System.
 * sessions may be used as context with the xs_context* API
 * such as: root, unroot, isRooted, list, set, get, unset, reset, link, unlink, browse ...
 * The "Client API" (networked operations) works within a session
 *
 * a session is stored as a rooted /${session}+/${agent}+${session-uuid} arrow
 *
 * TODO
 * - the session arrow is actually
 *   (${agent} ${session-uuid}) contextualy rooted in context [${session} ]
 * - session confined in a "environment" context (environment != EVE)
 * - system session vs user session
 *   - system session is a session at system/admin level,
 *     - session path is [ ${session} (${agent} ${uuid}) ]
 *       this is the current implementation
 *   - user session is a session for a given user
 *     - ${session-uuid} contextualy rooted in context [ ${user} ${$username} ${session} ${agent}]
 *     - session path is [ ${user} ${$username} ${session} (${agent} ${uuid}) ]
 * - environment switching (landing)
 *   - all the rooted arrows in the session are rooted in the session environment
 *   - for a system session, arrows are rooted directly (no context)
 *     it makes change permanent in the system
 *   - for a user session, arrows are rooted in the user context
 *     /${user}+${$username}
 *     it makes change permanent for this user
 *   - the session itself is reseted to unmask changes
 * - identity management API
 *   - authenticated user session (credentials managment, connection)
 *   - guest user session
 *   - admin authentication to get a trusted system session
 */

/** create a new system session for a given agent $agent
 * by rooting /${session}/${agent}+${uuid}
*/
Arrow xs_session_open(char* agent);

/**
 * get a session id
 */
char* xs_session_getId(Arrow session);

/** return a previously defined session, Eve if not found
*/
Arrow xs_session_get(char* agent, char* uuid);

/**
 * commit a session
 * it empties the Arrow pool so no Arrow are valid anymore
 * But it returns a new arrow corresponding to the current session
 */
Arrow xs_session_commit(Arrow session);

/** reset and remove (unroot) a session.
*/
void xs_session_close(Arrow session);


/** authenticate a session
 * @param session the session to authenticate
 * @return true if the session is authenticated, false otherwise
 */
int xs_session_connect(Arrow session, char* username, char* password);

/** @brief check if a session is authenticated
 * @param session the session to check
 * @return true if the session is authenticated, false otherwise
 */
int xs_session_isAuthenticated(Arrow session);


#endif /* SESSION_H */
