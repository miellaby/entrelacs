#ifndef SESSION_H
#define SESSION_H
/** @file
 Helpers to manage contexts and sessions.

 A "context path" is a way to represent a hierarchical structure of nested contexts.

 It's an arrow in the form "/C0+C1+C2+++Cn" where
  - C0 is Eve or an atom
  - each Cx is an identifier of some nested context in a parent context
  - C0=Eve is the root context path.

 For example : "/World+Europa+France" represents :
   - the context of "France"
   - in the parent context of "Europa"
   - in the parent context "World"
   - in the root context

 One can root/unroot an arrow "$a" within a context path "$c".
 It consists in rooting 2 arrows:
   - $r1 = "/C0/C1/++/Cn+$a"
   - $r2 = "/C0+C1+++Cn+$a" (that is "/$c+$a")

  $r1 is costly to build and store as each ancester up to $a is likely to
  be created. But $r1 contributes to index arrows and contexts whatever the abstraction levels.
  It allows to get all the context paths where some tail path (for example /Cn+$a) is rooted.

  $r2 is easy to build up. It allows to quickly check that $a is rooted in the
  context path. It allows to get immediatly the context $a is rooted.

  What does it mean concretly ?
   - consider some atom, eg: "red"
   - By fetching rooted incoming arrows ($r1 arrows), you can easily get all the contexts
     linked to "red", e.g: "/canada+tree+autumn+tree+color+red".
   - Now by considering the "meta" arrow /color+red, you can easily explore its descendants up
     to rooted $r1 arrows. It allows to efficiently get all the context paths where /color+red
     is rooted!

 Directly rooting an arrow "$a" corresponds to the specific case where the context path is Eve.

 A "session" is a context path in the form "/$super+sessions+$x"
  where
   - $super is some super context (maybe a container session),
   - $agent is some agent
   - $x is the bottom context. It is itself defined by the arrow "/$agent+$uuid" where
     - $agent represents an agent
     - $uuid represents an unique session identifier

*/

#include "entrelacs/entrelacs.h"

/** define a session
    - in a given context $c,
    - for a given agent $agent,
    - with a given identifier $id.

    = assimilate and root the arrow "/$agent+$id" in the context path "$c"
*/
Arrow xls_session(Arrow c, Arrow agent, Arrow id);

/** return a previously defined session /$c+sessions+/$agent+$id, Eve if not found
*/
Arrow xls_sessionMaybe(Arrow c, Arrow agent, Arrow id);

/** reset and remove (unroot) a session.
*/
Arrow xls_close(Arrow s);

/** root an arrow $a in the context defined by path $c and return the resulting path "/$c+$a"
    Two arrows are actually rooted:
    $r1 = /C0/C1/++/Cn+a
    $r2 = /$c+$a
*/
Arrow xls_root(Arrow c, Arrow a);

/** return $c+$a if $a is rooted within a context defined by path $c, otherelse EVE.
      ~= isRooted(/$c+$a)
*/
Arrow xls_isRooted(Arrow c, Arrow a);

/** unroot an arrow $a within a context defined by its path $c, then returns it.
    Two arrows are actually unrooted:
    $r1 = /C0/C1/++/Cn+a
    $r2 = /$c+$a
*/
Arrow xls_unroot(Arrow c, Arrow a);

/** unroot all arrows within a context defined by its path $c,
    and recursivly reset any sub-contexts.
*/
void xls_reset(Arrow c);

/** traditional "set-key-value".

     1) reset context of path "/$c+$key" (see above)
     2) root $value in context "/$c+$key"
*/
Arrow xls_set(Arrow c, Arrow slot, Arrow value);


/** traditional "unset-key".

    reset context of path $c+$key
*/
void  xls_unset(Arrow c, Arrow slot);

/** traditional "get-key".
    returns "the" rooted arrow in context of path "/$c+$key"
    - if several arrows are rooted there, only one is returned.
    - if no arrow, return NIL.
*/
Arrow xls_get(Arrow c, Arrow slot);

/** Forge an URL for a given arrow within a given session.
    Ancestors at 'depth' level are replaced by
    temporary ID which are only valid in this session.
*/
char* xls_urlOf(Arrow s, Arrow a, int depth);

/** Resolve an URL into an arrow.
    Any embedded ID must belong to the considered session.
*/
Arrow xls_url(Arrow s, char* url);

/** Resolve an URL into an arrow if it exists.
    Any embedded ID must belong to the considered session.
*/
Arrow xls_urlMaybe(Arrow s, char* url);

/** returns a list-arrow of all "the" rooted arrows in context of path "/$c+$key"
*/
Arrow xls_partnerOf(Arrow c, Arrow slot);


#endif /* SESSION_H */
