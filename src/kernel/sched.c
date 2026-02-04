#include "paging.h"
#include "sched.h"
#include "secure_caps.h"
#include "signal.h"
#include "timer.h"
#include "types.h"
#include "vm_space.h"

#define MAX_PROCESSES 16
#define STACK_SIZE 4096
#define MAX_PRIORITY_BOOST 8

static process_t processes[MAX_PROCESSES];
static uint8_t stacks[MAX_PROCESSES][STACK_SIZE] __attribute__((aligned(16)));
static uint32_t process_count = 0;
static process_t* process_list = 0;
static process_t* current_process = 0;
static uint32_t default_priority = 10;
static uint32_t default_time_slice = 4;
static wait_queue_t process_wait_queues[MAX_PROCESSES];

process_t* scheduler_current(void) {
    return current_process;
}

process_t* scheduler_process_list(void) {
    return process_list;
}

uint32_t scheduler_process_count(void) {
    return process_count;
}

int scheduler_kill(uint32_t pid) {
    if (pid >= process_count) {
        return 0;
    }
    process_t* proc = &processes[pid];
    proc->state = PROCESS_BLOCKED;
    proc->wake_tick = 0;
    proc->time_remaining = 0;
    proc->boost = 0;
    proc->ready_ticks = 0;
    for (uint32_t i = 0; i < PROCESS_MAX_FDS; ++i) {
        proc->fds[i].node = 0;
        proc->fds[i].offset = 0;
        proc->fds[i].flags = 0;
    }
    if (proc == current_process) {
        current_process = current_process->next;
    }
    return 1;
}

void scheduler_init(void) {
    process_count = 0;
    process_list = 0;
    current_process = 0;
    for (uint32_t i = 0; i < MAX_PROCESSES; ++i) {
        wait_queue_init(&process_wait_queues[i]);
    }
}

static process_t* list_append(process_t* proc) {
    if (!process_list) {
        process_list = proc;
        proc->next = proc;
        return proc;
    }
    process_t* tail = process_list;
    while (tail->next != process_list) {
        tail = tail->next;
    }
    tail->next = proc;
    proc->next = process_list;
    return proc;
}

process_t* process_create(void (*entry)(void), uint32_t* page_directory) {
    if (process_count >= MAX_PROCESSES) {
        return 0;
    }
    uint32_t pid = process_count;
    process_t* proc = &processes[pid];
    proc->pid = pid;
    proc->state = PROCESS_READY;
    if (!page_directory) {
        page_directory = vm_space_create();
    }
    if (!page_directory) {
        page_directory = paging_get_directory();
    }
    proc->page_directory = page_directory;
    uint32_t stack_top = (uint32_t)&stacks[pid][STACK_SIZE];
    registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
    frame->ds = 0x10;
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = stack_top;
    frame->esp = stack_top;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = (uint32_t)entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->useresp = 0;
    frame->ss = 0;

    proc->eax = frame->eax;
    proc->ebx = frame->ebx;
    proc->ecx = frame->ecx;
    proc->edx = frame->edx;
    proc->esi = frame->esi;
    proc->edi = frame->edi;
    proc->esp = (uint32_t)frame;
    proc->ebp = frame->ebp;
    proc->eip = frame->eip;
    proc->eflags = frame->eflags;
    proc->cs = frame->cs;
    proc->ds = frame->ds;
    proc->es = 0x10;
    proc->fs = 0x10;
    proc->gs = 0x10;
    proc->ss = frame->ss;
    proc->priority = default_priority;
    proc->boost = 0;
    proc->time_slice = default_time_slice;
    proc->time_remaining = default_time_slice;
    proc->sched_class = SCHED_CLASS_CFS;
    proc->cpu_mask = 1;
    proc->cgroup_id = 0;
    proc->cgroup_share = 1024;
    proc->rt_budget = 0;
    proc->rt_period = 0;
    proc->rt_deadline = 0;
    proc->rt_runtime = 0;
    proc->rt_release = 0;
    proc->vruntime = 0;
    proc->wake_tick = 0;
    proc->runtime_ticks = 0;
    proc->ready_ticks = 0;
    proc->switches = 0;
    proc->parent_pid = proc->pid;
    proc->exit_code = 0;
    proc->exited = 0;
    proc->pending_signals = 0;
    proc->signal_mask = 0;
    proc->job_id = proc->pid;
    proc->session_id = proc->pid;
    proc->uid = 0;
    proc->gid = 0;
    proc->euid = 0;
    proc->egid = 0;
    proc->security_id = proc->pid;
    proc->caps = secure_caps_from_mask(0);
    proc->ns_pid = proc->pid;
    proc->ns_mount = 0;
    proc->ns_net = 0;
    proc->ns_user = 0;
    proc->container_id = 0;
    proc->vm_id = 0;
    for (uint32_t i = 0; i < PROCESS_MAX_FDS; ++i) {
        proc->fds[i].node = 0;
        proc->fds[i].offset = 0;
        proc->fds[i].flags = 0;
    }
    vm_init_process(proc);
    proc->wait_next = 0;

    process_count++;
    list_append(proc);
    if (!current_process) {
        current_process = proc;
        current_process->state = PROCESS_RUNNING;
    }
    return proc;
}

