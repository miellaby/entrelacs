#pragma once
#include "entrelacs/entrelacs.h"

void weaver_addLoose(Address a);

void weaver_removeLoose(Address a);

void weaver_connect(Arrow a, Arrow child, int childWeakness, int outgoing);

void weaver_disconnect(Arrow a, Arrow child, int weakness, int outgoing);

void weaver_forgetLoose(Arrow a);

void weaver_performGC();