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

/** tell if $a has been rooted in a context $c via xs_context_root, otherwise NULL.
      ~= isRooted(/$c+$a)
*/
Arrow xs_context_isRooted(Arrow context, Arrow a);

/** list all arrows being rooted within a context $c
 */
Arrow xs_context_list(Arrow c);

// ============================================
// get/set: variable-like arrow management
// ============================================

/** regular "get key".
 *  @details returns one suposedly unique rooted arrow in $c+$key sub-context
 *  - if several arrows are rooted there, only one is returned.
 *  - if no arrow, return NULL.
*/
Arrow xs_context_get(Arrow c, Arrow key);

/** regular "set key value"
     1) reset context of path "/$c+$key" (see above)
     2) root $value in context "/$c+$key"
*/
Arrow xs_context_set(Arrow c, Arrow key, Arrow value);

/** regular unset key.
*/
void xs_context_unset(Arrow c, Arrow key);

/** reset a context
  Recursivly unroot any rooted arrow under a given context c
  also recursivly reset any sub-contexts to clean double-rooting.
*/
void xs_context_reset(Arrow c);

// ============================================
// link/unlink: pair things together with arrows
// ============================================

/// root (s-->d) into context "c"
#define xs_context_link(c,s,d) xs_context_root(xs_pair(c,s),d)

/// unroot (s-->d) from context "c"
#define xs_context_unlink(c,s,d) xs_context_unroot(xs_pair(c,s),d)

// list (s-->*) links in context "c"
#define xs_context_browse(c,s) xs_context_list(xs_pair(c,s))

// note: these variants don't leverage contextual indexes
// #define xs_context_link(c,s,d) xs_context_root(c,xs_pair(s,d))
// #define xs_context_unlink(c,s,d) xs_context_unroot(c,xs_pair(s,d))
// #define xs_context_browse(c,a) filter(xs_context_childrenOf(c), a => isChildOf(a))