void scheduler_set_priority(uint32_t pid, uint32_t priority) {
    if (pid >= process_count) {
        return;
    }
    processes[pid].priority = priority;
}

void scheduler_set_timeslice(uint32_t pid, uint32_t ticks) {
    if (pid >= process_count) {
        return;
    }
    if (ticks == 0) {
        ticks = 1;
    }
    processes[pid].time_slice = ticks;
    if (processes[pid].time_remaining > ticks) {
        processes[pid].time_remaining = ticks;
    }
}

void scheduler_set_class(uint32_t pid, uint32_t sched_class) {
    if (pid >= process_count) {
        return;
    }
    processes[pid].sched_class = sched_class;
}

void scheduler_set_affinity(uint32_t pid, uint32_t cpu_mask) {
    if (pid >= process_count) {
        return;
    }
    processes[pid].cpu_mask = cpu_mask;
}

void scheduler_set_cgroup(uint32_t pid, uint32_t cgroup_id, uint32_t share) {
    if (pid >= process_count) {
        return;
    }
    processes[pid].cgroup_id = cgroup_id;
    processes[pid].cgroup_share = share == 0 ? 1 : share;
}

void scheduler_set_rt(uint32_t pid, uint32_t priority, uint64_t budget, uint64_t period) {
    if (pid >= process_count) {
        return;
    }
    process_t* proc = &processes[pid];
    proc->priority = priority;
    proc->sched_class = SCHED_CLASS_RT;
    proc->rt_budget = budget;
    proc->rt_period = period;
    proc->rt_deadline = period;
    proc->rt_runtime = 0;
    proc->rt_release = timer_get_ticks();
}

void scheduler_set_deadline(uint32_t pid, uint64_t budget, uint64_t period, uint64_t deadline) {
    if (pid >= process_count) {
        return;
    }
    process_t* proc = &processes[pid];
    proc->sched_class = SCHED_CLASS_DEADLINE;
    proc->rt_budget = budget;
    proc->rt_period = period;
    proc->rt_deadline = deadline ? deadline : period;
    proc->rt_runtime = 0;
    proc->rt_release = timer_get_ticks();
}

void scheduler_sleep(uint64_t ticks) {
    if (!current_process || ticks == 0) {
        return;
    }
    current_process->wake_tick = timer_get_ticks() + ticks;
    current_process->state = PROCESS_SLEEPING;
    current_process->time_remaining = 0;
    current_process->boost = 0;
    current_process->ready_ticks = 0;
}

int scheduler_wake(uint32_t pid) {
    if (pid >= process_count) {
        return 0;
    }
    process_t* proc = &processes[pid];
    if (proc->state != PROCESS_SLEEPING && proc->state != PROCESS_BLOCKED) {
        return 0;
    }
    proc->state = PROCESS_READY;
    proc->wake_tick = 0;
    proc->boost = 0;
    proc->ready_ticks = 0;
    proc->time_remaining = proc->time_slice;
    return 1;
}

void scheduler_yield(void) {
    if (!current_process) {
        return;
    }
    current_process->time_remaining = 0;
}

static void scheduler_wake_sleepers(uint64_t now) {
    if (!process_list) {
        return;
    }
    process_t* it = process_list;
    do {
        if (it->state == PROCESS_SLEEPING && it->wake_tick != 0 && now >= it->wake_tick) {
            it->state = PROCESS_READY;
            it->wake_tick = 0;
            it->boost = 0;
            it->ready_ticks = 0;
            it->time_remaining = it->time_slice;
        }
        it = it->next;
    } while (it != process_list);
}

static void scheduler_account_tick(void) {
    if (!process_list) {
        return;
    }
    uint64_t now = timer_get_ticks();
    process_t* it = process_list;
    do {
        if (it->state == PROCESS_READY) {
            it->ready_ticks++;
            if (it->boost < MAX_PRIORITY_BOOST) {
                it->boost++;
            }
        } else if (it->state == PROCESS_RUNNING) {
            it->runtime_ticks++;
            uint32_t share = it->cgroup_share ? it->cgroup_share : 1;
            it->vruntime += (1024u / share) + 1u;
            if ((it->sched_class == SCHED_CLASS_RT || it->sched_class == SCHED_CLASS_DEADLINE) && it->rt_period > 0) {
                if (now >= it->rt_release + it->rt_period) {
                    it->rt_release = now;
                    it->rt_runtime = 0;
                }
                it->rt_runtime++;
                if (it->rt_budget > 0 && it->rt_runtime >= it->rt_budget) {
                    it->state = PROCESS_SLEEPING;
                    it->wake_tick = it->rt_release + it->rt_period;
                    it->time_remaining = 0;
                    it->boost = 0;
                    it->ready_ticks = 0;
                }
            }
        }
        it = it->next;
    } while (it != process_list);
    scheduler_balance_load();
}

