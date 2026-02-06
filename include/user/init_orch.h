#ifndef INIT_ORCH_H
#define INIT_ORCH_H

#include "types.h"

void init_orch_init(void);
int init_orch_start(void (*entry)(void));

#endif
