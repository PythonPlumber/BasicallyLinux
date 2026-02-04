#include "paging.h"
#include "sched.h"
#include "secure_caps.h"
#include "signal.h"
#include "smp_rally.h"
#include "timer.h"
#include "types.h"
#include "vm_space.h"
#include "heap.h"
#include "util.h"
#include "elf_loader.h"

#define STACK_SIZE 4096
#define MAX_PRIORITY_BOOST 8
#define MAX_CPUS 4

#define LAPIC_BASE 0xFEE00000
#define LAPIC_ID   0x0020
#define LAPIC_VER  0x0030
#define LAPIC_TPR  0x0080
#define LAPIC_EOI  0x00B0
#define LAPIC_SVR  0x00F0

typedef struct {
    process_t* head;
    process_t* tail;
    uint32_t count;
    spinlock_t lock;
} runqueue_t;

static runqueue_t runqueues[MAX_CPUS];

static inline uint32_t get_cpu_id(void) {
    // Read LAPIC ID (bits 24-31)
    // We assume LAPIC is mapped at default address
    if ((*(volatile uint32_t*)(LAPIC_BASE + LAPIC_SVR)) & 0x100) {
        return (*(volatile uint32_t*)(LAPIC_BASE + LAPIC_ID)) >> 24;
    }
    return 0;
}

// Global runqueue lock
static spinlock_t sched_lock = 0;

// List of all processes (for accounting/cleanup)
static process_t* process_list = 0;
static uint32_t process_count = 0;

// Current process (Per-CPU)
static process_t* current_process[MAX_CPUS];

static uint32_t default_priority = 10;
static uint32_t default_time_slice = 4;

process_t* scheduler_current(void) {
    return current_process[get_cpu_id()];
}

process_t* scheduler_process_list(void) {
    return process_list;
}

uint32_t scheduler_process_count(void) {
    return process_count;
}

// Helper: Remove from list
static void list_remove(process_t* proc) {
    if (!process_list || !proc) return;
    
    if (proc->next == proc) {
        if (process_list == proc) {
            process_list = 0;
        }
    } else {
        process_t* tail = process_list;
        while (tail->next != proc) {
            tail = tail->next;
        }
        tail->next = proc->next;
        if (process_list == proc) {
            process_list = proc->next;
        }
    }
}

// Helper: Append to list
static void list_append(process_t* proc) {
    if (!process_list) {
        process_list = proc;
        proc->next = proc;
    } else {
        process_t* tail = process_list;
        while (tail->next != process_list) {
            tail = tail->next;
        }
        tail->next = proc;
        proc->next = process_list;
    }
}

static void enqueue_task(process_t* proc) {
    uint32_t best_cpu = 0;
    uint32_t min_count = 0xFFFFFFFF;
    uint32_t online = smp_rally_online_mask();
    
    // Find best CPU among online ones in mask
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        if ((online & (1 << i)) && (proc->cpu_mask & (1 << i)) && runqueues[i].count < min_count) {
            min_count = runqueues[i].count;
            best_cpu = i;
        }
    }
    
    // Fallback if mask is invalid
    if (!(proc->cpu_mask & (1 << best_cpu))) {
        best_cpu = 0;
    }
    
    runqueue_t* rq = &runqueues[best_cpu];
    
    spin_lock(&rq->lock);
    
    proc->run_next = 0;
    proc->current_cpu = best_cpu;
    
    if (!rq->head) {
        rq->head = proc;
        rq->tail = proc;
    } else {
        rq->tail->run_next = proc;
        rq->tail = proc;
    }
    rq->count++;
    
    spin_unlock(&rq->lock);
}

static void dequeue_task(process_t* proc) {
    uint32_t cpu = proc->current_cpu;
    if (cpu >= MAX_CPUS) cpu = 0;
    
    runqueue_t* rq = &runqueues[cpu];
    
    spin_lock(&rq->lock);
    
    if (!rq->head) {
        spin_unlock(&rq->lock);
        return;
    }
    
    if (rq->head == proc) {
        rq->head = proc->run_next;
        if (!rq->head) rq->tail = 0;
        rq->count--;
    } else {
        process_t* prev = rq->head;
        while (prev->run_next && prev->run_next != proc) {
            prev = prev->run_next;
        }
        if (prev->run_next == proc) {
            prev->run_next = proc->run_next;
            if (rq->tail == proc) rq->tail = prev;
            rq->count--;
        }
    }
    proc->run_next = 0;
    
    spin_unlock(&rq->lock);
}

