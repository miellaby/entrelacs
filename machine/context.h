/**
 * @file context.h
 * @brief Context Handling API
 * @details
 * the "xs_context_*" API aims to manage contexts and
 * arrows defined within them.
 *
 * == Definition of a "Context"
 *
 * A "Context" is a referential where:
 *
 * - arrows may be rooted and unrooted,
 * - variables may be setted, got and reset
 * - links may be created, unlinked and browsed.
 *
 * Contexts may be embeded in each others to form
 * a hierarchical structure of nested contexts.
 *
 * == Context Path
 *
 * A context is unambiguously defined by a context path, which is
 *  an arrow in the form "////C0+C1+C2..+Cn" where:
 *
 * - C0 is an atom like Eve
 * - each Ci is an identifier of some nested context in a parent context
 * Ci is not necessarly an atom.
 *
 * For example : "//World+Europa+France" represents :
 *
 * - the context of "France"
 * - in the parent context of "Europa"
 * - in the root context "World"
 *
 * == Double Rooting
 *
 * Rooting an arrow "a" within a context defined by
 * a path "$c" = /../C0+C1..+Cn
 * consists in rooting 2 arrows ("double rooting"):
 * - $r1 = /$c+a, that is /../C0+C1..+Cn+a
 * - $r2 = "/C0/C1/../Cn+a"
 *
 * Note $r1 only needs one additional arrow per context-rooted arrow while
 * $r2 requires to store each descendant starting from Cn+$a
 * up to /C0/C1../Cn+$a.
 *
 * $r1 allows efficient browsing of contexts.
 *
 *    For example, one can check that '$a' is rooted in a context '$c'
 *    by simply checking that the pair /$c+$a is rooted.
 *
 *    Rooted outgoing children of $c points all rooted arrows
 *    in the context (like $a)
 *
 *    Rooted incoming children from $a lists all the context paths
 *    where $a is rooted.
 *
 * $r2 allows "back-browsing" the contexts containing an abstracted
 * context-to-arrow relationship.
 *
 *    Browsing all rooted incoming *descendants* of the abstract arrow
 *    /Ck/../Cn/a gives the full hierarchy of contexts where
 *    this abstract arrow is defined in, like /C0/C1/../Ck-1
 *
 *    Browsing all rooted outgoing children from C0 gives all $r2 arrows
 *    starting from C0. They correspond to all context-rooted arrows whatever
 *    their rooting depth. So it works like a deep search of the context
 *    hierarchy starting from C0.
 *
 * TODO $r2 arrows are extra costly and one might use Indexes to avoid
 * storing all descendants.
 *
 * == Usage Example
 *
 *  - Browsing rooted outgoing children from a context such as ///tree+leaf+color
 *    retrieves the arrows rooted within this context, such as "green".
 *  - Browsing all descendants of /color+red up to rooted $r2 arrows,
 *    retrieves all contexts where "color" is "red".
 *    However it's not an efficient method to get the color of a given context
 *
 * == Variables and Links
 *
 * Variables and Links are based on contextual rooting.
 *
 * - A variable definition consists in a unique rooted "value" arrow
 *  in a "key" sub-context. Setting a value replaces the previous value.
 * - A link consists in a "target" arrow rooted in a "source" sub-context.
 * Contrarly to variables, there may be several targets for a given source.
 *
 * == About "Eve" Context
 *
 * When the xs_context_* methods root, unroot, get, set, unset, link, unlink
 * and browse are called with the 'Eve' context, they behave like
 * rooting methods in the global arrows space.
 *
 * However list/reset methods can't be used to browse the 'Eve' context as
 * one can't browse all rooted arrows in the arrows space.
*/
#pragma once
#include "machine/transient.h"

/** root an arrow $a in the context defined by path $c"
    Two arrows are actually rooted:
    $r1 = /$c+$a
    $r2 = /C0/C1/../Cn+a
*/
Arrow xs_context_root(Arrow context, Arrow a);

/** unroot an arrow $a within a context defined by its path $c, then returns it.
    Two arrows are actually unrooted:
    $r1 = /$c+$a
    $r2 = /C0/C1/../Cn+a
*/
Arrow xs_context_unroot(Arrow context, Arrow a);

/** tell if $a has been rooted in a context $c via xs_context_root
      ~= isRooted($r1=/$c+$a)
*/
int xs_context_isRooted(Arrow context, Arrow a);

/** list all arrows being rooted within a context $c
      ~= map(filter(childrenOf($c,outgoing),isRooted),getHead)
 *  IMPORTANT: Eve can't be browsed this way (no connectivity for Eve)
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
