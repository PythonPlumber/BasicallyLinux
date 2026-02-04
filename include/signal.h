#ifndef SIGNAL_H
#define SIGNAL_H

#include "process.h"
#include "types.h"

#define SIGKILL 9u
#define SIGTERM 15u
#define SIGSTOP 19u

int signal_send(uint32_t pid, uint32_t sig);
void signal_set_mask(process_t* proc, uint32_t mask);
void signal_dispatch(process_t* proc);

#endif
