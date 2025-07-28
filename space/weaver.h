#pragma once
#include <stdint.h>
#include "space/cell.h"
#include "entrelacs/entrelacs.h"
extern uint32_t looseLogSize;

int weaver_init();

void weaver_addLoose(Address a);

void weaver_removeLoose(Address a);

void weaver_connect(Address parent, Address child, int outgoing);

void weaver_disconnect(Address parent, Address child, int outgoing);

void weaver_performGC();

void weaver_destroy();