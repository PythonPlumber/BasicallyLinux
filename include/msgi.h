#ifndef MSGI_H
#define MSGI_H

#include "types.h"

void msgi_init(void);
int msgi_send(uint32_t vector, uint32_t cpu);

#endif
