#ifndef SESSION_H
#define SESSION_H
/** @file
 Session contextualized information.

 session.h gathers functions to handle contexts and sessions.

 A "context path" is an arrow in the form /C0.C1.C2...Cn where C0 is Eve or an atom and each Cx is an identifier of some nested context in a parent context
 For example : /World.Europa.France points to the context identified by "France" in the parent context "Europa" in the parent context "World" in the root context

 Context-rooting an arrow $a within a context path /C0.C1..Cn consists in rooting both
  /C0.C1...Cn.$a and /C0/C1/../Cn.$a arrows. The newly formed context path (/$c.$a) is returned

 Of course, the path of the root context is Eve.

 A "session" simply is a special context whose path is in the form /$c.sessions./$agent.$id
  with $c is some context (maybe a super session), $agent some agent and $id some identifier

*/
#include "entrelacs/entrelacs.h"

/** context-root and return a session for given context, agent, and identifier.
    $session =  /$c.sessions./$agent.$id
*/
Arrow xls_session(Arrow c, Arrow agent, Arrow id);

/** reset a session and unroot it from its context.
*/
Arrow xls_close(Arrow s);

/** context-root an arrow and return its path ($c.$e).
    xls_root($c=/C0.C1...Cn, a)
      --> root(/C0/C1/.../Cn.a), root(/$c.a)
*/
Arrow xls_root(Arrow c, Arrow a);

/** return ($c.$e) if $e is context-rooted within $c, otherelse EVE.
    xls_isRooted($c, a) --> isRooted(/$c.a)
*/
Arrow xls_isRooted(Arrow c, Arrow a);

/** context-unroot an arrow and retun its path.
    xls_unroot($c=/C0.C1...Cn, a)
      --> unroot($c.a), unroot(/C0/C1/.../Cn.a)
*/
Arrow xls_unroot(Arrow c, Arrow a);

/** context-unroot all arrows for a given context path,
    and recursivly reset any sub-context.
*/
void xls_reset(Arrow c);

/** traditional "set-key-value".

     1) unroot any arrow from $c.$key context path
     2) root $value in /$c.$key context path
*/
Arrow xls_set(Arrow c, Arrow slot, Arrow value);

/** traditional "unset-key".

    reset $c.$key context path
*/
void  xls_unset(Arrow c, Arrow slot);

/** traditional "get-key".
    returns the rooted arrow in $c.$key context path
    (if several arrows, only one is returned)
*/
Arrow xls_get(Arrow c, Arrow slot);

/** Resolve an URL into an arrow.
    Any embedded ID must belong to the considered session.
*/
Arrow xls_url(Arrow s, char* url);

/** Resolve an URL into an arrow if it exists.
    Any embedded ID must belong to the considered session.
*/
Arrow xls_urlMaybe(Arrow s, char* url);

/** Forge an URL for a given arrow within a given session.
    Ancestors at 'depth' level are replaced by
    temporary ID which are only valid for the session.
*/
char* xls_urlOf(Arrow s, Arrow a, int depth);
#endif /* SESSION_H */
