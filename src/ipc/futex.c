#include "ipc/ipc_futex.h"
#include "kernel/sched.h"
#include "types.h"

void ipc_futex_init(ipc_futex_t* futex, uint32_t value) {
    if (!futex) {
        return;
    }
    futex->value = value;
    wait_queue_init(&futex->waiters);
}

void ipc_futex_wait(ipc_futex_t* futex, uint32_t expected) {
    if (!futex) {
        return;
    }
    if (futex->value != expected) {
        return;
    }
    wait_queue_wait(&futex->waiters);
}

void ipc_futex_wake(ipc_futex_t* futex) {
    if (!futex) {
        return;
    }
    wait_queue_wake_one(&futex->waiters);
}