static process_t* find_process_by_pid(uint32_t pid) {
    if (!process_list) return 0;
    process_t* it = process_list;
    do {
        if (it->pid == pid) return it;
        it = it->next;
    } while (it != process_list);
    return 0;
}

int scheduler_kill(uint32_t pid) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (!proc) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    if (proc->state == PROCESS_READY) {
        dequeue_task(proc);
    }
    
    proc->state = PROCESS_BLOCKED;
    proc->wake_tick = 0;
    proc->time_remaining = 0;
    proc->exited = 1;
    proc->exit_code = -1; // Killed
    
    // Clean up resources if needed immediately, or let parent wait()
    // For now, we just mark it as zombie/blocked
    
    // If we killed the current process, we must schedule soon.
    // But we can't switch here inside lock easily without care.
    // The timer tick will pick up the change.
    
    spin_unlock(&sched_lock);
    return 1;
}

void scheduler_init(void) {
    process_count = 0;
    process_list = 0;
    sched_lock = 0;
    
    for (int i = 0; i < MAX_CPUS; i++) {
        runqueues[i].head = 0;
        runqueues[i].tail = 0;
        runqueues[i].count = 0;
        runqueues[i].lock = 0;
        current_process[i] = 0;
    }
}

static process_t* process_create_ex(void (*entry)(void), uint32_t* page_directory, int start_immediately) {
    spin_lock(&sched_lock);
    
    uint32_t pid = process_count++; // Simple PID alloc
    
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) {
        spin_unlock(&sched_lock);
        return 0;
    }
    memset(proc, 0, sizeof(process_t));
    
    void* stack_ptr = kmalloc(STACK_SIZE);
    if (!stack_ptr) {
        kfree(proc);
        spin_unlock(&sched_lock);
        return 0;
    }
    proc->kernel_stack = stack_ptr;
    
    proc->pid = pid;
    proc->state = PROCESS_BLOCKED; // Default to blocked until decided
    
    if (!page_directory) {
        page_directory = vm_space_create();
    }
    if (!page_directory) {
        page_directory = paging_get_directory();
    }
    proc->page_directory = page_directory;
    
    uint32_t stack_top = (uint32_t)stack_ptr + STACK_SIZE;
    registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
    
    memset(frame, 0, sizeof(registers_t));
    frame->ds = 0x10;
    frame->es = 0x10;
    frame->fs = 0x10;
    frame->gs = 0x10;
    frame->ebp = stack_top;
    frame->esp = stack_top;
    frame->eip = (uint32_t)entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->ss = 0; // Kernel stack ss is ignored usually

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
    proc->cpu_mask = 0xFFFFFFFF;
    proc->cgroup_share = 1024;
    
    proc->parent_pid = proc->pid;
    proc->job_id = proc->pid;
    proc->session_id = proc->pid;
    proc->security_id = proc->pid;
    proc->caps = secure_caps_from_mask(0);
    proc->ns_pid = proc->pid;
    
    vm_init_process(proc);
    
    list_append(proc);
    
    if (start_immediately) {
        process_t* current = scheduler_current();
        if (!current) {
            uint32_t cpu = get_cpu_id();
            current_process[cpu] = proc;
            proc->current_cpu = cpu;
            proc->state = PROCESS_RUNNING;
        } else {
            proc->state = PROCESS_READY;
            enqueue_task(proc);
        }
    }
    
    spin_unlock(&sched_lock);
    return proc;
}

process_t* process_create(void (*entry)(void), uint32_t* page_directory) {
    return process_create_ex(entry, page_directory, 1);
}

void scheduler_set_priority(uint32_t pid, uint32_t priority) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        proc->priority = priority;
    }
    spin_unlock(&sched_lock);
}

