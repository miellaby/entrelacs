/** @file
 context handling

 A "context" is a way to represent a hierarchical structure of nested contexts.

 It's an arrow in the form "/C0+C1+C2..+Cn" where
  - C0 is an atom
  - each Cx is an identifier of some nested context in a parent context

 For example : "/World+Europa+France" represents :
   - the context of "France"
   - in the parent context of "Europa"
   - in the parent context "World"
   - in the root context

 One can root/unroot an arrow "$a" within a context path "$c".
 It consists in rooting 2 arrows ("double rooting"):
   - $r1 = "/C0/C1/../Cn+$a"
   - $r2 = "/C0+C1..+Cn+$a" (that is "/$c+$a")

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

*/
#pragma once
#include "machine/transient.h"


/** root an arrow $a in the context defined by path $c"
    Two arrows are actually rooted:
    $r1 = /C0/C1/../Cn+a
    $r2 = /$c+$a
*/
Arrow xs_context_root(Arrow context, Arrow a);

/** unroot an arrow $a within a context defined by its path $c, then returns it.
    Two arrows are actually unrooted:
    $r1 = /C0/C1/../Cn+a
    $r2 = /$c+$a
*/
Arrow xs_context_unroot(Arrow context, Arrow a);

/** return $c+$a if $a is rooted within a context defined by path $c, otherelse EVE.
      ~= isRooted(/$c+$a)
*/
Arrow xs_context_isRooted(Arrow context, Arrow a);

/** returns a list of all children of $a rooted within context path $c
 */
Arrow xs_context_childrenOf(Arrow c, Arrow a);

/** regular "get key".
    returns one suposedly unique rooted arrow in $c+$key sub-context
*/
Arrow xs_context_get(Arrow c, Arrow key);

/** regular "set key value"
*/
Arrow xs_context_set(Arrow c, Arrow key, Arrow value);

/** regular unset key.
*/
void xs_context_unset(Arrow c, Arrow key);

/** reset a context
  Recursivly unroot any rooted arrow under a given context
 */
void xs_context_reset(Arrow c);


#if 0

/** traditional edge
    root a pair from $source to $destination in the context defined by $c path.
    Two arrows are actually rooted:
    $r1 = /C0/C1/../Cn+/$source+$destination
    $r2 = //$c+$source+/$c+$destination
 */
Arrow xs_context_link(Arrow c, Arrow source, Arrow destination);

/** traditional edge removal
    unroot a pair from $source to $destination in the context defined by $c path.
 */
Arrow xs_context_unlink(Arrow c, Arrow source, Arrow destination);

/** returns a list of all children of $a rooted within context path $c
    via link/unlink functions
 */
Arrow xs_context_partnerOf(Arrow c, Arrow a);

/** returns a list of all rooted pairs within context path "/$c+$key"
*/
Arrow xs_context_partnersOf(Arrow c, Arrow a);

/** unroot all arrows within a context defined by its path $c,
    and recursivly reset any sub-contexts.
*/
void xs_context_reset(Arrow c);

/** traditional "set-key-value".

     1) reset context of path "/$c+$key" (see above)
     2) root $value in context "/$c+$key"
*/
Arrow xs_context_set(Arrow c, Arrow slot, Arrow value);


/** traditional "unset-key".

    reset context of path $c+$key
*/
void  xs_context_unset(Arrow c, Arrow slot);

/** traditional "get-key".
    returns "the" rooted arrow in context of path "/$c+$key"
    - if several arrows are rooted there, only one is returned.
    - if no arrow, return NIL.
*/
Arrow xs_context_get(Arrow c, Arrow slot);

#endif

