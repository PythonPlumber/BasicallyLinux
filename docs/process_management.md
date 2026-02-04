# Process Management & Scheduling

## Overview
The OS implements a sophisticated process management subsystem designed for low latency, high throughput, and strict isolation. It supports a multi-class scheduling architecture including Real-Time (RT) and Deadline scheduling, alongside a Completely Fair Scheduler (CFS) for general-purpose tasks.

## Process Model
The core unit of execution is the `process_t` structure (defined in `include/process.h`).

### Key Structures
- **Process Control Block (`process_t`):**
    - **Identification:** `pid`, `parent_pid`, `job_id`, `session_id`.
    - **State:** `PROCESS_READY`, `PROCESS_RUNNING`, `PROCESS_BLOCKED`, `PROCESS_SLEEPING`.
    - **Context:** Full CPU register state (`eax`, `eip`, `eflags`, etc.) for context switching.
    - **Resources:** Per-process page directory (`page_directory`), file descriptors (`fds`), and VM regions.
    - **Security:** Capabilities (`caps`), security ID, and namespace IDs (`ns_pid`, `ns_net`, etc.).

- **Thread/Task (`task_t` in `src/task/task.c`):**
    - Represents the low-level execution context (kernel threads).
    - Lightweight context switching via `switch_to` assembly routine.

## Scheduling Architecture
The scheduler (`src/kernel/sched.c`) is a priority-based, preemptive scheduler supporting multiple scheduling classes.

### Scheduling Classes
1.  **SCHED_CLASS_RT (Real-Time):**
    - Fixed priority tasks that preempt CFS tasks.
    - Uses `scheduler_set_rt()` to assign priority and budget.
    
2.  **SCHED_CLASS_DEADLINE:**
    - Deadline-driven scheduling (EDF - Earliest Deadline First logic).
    - **Parameters:** `rt_budget`, `rt_period`, `rt_deadline`.
    - Guarantees execution time within a period.

3.  **SCHED_CLASS_CFS (Completely Fair Scheduler):**
    - Default for user processes.
    - Uses `vruntime` (virtual runtime) to ensure fair CPU distribution.

### Context Switching
Context switching is handled by the `switch_to` routine in `src/task/switch.s`.
- **Mechanism:**
    1.  Saves current CPU state (`pushad`, `pushfd`) to the current task's stack/structure.
    2.  Updates `current_task_ptr`.
    3.  Restores the next task's state.
    4.  Jumps to the restored `EIP`.

## Process Lifecycle
1.  **Creation (`process_create`):**
    - Allocates a new PID and `process_t`.
    - Creates a new address space (Page Directory).
    - Sets up the kernel stack.
    
2.  **Execution:**
    - Scheduled by `scheduler()` during timer interrupts or blocking calls.
    
3.  **Termination (`scheduler_kill`):**
    - Transitions state to `PROCESS_BLOCKED` (or zombie).
    - Closes file descriptors and releases resources.
    - Parent process notified via wait queues.

## Synchronization
- **Wait Queues:** `wait_queue_t` allows processes to sleep until an event occurs.
- **Futex:** Fast Userspace Mutexes (supported via syscalls) for efficient user-level locking.

## Advanced Features
- **CPU Affinity:** `cpu_mask` field allows pinning processes to specific cores.
- **Control Groups (cgroups):** `cgroup_id` and `cgroup_share` support resource partitioning.
- **Namespaces:** Full namespace support for PID, Network, Mount, and User isolation.