void scheduler_balance_load(void) {
    if (!process_list) {
        return;
    }
    uint32_t ready = 0;
    process_t* it = process_list;
    do {
        if (it->state == PROCESS_READY) {
            ready++;
        }
        it = it->next;
    } while (it != process_list);
    if (ready == 0) {
        return;
    }
    it = process_list;
    do {
        if (it->state == PROCESS_READY && it->boost > 0) {
            it->boost--;
        }
        it = it->next;
    } while (it != process_list);
}

static uint32_t scheduler_effective_priority(const process_t* proc) {
    uint32_t value = proc->priority + proc->boost;
    if (value < proc->priority) {
        value = proc->priority;
    }
    return value;
}

static process_t* scheduler_pick_best_ready(void) {
    if (!process_list) {
        return 0;
    }
    process_t* it = process_list;
    process_t* best = 0;
    uint32_t best_prio = 0;
    uint64_t best_vruntime = 0;
    uint64_t best_deadline = 0;
    uint64_t now = timer_get_ticks();
    do {
        if (it->state == PROCESS_READY && (it->cpu_mask & 1u)) {
            if (it->sched_class == SCHED_CLASS_DEADLINE) {
                if (it->rt_period > 0 && it->rt_budget > 0 && it->rt_runtime >= it->rt_budget && now < it->rt_release + it->rt_period) {
                    it = it->next;
                    continue;
                }
                uint64_t abs_deadline = it->rt_release + (it->rt_deadline ? it->rt_deadline : it->rt_period);
                if (!best || best->sched_class != SCHED_CLASS_DEADLINE || abs_deadline < best_deadline) {
                    best = it;
                    best_deadline = abs_deadline;
                }
            } else if (it->sched_class == SCHED_CLASS_RT) {
                uint32_t prio = scheduler_effective_priority(it);
                if (!best || best->sched_class != SCHED_CLASS_RT || prio > best_prio) {
                    best = it;
                    best_prio = prio;
                }
            } else {
                if (!best || best->sched_class == SCHED_CLASS_RT || best->sched_class == SCHED_CLASS_DEADLINE) {
                    best = it;
                    best_vruntime = it->vruntime;
                } else if (it->vruntime < best_vruntime || (it->vruntime == best_vruntime && it->ready_ticks > best->ready_ticks)) {
                    best = it;
                    best_vruntime = it->vruntime;
                }
            }
        }
        it = it->next;
    } while (it != process_list);
    return best;
}

process_t* switch_task(registers_t* saved_stack, process_t** previous) {
    uint64_t now = timer_get_ticks();
    scheduler_wake_sleepers(now);
    scheduler_account_tick();

    if (!current_process || !process_list) {
        if (previous) {
            *previous = current_process;
        }
        return current_process;
    }

    process_t* current = current_process;
    if (previous) {
        *previous = current;
    }
    signal_dispatch(current);
    current->esp = (uint32_t)saved_stack;
    current->edi = saved_stack->edi;
    current->esi = saved_stack->esi;
    current->ebp = saved_stack->ebp;
    current->ebx = saved_stack->ebx;
    current->edx = saved_stack->edx;
    current->ecx = saved_stack->ecx;
    current->eax = saved_stack->eax;
    current->eflags = saved_stack->eflags;
    current->eip = saved_stack->eip;
    current->cs = saved_stack->cs;
    current->ds = saved_stack->ds;
    current->ss = saved_stack->ss;

    if (current_process->state == PROCESS_RUNNING) {
        if (current_process->time_remaining > 0) {
            current_process->time_remaining--;
        }
    }

    process_t* best = scheduler_pick_best_ready();
    if (current_process->state == PROCESS_RUNNING) {
        if (current_process->time_remaining > 0) {
            uint32_t current_prio = scheduler_effective_priority(current_process);
            uint32_t best_prio = best ? scheduler_effective_priority(best) : 0;
            if (!best || best_prio <= current_prio) {
                return current_process;
            }
        } else {
            current_process->time_remaining = current_process->time_slice;
        }
        current_process->state = PROCESS_READY;
        current_process->boost = 0;
        current_process->ready_ticks = 0;
    }

    if (!best) {
        return current_process;
    }

    current_process = best;
    current_process->state = PROCESS_RUNNING;
    current_process->time_remaining = current_process->time_slice;
    current_process->boost = 0;
    current_process->ready_ticks = 0;
    current_process->switches++;

    return current_process;
}

