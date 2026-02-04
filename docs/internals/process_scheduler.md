# Process & Scheduler Internals

## 1. Overview
The Kernel employs a preemptive, multi-class scheduler supporting SMP (Symmetric Multiprocessing). It handles task switching, CPU load balancing, and real-time constraints.

## 2. Process Model (`process_t`)
The `process_t` structure is the core entity, containing the execution context, memory map, and resource limits.

### Key Fields
- **Context**: `esp`, `eip`, `eflags`, general registers.
- **Memory**: `page_directory` (CR3) and `vm_region_t` array for VMM.
- **Scheduling**: `priority`, `time_slice`, `vruntime` (CFS), `rt_budget` (Real-Time).
- **Identity**: `pid`, `uid`, `gid`, `caps` (Capabilities).
- **Resources**: File descriptors (`fds`), Signals (`pending_signals`).

### Lifecycle Diagram
```ascii
+---------+     fork()     +-------+     exec()     +---------+
| Parent  | -------------> | Child | -------------> | Running |
+---------+                +-------+                +---------+
                              |                          |
                              | wait()                   | exit()
                              v                          v
                         +---------+               +---------+
                         | Running | <------------ | Zombie  |
                         +---------+               +---------+
```

### Lifecycle Steps
1.  **Creation**: `process_create` allocates the structure and a new Page Directory.
2.  **Fork**: `process_fork` performs a copy-on-write clone of the parent.
3.  **Exec**: `process_exec` replaces the memory image with an ELF binary.
4.  **Exit**: `process_exit` releases resources but keeps the structure as a zombie until `process_wait`.

## 3. Scheduling Classes
The scheduler supports multiple classes, prioritized in order:

### Priority Matrix
| Class ID | Name | Priority Range | Algorithm | Use Case |
| :--- | :--- | :--- | :--- | :--- |
| 0 | `SCHED_CLASS_DEADLINE` | N/A (Earliest Deadline) | EDF | Hard Real-Time (Audio, Control) |
| 1 | `SCHED_CLASS_RT` | 0-99 | FIFO/RR | Soft Real-Time (UI, Input) |
| 2 | `SCHED_CLASS_CFS` | 100-139 | Red-Black Tree | Normal User Tasks |
| 3 | `SCHED_CLASS_IDLE` | 140 | Round-Robin | System Idle Loop |

### A. Deadline (`SCHED_CLASS_DEADLINE`)
- **Algorithm**: Earliest Deadline First (EDF).
- **Parameters**: `runtime`, `period`, `deadline`.

### B. Real-Time (`SCHED_CLASS_RT`)
- **Algorithm**: Fixed Priority Preemptive.
- **Logic**: Higher priority tasks always preempt lower ones. Round-robin for equal priority.

### C. CFS (Completely Fair Scheduler - `SCHED_CLASS_CFS`)
- **Algorithm**: Red-Black Tree (or sorted list) based on `vruntime`.
- **Logic**: Tasks with the lowest virtual runtime are picked next. `vruntime` increases as the task runs, weighted by priority.

## 4. SMP (Symmetric Multiprocessing)
The `smp_rally` subsystem handles multicore initialization and management.

### Architecture
- **BSP (Bootstrap Processor)**: Boots the system and wakes up APs (Application Processors).
- **AP Initialization**:
    1.  BSP sends INIT IPI.
    2.  BSP sends SIPI (Start-up IPI) with a trampoline vector.
    3.  AP jumps to protected mode and enables paging.
    4.  AP calls `scheduler_init` for its local queue.

### Load Balancing
- **Affinity**: `cpu_mask` restricts which CPUs a process can run on.
- **Migration**: `scheduler_balance_load` moves tasks from busy queues to idle queues.

## 5. Context Switching
The low-level mechanism (`switch_task`, `context_switch`) saves and restores state.

### Flow
1.  **Interrupt**: Timer or System Call triggers the scheduler.
2.  **Save**: Current registers are pushed to the kernel stack.
3.  **Pick**: The scheduler selects the next process.
4.  **Switch**:
    - `CR3` is updated (TLB flush).
    - Kernel stack pointer (`ESP0` in TSS) is updated.
    - Registers are popped from the new task's stack.
5.  **Return**: `IRET` jumps to the new task's `EIP`.

## 6. Configuration & Tuning
System parameters can be adjusted in `sched_config.h`:

```c
#define SCHED_TICK_HZ 1000       // Timer interrupt frequency
#define MIN_GRANULARITY_NS 100000 // Min slice for CFS
#define LOAD_BALANCE_INTERVAL 50 // Balance every 50 ticks
```

## API Reference
- `scheduler_yield()`: Voluntarily give up CPU.
- `scheduler_set_affinity(pid, mask)`: Bind process to specific cores.
- `scheduler_set_deadline(...)`: Promote a task to Deadline class.
- `process_fork(parent)`: Create a child process.
