# Process & Scheduler Internals

## 1. Overview
The Kernel employs a preemptive, multi-class scheduler supporting SMP (Symmetric Multiprocessing). It handles task switching, CPU load balancing, and real-time constraints using a modular scheduling class architecture.

## 2. Process Model (`process_t`)
The `process_t` structure is the core entity, containing the execution context, memory map, and resource limits.

### Data Structures
```c
// Core Process Control Block
typedef struct process {
    uint32_t pid;               // Process ID
    uint32_t state;             // RUNNING, SLEEPING, ZOMBIE
    uint32_t flags;             // KERNEL_THREAD, REALTIME

    // Execution Context
    regs_t context;             // Saved registers
    uintptr_t kernel_stack;     // Kernel stack pointer (ESP0)
    page_directory_t* cr3;      // Page Directory (Physical Address)

    // Scheduling
    struct sched_entity se;     // Scheduling entity info
    struct sched_class* class;  // Pointer to handling class
    
    // Links
    struct process* parent;
    struct list_head children;
    struct list_head siblings;
    
    // Resources
    file_descriptor_t* fds[MAX_FDS];
    signal_state_t signals;
} process_t;

// Scheduling Entity (Generic)
struct sched_entity {
    uint32_t priority;          // Static priority
    uint64_t vruntime;          // Virtual Runtime (CFS)
    uint64_t time_slice;        // Remaining slice (RR)
    uint64_t deadline;          // Absolute deadline (EDF)
    struct list_head run_list;  // Link to runqueue
};
```

### Lifecycle Diagram
```ascii
      fork()          scheduler_pick()
  +---> [ NEW ] ---------------------> [ READY ] <----+
  |                                      ^  |         |
Create                                   |  | schedule()
  |                                      |  v         | preempt()
  +---------------------------------- [ RUNNING ] ----+
                                         |
                                         | block() / wait()
                                         v
                                     [ BLOCKED ]
```

## 3. Scheduling Classes Implementation
The scheduler uses a virtual function table (VTable) approach to support multiple scheduling policies.

### Scheduler Class Interface
```c
struct sched_class {
    const char* name;
    
    // Core API
    void (*enqueue_task)(struct runqueue* rq, process_t* p);
    void (*dequeue_task)(struct runqueue* rq, process_t* p);
    void (*yield_task)(struct runqueue* rq);
    
    // Selection
    process_t* (*pick_next_task)(struct runqueue* rq);
    
    // Events
    void (*task_tick)(struct runqueue* rq, process_t* p);
    void (*task_fork)(process_t* p);
};
```

### Priority Matrix
| Class ID | Name | Priority | Algorithm | Implementation |
| :--- | :--- | :--- | :--- | :--- |
| 0 | `SCHED_DEADLINE` | Top | EDF | Red-Black Tree by Deadline |
| 1 | `SCHED_RT` | High | FIFO/RR | Array of Linked Lists (O(1)) |
| 2 | `SCHED_CFS` | Normal | CFS | Red-Black Tree by vruntime |
| 3 | `SCHED_IDLE` | Low | Round-Robin | Single Linked List |

## 4. Core Scheduler Implementation

### The Runqueue (`runqueue_t`)
Each CPU has its own runqueue to minimize lock contention.

```c
typedef struct runqueue {
    spinlock_t lock;
    uint32_t nr_running;
    process_t* curr;
    
    // Class-specific sub-queues
    struct rb_root cfs_root;        // CFS Red-Black Tree
    struct list_head rt_queues[100];// Real-Time Priority Arrays
    struct list_head dl_root;       // Deadline Tree
    
    uint64_t clock;                 // Monotonic clock
} runqueue_t;
```

### The `schedule()` Function
This is the main entry point, called by interrupt handlers or voluntary yield.

```c
void schedule(void) {
    int cpu = get_cpu_id();
    runqueue_t* rq = &runqueues[cpu];
    
    spin_lock(&rq->lock);
    
    process_t* prev = rq->curr;
    
    // 1. Check for stack overflow (Debug)
    check_stack_canary(prev);
    
    // 2. Put previous task back to queue (if still runnable)
    if (prev->state == TASK_RUNNING) {
        prev->class->enqueue_task(rq, prev);
    }
    
    // 3. Pick next task
    process_t* next = pick_next_task(rq);
    
    // 4. Context Switch
    if (prev != next) {
        rq->curr = next;
        context_switch(prev, next);
    }
    
    spin_unlock(&rq->lock);
}
```

## 5. Context Switch Mechanism
The low-level context switch is architecture-specific (x86 example). It must save callee-saved registers and switch the stack pointer.

### Assembly Implementation (`switch.S`)
```nasm
; void switch_to(process_t* prev, process_t* next);
global switch_to
switch_to:
    ; [ESP+4] = prev
    ; [ESP+8] = next
    
    mov eax, [esp+4]    ; EAX = prev
    mov edx, [esp+8]    ; EDX = next
    
    ; 1. Save Previous Context
    push ebp
    push ebx
    push esi
    push edi
    
    ; Save ESP to prev->thread.esp
    mov [eax + OFFSET_ESP], esp
    
    ; 2. Restore Next Context
    ; Load ESP from next->thread.esp
    mov esp, [edx + OFFSET_ESP]
    
    ; 3. Load Page Directory (if different)
    mov ecx, [edx + OFFSET_CR3]
    mov ebx, cr3
    cmp ecx, ebx
    je .same_pd
    mov cr3, ecx
.same_pd:
    
    ; 4. Restore Registers
    pop edi
    pop esi
    pop ebx
    pop ebp
    
    ret
```

## 6. Tick Handler Integration
The timer interrupt calls `scheduler_tick()` to update runtime statistics and trigger preemption.

```c
void scheduler_tick(void) {
    int cpu = get_cpu_id();
    runqueue_t* rq = &runqueues[cpu];
    process_t* curr = rq->curr;
    
    spin_lock(&rq->lock);
    
    // Update generic stats
    rq->clock++;
    
    // Class-specific tick handling
    // e.g., CFS updates vruntime, RT decrements time_slice
    curr->class->task_tick(rq, curr);
    
    spin_unlock(&rq->lock);
}
```

## 7. SMP Load Balancing
The scheduler periodically balances load across CPUs using work stealing.

### Migration Logic
1.  **Trigger**: Timer interrupt (every 200ms) or when a CPU goes idle.
2.  **Find Busiest**: Scan other CPUs' runqueues to find the one with the highest load.
3.  **Steal**: Move tasks from the busiest runqueue to the current one.
4.  **Affinity Check**: Ensure the task is allowed to run on the current CPU (`p->cpu_mask`).

## 8. Configuration (`sched_config.h`)
Tunable parameters for system performance.

```c
#ifndef SCHED_CONFIG_H
#define SCHED_CONFIG_H

// Time Slices
#define BASE_TIME_SLICE_MS    100
#define MIN_TIME_SLICE_MS     10

// Priorities
#define MAX_PRIO              140
#define MAX_RT_PRIO           100

// CFS Parameters
#define CFS_LATENCY_TARGET_NS 20000000ULL // 20ms
#define CFS_MIN_GRANULARITY   4000000ULL  // 4ms

// Load Balancing
#define BALANCE_INTERVAL_MS   200
#define MIGRATION_COST_NS     500000ULL

#endif
```
