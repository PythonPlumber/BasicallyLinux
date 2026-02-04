# Memory Management Subsystem

## Overview
The OS implements a professional-grade memory management subsystem designed for reliability, performance, and security. It features a hybrid physical/virtual memory model with per-process address space isolation, a custom slab allocator for efficient kernel object management, and an active page reclamation daemon (`kswapd`) to handle memory pressure.

## Physical Memory Management (PMM)
The Physical Memory Manager (PMM) manages the allocation and deallocation of physical page frames (4KB). It uses a bitmap/stack-based approach to track free pages.

- **Page Size:** 4096 bytes (4KB)
- **Alignment:** 4KB aligned allocations
- **Functions:** `pmm_alloc_page()`, `pmm_free_page()` (see `include/pmm.h`)

## Virtual Memory Manager (VMM)
The Virtual Memory Manager (VMM) handles the mapping between virtual addresses and physical addresses using paging.

### Paging Architecture
- **Architecture:** x86 32-bit Paging (2-level: Page Directory -> Page Table)
- **Recursive Mapping:** Utilizes recursive page directory mapping for efficient modification of page tables.
- **Hardware Enforced:** Enabled via `cr0` (PG bit) and `cr3` (Page Directory Base Register).

**Key Functions (`src/mem/paging.c`):**
- `paging_enable()`: Activates paging by setting the PG bit in CR0.
- `map_page_dir(dir, virt, phys, flags)`: Maps a virtual page to a physical frame in a specific page directory.
- `load_cr3(dir)`: Context switches the address space.

### Per-Process Isolation
Each process maintains its own independent virtual address space, ensuring complete isolation.
- **Structure:** `process_t` (in `include/process.h`) contains:
    - `uint32_t* page_directory`: Physical address of the process's page directory.
    - `vm_region_t regions[16]`: Tracks high-level virtual memory regions (e.g., code, stack, heap).

**VM Region (`vm_region_t`):**
```c
typedef struct {
    uint32_t start;      // Virtual start address
    uint32_t end;        // Virtual end address
    uint32_t flags;      // Permissions (Read/Write, User, COW)
    uint32_t shared_id;  // ID for shared memory segments
} vm_region_t;
```

## Slab Allocator
To reduce fragmentation and improve performance for frequent kernel object allocations (like `process_t`, `fs_node_t`), the OS implements a custom Slab Allocator.

**Design (`src/mem/slab.c`):**
- **Caches:** `kmem_cache_t` structures track specific object types.
- **Slabs:** `slab_t` structures manage blocks of pages divided into fixed-size objects.
- **Free List:** Embedded free list within unused objects to save memory.
- **Reclamation:** Unused slabs are returned to the PMM when memory pressure rises.

**API:**
- `kmem_cache_create()`: Create a new object cache.
- `slab_alloc()`: Allocate an object from a cache.
- `slab_free()`: Return an object to its cache.

## Page Reclamation (kswapd)
The kernel includes a background daemon (`kswapd`) that actively monitors memory usage and reclaims pages when free memory falls below thresholds.

**Watermarks (`src/mem/kswapd.c`):**
- **High Watermark:** 80% usage (starts background reclaim).
- **Low Watermark:** 60% usage (stops reclaim).

**Mechanism:**
1. **Registration:** Subsystems (Slab, Page Cache) register reclaimer functions via `kswapd_register_reclaimer()`.
2. **Monitoring:** `kswapd_tick()` checks `mem_usage_pct()`.
3. **Reclaim:** If usage > High Watermark, it calls registered reclaimers to free pages until usage drops.

**Registered Reclaimers:**
- `slab_reclaim()`: Frees empty slabs.
- `page_cache_reclaim()`: Evicts clean pages from the file system cache.

## NUMA Awareness (Planned)
Future extensions include NUMA (Non-Uniform Memory Access) awareness to optimize memory allocation on multi-socket systems, ensuring pages are allocated from local nodes.
