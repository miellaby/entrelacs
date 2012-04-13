#ifndef SESSION_H
#define SESSION_H
/** @file
 Session contextualized information.
 */
#include "entrelacs/entrelacs.h"



/** roots and returns a session arrow, given a super session, an agent and an UUID arrow.
  * basically (superSession ("session" (agent sessionUuid))).
  * Note how a session arrow is a context stack: S = /Eve.s0.s1...sn
 */
Arrow xls_session(Arrow superSession, Arrow agent, Arrow sessionUuid);

/** root an arrow into a session context. Returns it as a meta-arrow.
    xls_root(S=/Eve.s0.s1...sn, a)
    <=> root(/S.a), root(/s0/s1/.../sn.a)
*/
Arrow xls_root(Arrow s, Arrow a);

/** return meta-arrow for a given arrow rooted in a session.
    xls_isRooted(S=/Eve.s0.s1...sn, a)
    <=> isRooted(/S.a) && isRooted(/s0/s1/.../sn.a)
*/
Arrow xls_isRooted(Arrow s, Arrow a);

/** unroot an arrow out of a session.
    xls_unroot(S=/Eve.s0.s1...sn, a)
    <=> unroot(/S.a), unroot(/s0/s1/.../sn.a)
*/
void xls_unroot(Arrow s, Arrow a);

/** unroot all session-attached arrows.
  * also reset and unroot any sub-session
*/
void xls_reset(Arrow s);

/** reset a session and detach it from its super-session. */
Arrow xls_close(Arrow s);


/* context stack is:
  S = /C0.C1.C2...Cn w/ C0 entrelacs or Eve
*/

/** set a slot to a value within a context stack.
  1) xls_unset(S, slot)
  2) xls_root(/S.slot, value)
 */
void  xls_set(Arrow s, Arrow slot, Arrow value);

/** Remove a slot and any attached value within a session
    simply by performing xls_reset(/s.slot)
 */
void  xls_unset(Arrow s, Arrow slot);

/** get a value of a slot within a context stack.
  return any value such as
    1) /S.slot.value is rooted
    2) /s0/s1...sn/slot.value is rooted as well
 */
Arrow xls_get(Arrow s, Arrow slot);

/** Assimilate an URL and get back its corresponding arrow.
  * Temporary ID based URL are only valid in a given session.
  */
Arrow xls_url(Arrow s, char* url);
/** Assimilate an URL and get back its corresponding arrow if existing.
  * Temporary ID based URL are only valid in a given session.
  */
Arrow xls_urlMaybe(Arrow s, char* url);


/** Forge a valid URL for a given arrow and session.
  * Ancestors at 'depth' level are substitued with temporary ID which are only valid for the session.
  */
char* xls_urlOf(Arrow s, Arrow a, int depth);
#endig /* SESSION_H *.
