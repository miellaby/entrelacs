#pragma once
#include <stdint.h>
#include "space/cell.h"
#include "entrelacs/entrelacs.h"
extern uint32_t looseLogSize;

int weaver_init();

void weaver_addLoose(Address a);

void weaver_removeLoose(Address a);

void weaver_connect(Address a, Address child, int childWeakness, int outgoing);

void weaver_disconnect(Address a, Address child, int weakness, int outgoing);

void weaver_forgetLoose(Address a);

void weaver_performGC();

void weaver_destroy();