#include "kernel/job.h"
#include "types.h"

void job_init(void) {
}

void job_assign(process_t* proc, uint32_t job_id, uint32_t session_id) {
    if (!proc) {
        return;
    }
    proc->job_id = job_id;
    proc->session_id = session_id;
}

void job_set_pgrp(process_t* proc, uint32_t job_id) {
    if (!proc) {
        return;
    }
    proc->job_id = job_id;
}