void scheduler_set_timeslice(uint32_t pid, uint32_t ticks) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        if (ticks == 0) ticks = 1;
        proc->time_slice = ticks;
        if (proc->time_remaining > ticks) {
            proc->time_remaining = ticks;
        }
    }
    spin_unlock(&sched_lock);
}

void scheduler_set_class(uint32_t pid, uint32_t sched_class) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        proc->sched_class = sched_class;
    }
    spin_unlock(&sched_lock);
}

void scheduler_set_affinity(uint32_t pid, uint32_t cpu_mask) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        proc->cpu_mask = cpu_mask;
    }
    spin_unlock(&sched_lock);
}

void scheduler_set_cgroup(uint32_t pid, uint32_t cgroup_id, uint32_t share) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        proc->cgroup_id = cgroup_id;
        proc->cgroup_share = share == 0 ? 1 : share;
    }
    spin_unlock(&sched_lock);
}

void scheduler_set_rt(uint32_t pid, uint32_t priority, uint64_t budget, uint64_t period) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        proc->priority = priority;
        proc->sched_class = SCHED_CLASS_RT;
        proc->rt_budget = budget;
        proc->rt_period = period;
        proc->rt_deadline = period;
        proc->rt_runtime = 0;
        proc->rt_release = timer_get_ticks();
    }
    spin_unlock(&sched_lock);
}

void scheduler_set_deadline(uint32_t pid, uint64_t budget, uint64_t period, uint64_t deadline) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (proc) {
        proc->sched_class = SCHED_CLASS_DEADLINE;
        proc->rt_budget = budget;
        proc->rt_period = period;
        proc->rt_deadline = deadline ? deadline : period;
        proc->rt_runtime = 0;
        proc->rt_release = timer_get_ticks();
    }
    spin_unlock(&sched_lock);
}

void scheduler_sleep(uint64_t ticks) {
    // Cannot sleep without current process
    process_t* current = scheduler_current();
    if (!current) return;

    spin_lock(&sched_lock);
    if (ticks > 0) {
        current->wake_tick = timer_get_ticks() + ticks;
        current->state = PROCESS_SLEEPING;
        current->time_remaining = 0;
        current->boost = 0;
        current->ready_ticks = 0;
    }
    spin_unlock(&sched_lock);
    
    // The next interrupt/yield will switch us out
}

int scheduler_wake(uint32_t pid) {
    spin_lock(&sched_lock);
    process_t* proc = find_process_by_pid(pid);
    if (!proc) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    if (proc->state != PROCESS_SLEEPING && proc->state != PROCESS_BLOCKED) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    proc->state = PROCESS_READY;
    proc->wake_tick = 0;
    proc->boost = 0;
    proc->ready_ticks = 0;
    proc->time_remaining = proc->time_slice;
    enqueue_task(proc);
    spin_unlock(&sched_lock);
    return 1;
}

void scheduler_yield(void) {
    process_t* current = scheduler_current();
    if (!current) return;
    
    spin_lock(&sched_lock);
    current->time_remaining = 0;
    spin_unlock(&sched_lock);
    
    // Force a switch
    asm volatile("int $0x20"); // Assuming timer vector, or just wait for tick
}

static void scheduler_wake_sleepers(uint64_t now) {
    if (!process_list) return;
    process_t* it = process_list;
    do {
        if (it->state == PROCESS_SLEEPING && it->wake_tick != 0 && now >= it->wake_tick) {
            it->state = PROCESS_READY;
            it->wake_tick = 0;
            it->boost = 0;
            it->ready_ticks = 0;
            it->time_remaining = it->time_slice;
            enqueue_task(it);
        }
        it = it->next;
    } while (it != process_list);
}

static void scheduler_account_tick(void) {
    if (!process_list) return;
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
}

