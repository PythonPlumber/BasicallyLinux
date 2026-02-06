#ifndef PROCESS_H
#define PROCESS_H

#include "security/secure_caps.h"
#include "types.h"

struct process;

typedef struct wait_queue {
    struct process* head;
    struct process* tail;
} wait_queue_t;

typedef struct fs_node fs_node_t;

typedef struct {
    fs_node_t* node;
    uint32_t offset;
    uint32_t flags;
} file_desc_t;

#define PROCESS_MAX_FDS 8
#define PROCESS_MAX_REGIONS 16

typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t flags;
    uint32_t shared_id;
} vm_region_t;

typedef enum {
    PROCESS_READY = 0,
    PROCESS_RUNNING = 1,
    PROCESS_BLOCKED = 2,
    PROCESS_SLEEPING = 3
} process_state_t;

#define SCHED_CLASS_CFS 0u
#define SCHED_CLASS_RT 1u
#define SCHED_CLASS_DEADLINE 2u

typedef struct process {
    uint32_t pid;
    process_state_t state;
    uint32_t* page_directory;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    uint32_t eflags;
    uint32_t cs;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t ss;
    uint32_t priority;
    uint32_t boost;
    uint32_t time_slice;
    uint32_t time_remaining;
    uint32_t sched_class;
    uint32_t cpu_mask;
    uint32_t current_cpu;
    uint32_t cgroup_id;
    uint32_t cgroup_share;
    uint64_t rt_budget;
    uint64_t rt_period;
    uint64_t rt_deadline;
    uint64_t rt_runtime;
    uint64_t rt_release;
    uint64_t vruntime;
    uint64_t wake_tick;
    uint64_t runtime_ticks;
    uint64_t ready_ticks;
    uint32_t switches;
    uint32_t parent_pid;
    uint32_t exit_code;
    uint32_t exited;
    uint32_t pending_signals;
    uint32_t signal_mask;
    uint32_t job_id;
    uint32_t session_id;
    uint32_t uid;
    uint32_t gid;
    uint32_t euid;
    uint32_t egid;
    uint32_t security_id;
    secure_caps_t caps;
    uint32_t ns_pid;
    uint32_t ns_mount;
    uint32_t ns_net;
    uint32_t ns_user;
    uint32_t container_id;
    uint32_t vm_id;
    void* kernel_stack;
    file_desc_t fds[PROCESS_MAX_FDS];
    uint32_t region_count;
    vm_region_t regions[PROCESS_MAX_REGIONS];
    struct process* next;
    struct process* wait_next;
    struct process* run_next;
    wait_queue_t wait_queue;
} process_t;

#endif
