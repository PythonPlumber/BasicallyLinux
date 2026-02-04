# Kernel Internals

## Overview
This document details the internal algorithms and data structures used within the OS kernel. It is intended for core developers debugging deep system issues or optimizing performance.

## Data Structures

### 1. Circular Linked Lists
- **Usage**: Process lists (`process_t`), Slab allocator (`slab_t`).
- **Implementation**: Intrusive linked lists where the structure contains a `next` pointer to the same type.
- **Traversal**:
  ```c
  process_t* start = head;
  process_t* current = head;
  do {
      // process current
      current = current->next;
  } while (current != start);
  ```

### 2. Ring Buffers (Fixed-Size)
- **Usage**: `trace_forge` (kernel tracing), `secure_audit` (security logs).
- **Implementation**: Array with `head` index and `count`.
- **Properties**: Overwrites oldest entries when full (circular buffer). Lock-free for single-producer/single-consumer (SPSC) scenarios, but currently guarded by interrupt disabling for safety.

### 3. Bitmaps
- **Usage**: Physical Memory Manager (PMM).
- **Implementation**: Array of `uint32_t` where each bit represents a 4KB page.
- **Logic**:
    - `0`: Free
    - `1`: Used
    - **Allocation**: Scans for the first zero bit, sets it, and returns the physical address.

## Algorithms

### 1. Scheduling (Hybrid Class)
The scheduler uses a hierarchical selection strategy:
1.  **Check RT/Deadline**: Scans all processes for `SCHED_CLASS_RT` or `SCHED_CLASS_DEADLINE`.
    - **EDF (Earliest Deadline First)**: Picks the task with the closest `rt_deadline`.
2.  **Check CFS**: If no RT tasks are ready, picks `SCHED_CLASS_CFS` tasks.
    - **Round Robin**: Currently simplifies CFS to round-robin within priority levels.

### 2. Memory Allocation (Slab)
- **Strategy**: Segregated Fit.
- **Logic**:
    - Pre-allocates "Slabs" (chunks of pages) for specific object sizes.
    - Objects form a free-list *inside* the unallocated memory (embedded pointers).
    - **Alloc**: Popping from the free-list (O(1)).
    - **Free**: Pushing to the free-list (O(1)).

### 3. Page Replacement (Clock/LRU Approximation)
- **Component**: `kswapd`.
- **Logic**:
    - Monitors `page_cache`.
    - When memory > `high_watermark`, iterates through cache entries.
    - **Second Chance**: Checks `accessed` bit (if HW supports) or internal usage counters.
    - Frees clean pages; writes back dirty pages (future).

## Synchronization
- **Current Model**: Uniprocessor-safe via Interrupt Disabling (`cli`/`sti`).
- **Planned**: Spinlocks for SMP support.
    - `spinlock_acquire`: Atomic `lock xchg`.
    - `spinlock_release`: Atomic store `0`.