void scheduler_balance_load(void) {
    uint32_t this_cpu = get_cpu_id();
    runqueue_t* this_rq = &runqueues[this_cpu];
    
    // Only balance if we are empty
    if (this_rq->count > 0) return;
    
    uint32_t busiest_cpu = this_cpu;
    uint32_t max_count = 0;
    uint32_t online = smp_rally_online_mask();
    
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        if ((online & (1 << i)) && runqueues[i].count > max_count) {
            max_count = runqueues[i].count;
            busiest_cpu = i;
        }
    }
    
    if (busiest_cpu == this_cpu || max_count <= 1) return;
    
    runqueue_t* remote_rq = &runqueues[busiest_cpu];
    
    // Lock ordering to prevent deadlocks
    uint32_t id1 = (this_cpu < busiest_cpu) ? this_cpu : busiest_cpu;
    uint32_t id2 = (this_cpu < busiest_cpu) ? busiest_cpu : this_cpu;
    
    spin_lock(&runqueues[id1].lock);
    if (id1 != id2) spin_lock(&runqueues[id2].lock);
    
    // Try to steal from head for O(1)
    process_t* victim = remote_rq->head;
    if (victim && victim->state == PROCESS_READY && (victim->cpu_mask & (1 << this_cpu))) {
        // Steal it
        remote_rq->head = victim->run_next;
        if (!remote_rq->head) remote_rq->tail = 0;
        remote_rq->count--;
        
        // Add to local
        victim->run_next = 0;
        victim->current_cpu = this_cpu;
        
        if (!this_rq->head) {
            this_rq->head = victim;
            this_rq->tail = victim;
        } else {
            this_rq->tail->run_next = victim;
            this_rq->tail = victim;
        }
        this_rq->count++;
    }
    
    if (id1 != id2) spin_unlock(&runqueues[id2].lock);
    spin_unlock(&runqueues[id1].lock);
}

static uint32_t scheduler_effective_priority(const process_t* proc) {
    uint32_t value = proc->priority + proc->boost;
    if (value < proc->priority) {
        value = proc->priority;
    }
    return value;
}

static process_t* scheduler_pick_best_ready(void) {
    uint32_t cpu = get_cpu_id();
    runqueue_t* rq = &runqueues[cpu];
    
    spin_lock(&rq->lock);
    
    if (!rq->head) {
        spin_unlock(&rq->lock);
        return 0;
    }
    
    process_t* it = rq->head;
    process_t* best = 0;
    uint32_t best_prio = 0;
    uint64_t best_vruntime = 0;
    uint64_t best_deadline = 0;
    uint64_t now = timer_get_ticks();
    
    while (it) {
        // Deadline Logic
        if (it->sched_class == SCHED_CLASS_DEADLINE) {
            // Check if blocked by budget
            if (it->rt_period > 0 && it->rt_budget > 0 && it->rt_runtime >= it->rt_budget && now < it->rt_release + it->rt_period) {
                it = it->run_next;
                continue;
            }
            
            uint64_t abs_deadline = it->rt_release + (it->rt_deadline ? it->rt_deadline : it->rt_period);
            if (!best || best->sched_class != SCHED_CLASS_DEADLINE || abs_deadline < best_deadline) {
                best = it;
                best_deadline = abs_deadline;
            }
        } 
        // RT Logic
        else if (it->sched_class == SCHED_CLASS_RT) {
            if (best && best->sched_class == SCHED_CLASS_DEADLINE) {
                // Deadline trumps RT
            } else {
                uint32_t prio = scheduler_effective_priority(it);
                if (!best || best->sched_class != SCHED_CLASS_RT || prio > best_prio) {
                    best = it;
                    best_prio = prio;
                }
            }
        } 
        // CFS Logic
        else {
            if (best && (best->sched_class == SCHED_CLASS_RT || best->sched_class == SCHED_CLASS_DEADLINE)) {
                // RT/Deadline trumps CFS
            } else {
                if (!best) {
                    best = it;
                    best_vruntime = it->vruntime;
                } else if (it->vruntime < best_vruntime || (it->vruntime == best_vruntime && it->ready_ticks > best->ready_ticks)) {
                    best = it;
                    best_vruntime = it->vruntime;
                }
            }
        }
        it = it->run_next;
    }
    
    spin_unlock(&rq->lock);
    
    return best;
}

