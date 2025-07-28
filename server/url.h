#pragma once
#include "machine/transient.h"

/** Forge an URL for a given arrow within a given session.
    Ancestors at 'depth' level are replaced by
    temporary ID which are only valid in this session.
*/
char* xs_getURL(Arrow s, Arrow a, int depth);

/** Turn an URL into an arrow.
 *  Any embedded ID must belong to the considered session.
*/
Arrow xs_url(Arrow s, char* url);