process_t* process_fork(process_t* parent) {
    if (!parent) {
        parent = current_process;
    }
    if (!parent) {
        return 0;
    }
    process_t* child = process_create((void (*)(void))parent->eip, 0);
    if (!child) {
        return 0;
    }
    child->parent_pid = parent->pid;
    child->priority = parent->priority;
    child->sched_class = parent->sched_class;
    child->cpu_mask = parent->cpu_mask;
    child->cgroup_id = parent->cgroup_id;
    child->cgroup_share = parent->cgroup_share;
    child->rt_budget = parent->rt_budget;
    child->rt_period = parent->rt_period;
    child->rt_deadline = parent->rt_deadline;
    child->rt_runtime = 0;
    child->rt_release = parent->rt_release;
    child->vruntime = parent->vruntime;
    child->uid = parent->uid;
    child->gid = parent->gid;
    child->euid = parent->euid;
    child->egid = parent->egid;
    child->security_id = parent->security_id;
    child->caps = parent->caps;
    child->ns_pid = parent->ns_pid;
    child->ns_mount = parent->ns_mount;
    child->ns_net = parent->ns_net;
    child->ns_user = parent->ns_user;
    child->container_id = parent->container_id;
    child->vm_id = parent->vm_id;
    if (!vm_space_clone_cow(parent, child)) {
        return 0;
    }
    child->eax = 0;
    return child;
}

int process_exec(process_t* proc, void (*entry)(void)) {
    if (!proc || !entry) {
        return 0;
    }
    uint32_t pid = proc->pid;
    uint32_t stack_top = (uint32_t)&stacks[pid][STACK_SIZE];
    registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
    frame->ds = 0x10;
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = stack_top;
    frame->esp = stack_top;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;
    frame->int_no = 0;
    frame->err_code = 0;
    frame->eip = (uint32_t)entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->useresp = 0;
    frame->ss = 0;

    proc->eax = frame->eax;
    proc->ebx = frame->ebx;
    proc->ecx = frame->ecx;
    proc->edx = frame->edx;
    proc->esi = frame->esi;
    proc->edi = frame->edi;
    proc->esp = (uint32_t)frame;
    proc->ebp = frame->ebp;
    proc->eip = frame->eip;
    proc->eflags = frame->eflags;
    proc->cs = frame->cs;
    proc->ds = frame->ds;
    proc->es = 0x10;
    proc->fs = 0x10;
    proc->gs = 0x10;
    proc->ss = frame->ss;
    return 1;
}

void process_exit(process_t* proc, uint32_t code) {
    if (!proc) {
        return;
    }
    proc->exit_code = code;
    proc->exited = 1;
    proc->state = PROCESS_BLOCKED;
    if (proc->parent_pid < process_count) {
        wait_queue_wake_one(&process_wait_queues[proc->parent_pid]);
    }
}

int process_wait(uint32_t pid, uint32_t* exit_code) {
    if (pid >= process_count) {
        return 0;
    }
    for (uint32_t i = 0; i < process_count; ++i) {
        process_t* proc = &processes[i];
        if (proc->parent_pid == pid && proc->exited) {
            if (exit_code) {
                *exit_code = proc->exit_code;
            }
            proc->exited = 0;
            return 1;
        }
    }
    wait_queue_wait(&process_wait_queues[pid]);
    return 0;
}

void wait_queue_init(wait_queue_t* queue) {
    if (!queue) {
        return;
    }
    queue->head = 0;
    queue->tail = 0;
}

void wait_queue_wait(wait_queue_t* queue) {
    if (!queue || !current_process) {
        return;
    }
    current_process->state = PROCESS_BLOCKED;
    current_process->wait_next = 0;
    if (!queue->head) {
        queue->head = current_process;
        queue->tail = current_process;
    } else {
        queue->tail->wait_next = current_process;
        queue->tail = current_process;
    }
    current_process->time_remaining = 0;
}

process_t* wait_queue_wake_one(wait_queue_t* queue) {
    if (!queue || !queue->head) {
        return 0;
    }
    process_t* proc = queue->head;
    queue->head = proc->wait_next;
    if (!queue->head) {
        queue->tail = 0;
    }
    proc->wait_next = 0;
    proc->state = PROCESS_READY;
    proc->time_remaining = proc->time_slice;
    return proc;
}

uint32_t wait_queue_wake_all(wait_queue_t* queue) {
    if (!queue) {
        return 0;
    }
    uint32_t count = 0;
    while (queue->head) {
        wait_queue_wake_one(queue);
        count++;
    }
    return count;
}