process_t* switch_task(registers_t* saved_stack, process_t** previous) {
    spin_lock(&sched_lock);
    
    uint32_t cpu = get_cpu_id();
    process_t* current = current_process[cpu];
    
    uint64_t now = timer_get_ticks();
    scheduler_wake_sleepers(now);
    scheduler_account_tick();

    if (!current || !process_list) {
        if (previous) *previous = current;
        spin_unlock(&sched_lock);
        return current;
    }

    if (previous) *previous = current;
    
    signal_dispatch(current);
    
    // Save context
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
    current->es = saved_stack->es;
    current->fs = saved_stack->fs;
    current->gs = saved_stack->gs;
    current->ss = saved_stack->ss;

    if (current->state == PROCESS_RUNNING) {
        if (current->time_remaining > 0) {
            current->time_remaining--;
        }
    }

    process_t* best = scheduler_pick_best_ready();
    
    // SMP Load Balance if idle
    if (!best) {
        scheduler_balance_load();
        best = scheduler_pick_best_ready();
    }
    
    // Preemption Logic
    if (current->state == PROCESS_RUNNING) {
        // If we still have time, check if someone BETTER showed up (preemption)
        if (current->time_remaining > 0) {
            uint32_t current_prio = scheduler_effective_priority(current);
            uint32_t best_prio = best ? scheduler_effective_priority(best) : 0;
            
            int switch_needed = 0;
            if (best) {
                // Priority class overrides
                if (best->sched_class > current->sched_class) switch_needed = 1; 
                else if (best->sched_class == current->sched_class) {
                     if (best->sched_class == SCHED_CLASS_RT && best_prio > current_prio) switch_needed = 1;
                     else if (best->sched_class == SCHED_CLASS_CFS && best->vruntime < current->vruntime) switch_needed = 1;
                }
            }
            
            if (!switch_needed) {
                spin_unlock(&sched_lock);
                return current;
            }
        } else {
            // Time slice expired
            current->time_remaining = current->time_slice;
        }
        current->state = PROCESS_READY;
        current->boost = 0;
        current->ready_ticks = 0;
        enqueue_task(current);
    }

    if (!best) {
        // Idle?
        spin_unlock(&sched_lock);
        return current;
    }

    dequeue_task(best);
    current_process[cpu] = best;
    best->state = PROCESS_RUNNING;
    best->time_remaining = best->time_slice;
    best->boost = 0;
    best->ready_ticks = 0;
    best->switches++;

    spin_unlock(&sched_lock);
    return best;
}

process_t* process_fork(process_t* parent, registers_t* regs) {
    if (!parent) parent = scheduler_current();
    if (!parent) return 0;
    
    // Create blocked child with dummy entry, we will overwrite the stack frame
    process_t* child = process_create_ex((void (*)(void))regs->eip, 0, 0);
    if (!child) return 0;
    
    spin_lock(&sched_lock);
    
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
        // Failed to clone memory
        if (child->kernel_stack) kfree(child->kernel_stack);
        kfree(child);
        spin_unlock(&sched_lock);
        return 0;
    }
    
    // Setup child stack frame to match parent's state at syscall
    if (child->kernel_stack) {
        registers_t* child_frame = (registers_t*)child->esp;
        // Copy registers from the syscall regs
        *child_frame = *regs;
        // Fork returns 0 to child
        child_frame->eax = 0;
    }
    
    child->eax = 0; // Fork returns 0 to child
    
    // Now make it ready
    child->state = PROCESS_READY;
    child->wake_tick = 0;
    child->boost = 0;
    child->ready_ticks = 0;
    child->time_remaining = child->time_slice;
    enqueue_task(child);
    
    spin_unlock(&sched_lock);
    return child;
}

