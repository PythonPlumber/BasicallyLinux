#ifndef IPC_FUTEX_H
#define IPC_FUTEX_H

#include "sched.h"
#include "types.h"

typedef struct {
    uint32_t value;
    wait_queue_t waiters;
} ipc_futex_t;

void ipc_futex_init(ipc_futex_t* futex, uint32_t value);
void ipc_futex_wait(ipc_futex_t* futex, uint32_t expected);
void ipc_futex_wake(ipc_futex_t* futex);

#endif
