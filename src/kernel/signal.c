#include "sched.h"
#include "signal.h"
#include "types.h"

int signal_send(uint32_t pid, uint32_t sig) {
    if (pid >= scheduler_process_count()) {
        return 0;
    }
    process_t* proc = scheduler_process_list();
    if (!proc) {
        return 0;
    }
    process_t* it = proc;
    do {
        if (it->pid == pid) {
            it->pending_signals |= (1u << (sig & 31u));
            return 1;
        }
        it = it->next;
    } while (it != proc);
    return 0;
}

void signal_set_mask(process_t* proc, uint32_t mask) {
    if (!proc) {
        return;
    }
    proc->signal_mask = mask;
}

void signal_dispatch(process_t* proc) {
    if (!proc) {
        return;
    }
    uint32_t pending = proc->pending_signals & ~proc->signal_mask;
    if (!pending) {
        return;
    }
    uint32_t sig = 0;
    for (uint32_t i = 0; i < 32; ++i) {
        if (pending & (1u << i)) {
            sig = i;
            break;
        }
    }
    proc->pending_signals &= ~(1u << sig);
    if (sig == SIGKILL || sig == SIGTERM) {
        process_exit(proc, sig);
    } else if (sig == SIGSTOP) {
        proc->state = PROCESS_BLOCKED;
    }
}