registers_t* process_exec(process_t* proc, void (*entry)(void)) {
    spin_lock(&sched_lock);
    if (!proc || !entry) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    // Reset stack
    if (proc->kernel_stack) {
        uint32_t stack_top = (uint32_t)proc->kernel_stack + STACK_SIZE;
        
        // Reset stack frame
        registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
        memset(frame, 0, sizeof(registers_t));
        
        frame->ds = 0x10;
        frame->es = 0x10;
        frame->fs = 0x10;
        frame->gs = 0x10;
        frame->ebp = stack_top;
        frame->esp = stack_top;
        frame->eip = (uint32_t)entry;
        frame->cs = 0x08;
        frame->eflags = 0x202;
        frame->ss = 0;
        
        // Update process registers
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
        
        proc->eax = 0;
        proc->ebx = 0;
        proc->ecx = 0;
        proc->edx = 0;
        proc->esi = 0;
        proc->edi = 0;
        
        spin_unlock(&sched_lock);
        return frame;
    } else {
        proc->eip = (uint32_t)entry;
        spin_unlock(&sched_lock);
        return 0; // Should have kernel stack
    }
}

registers_t* process_exec_elf(process_t* proc, const uint8_t* elf_data, uint32_t size) {
    spin_lock(&sched_lock);
    if (!proc || !elf_data) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    // Parse ELF
    elf_image_t image;
    if (!elf_loader_parse(elf_data, size, &image)) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    // Reset regions
    proc->region_count = 0;
    
    // Map segments
    const Elf32_Ehdr* hdr = (const Elf32_Ehdr*)elf_data;
    const Elf32_Phdr* phdr = (const Elf32_Phdr*)(elf_data + hdr->e_phoff);
    
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // Force Write for loading, ideally we remap later
            uint32_t flags = VM_USER | VM_READ | VM_WRITE | VM_EXEC;
            
            uint32_t vaddr = phdr[i].p_vaddr & 0xFFFFF000;
            uint32_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
            uint32_t size_aligned = ((end + 0xFFF) & 0xFFFFF000) - vaddr;
            
            if (!vm_map_region(proc, vaddr, size_aligned, flags)) {
                spin_unlock(&sched_lock);
                return 0;
            }
        }
    }
    
    // Map User Stack
    uint32_t stack_size = 0x4000;
    uint32_t stack_base = 0xBFFFC000;
    if (!vm_map_region(proc, stack_base, stack_size, VM_USER | VM_WRITE | VM_READ)) {
         spin_unlock(&sched_lock);
         return 0;
    }
    
    // Load Data
    if (proc == scheduler_current()) {
        // Flush TLB to ensure new mappings are visible
        load_cr3(proc->page_directory);
        
        if (!elf_loader_load(elf_data, size, &image)) {
             spin_unlock(&sched_lock);
             return 0;
        }
    } else {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    // Setup Context
    if (proc->kernel_stack) {
        uint32_t kstack_top = (uint32_t)proc->kernel_stack + STACK_SIZE;
        registers_t* frame = (registers_t*)(kstack_top - sizeof(registers_t));
        memset(frame, 0, sizeof(registers_t));
        
        frame->ds = 0x23;
        frame->es = 0x23;
        frame->fs = 0x23;
        frame->gs = 0x23;
        frame->ss = 0x23;
        frame->esp = stack_base + stack_size;
        frame->ebp = frame->esp;
        frame->cs = 0x1B;
        frame->eip = image.entry;
        frame->eflags = 0x202;
        
        proc->esp = (uint32_t)frame;
        proc->eip = frame->eip;
        
        spin_unlock(&sched_lock);
        return frame;
    }
    
    spin_unlock(&sched_lock);
    return 0;
}

void process_exit(process_t* proc, uint32_t code) {
    if (!proc) return;
    
    spin_lock(&sched_lock);
    proc->exit_code = code;
    proc->exited = 1;
    proc->state = PROCESS_BLOCKED;
    
    // Wake parent
    process_t* parent = find_process_by_pid(proc->parent_pid);
    if (parent) {
        // Wake up all processes waiting on the parent (which might include the parent itself waiting for children)
        // Actually, the parent sleeps on its OWN wait_queue?
        // Usually wait_queue is "queue of processes waiting for THIS event".
        // So `parent->wait_queue` should be "processes waiting for parent"?
        // No, typically `process_wait` sleeps on a queue.
        // If `process_wait` sleeps on `current->wait_queue`, then `current` is the one waiting.
        // So we need to wake `parent` which is in `parent->wait_queue`?
        // No. `wait_queue_wait` puts `current` into `queue`.
        // So if parent calls `wait_queue_wait(&parent->wait_queue)`, parent is in `parent->wait_queue`.
        // So we wake `parent->wait_queue`.
        
        wait_queue_t* queue = &parent->wait_queue;
        if (queue->head) {
             process_t* p = queue->head;
             while (p) {
                 process_t* next = p->wait_next;
                 p->wait_next = 0;
                 p->state = PROCESS_READY;
                 enqueue_task(p);
                 p = next;
             }
             queue->head = 0;
             queue->tail = 0;
        }
    }
    
    spin_unlock(&sched_lock);
    
    // If we are killing ourselves
    if (proc == scheduler_current()) {
        scheduler_yield();
    }
}

int process_wait(uint32_t pid, uint32_t* exit_code) {
    while (1) {
        spin_lock(&sched_lock);
        
        process_t* current = scheduler_current();
        if (!current) {
            spin_unlock(&sched_lock);
            return -1;
        }

        process_t* child = 0;
        if (pid == -1) {
            // Any child
            process_t* it = process_list;
            if (it) {
                do {
                    if (it->parent_pid == current->pid) {
                        if (it->exited) {
                            child = it;
                            break;
                        }
                        // Found a living child, remember we have children
                        child = (process_t*)1; 
                    }
                    it = it->next;
                } while (it != process_list);
            }
        } else {
            child = find_process_by_pid(pid);
            if (child && child->parent_pid != current->pid) {
                child = 0; // Not our child
            }
        }
        
        if (!child) {
            spin_unlock(&sched_lock);
            return -1; // No children
        }
        
        if (child != (process_t*)1 && child->exited) {
            // Found zombie
            if (exit_code) *exit_code = child->exit_code;
            
            // Remove from list
            list_remove(child);
            process_count--;
            
            // Free memory
            if (child->kernel_stack) {
                kfree(child->kernel_stack);
            }
            
            kfree(child);
            
            spin_unlock(&sched_lock);
            return pid == -1 ? child->pid : pid;
        }
        
        // Child exists but running, sleep
        wait_queue_t* queue = &current->wait_queue;
        current->state = PROCESS_BLOCKED;
        current->wait_next = 0;
        
        if (!queue->head) {
            queue->head = current;
            queue->tail = current;
        } else {
            queue->tail->wait_next = current;
            queue->tail = current;
        }
        
        spin_unlock(&sched_lock);
        scheduler_yield();
    }
}

void wait_queue_init(wait_queue_t* queue) {
    if (queue) {
        queue->head = 0;
        queue->tail = 0;
    }
}

void wait_queue_wait(wait_queue_t* queue) {
    process_t* current = scheduler_current();
    if (!current) return;
    
    spin_lock(&sched_lock);
    current->state = PROCESS_BLOCKED;
    current->wait_next = 0;
    
    if (!queue->head) {
        queue->head = current;
        queue->tail = current;
    } else {
        queue->tail->wait_next = current;
        queue->tail = current;
    }
    spin_unlock(&sched_lock);
    
    scheduler_yield();
}

process_t* wait_queue_wake_one(wait_queue_t* queue) {
    spin_lock(&sched_lock);
    if (!queue || !queue->head) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    process_t* proc = queue->head;
    queue->head = proc->wait_next;
    if (!queue->head) queue->tail = 0;
    
    proc->wait_next = 0;
    proc->state = PROCESS_READY;
    enqueue_task(proc);
    
    spin_unlock(&sched_lock);
    return proc;
}

uint32_t wait_queue_wake_all(wait_queue_t* queue) {
    spin_lock(&sched_lock);
    if (!queue || !queue->head) {
        spin_unlock(&sched_lock);
        return 0;
    }
    
    uint32_t count = 0;
    process_t* proc = queue->head;
    while (proc) {
        process_t* next = proc->wait_next;
        proc->wait_next = 0;
        proc->state = PROCESS_READY;
        enqueue_task(proc);
        proc = next;
        count++;
    }
    queue->head = 0;
    queue->tail = 0;
    
    spin_unlock(&sched_lock);
    return count;
}
