# Memory Management Internals

## 1. Overview
The Kernel implements a hierarchical memory management system designed for scalability and safety. It features a Physical Memory Manager (PMM) based on bitmaps, a Virtual Memory Manager (VMM) with per-process page tables, a Slab Allocator for kernel objects, and a background reclamation daemon (kswapd).

## 2. Memory Architecture

### Layered Hierarchy Diagram
```ascii
[ Applications (User Space) ]
      | malloc() / free()
      v
[ Heap Manager (libc) ]
      | sbrk() / mmap()
      v
[ Virtual Memory Manager (VMM) ] -> Page Tables (CR3)
      | Page Faults
      v
[ Physical Memory Manager (PMM) ] -> Bitmap / Zones
      | Alloc Frame
      v
[ Hardware RAM ]
```

## 3. Physical Memory Manager (PMM)
The PMM is responsible for tracking physical RAM usage. It uses a bitmap allocator where each bit represents a 4KB page frame.

### Key Data Structures
- **Bitmap**: A global array where `0` indicates free and `1` indicates used.
- **Regions**: Memory is divided into reserved regions (kernel code, MMIO) and allocatable regions (RAM).

### API Reference
- `pmm_init(start, size)`: Initializes the bitmap based on BIOS/UEFI memory map.
- `pmm_alloc_block()`: Scans the bitmap for the first free bit (O(n) search, optimized with a roving pointer).
- `pmm_free_block(addr)`: Clears the corresponding bit.
- `pmm_reserve_region(addr, size)`: Marks a range as used (e.g., for DMA buffers).

## 4. Virtual Memory Manager (VMM)
The VMM manages address spaces using x86 paging (CR3, PDEs, PTEs).

### Memory Map Layout
| Virtual Address Range | Description | Privileges |
| :--- | :--- | :--- |
| `0xC0000000 - 0xFFFFFFFF` | Kernel Space (Higher Half) | Ring 0 RWX |
| `0xB0000000 - 0xBFFFFFFF` | Stack & MMIO | Ring 0/3 RW |
| `0x08048000 - 0xAFFFFFFF` | User Heap & Code | Ring 3 RWX |
| `0x00000000 - 0x00001000` | Null Guard Page | No Access |

### Per-Process Isolation
Each process (`process_t`) has its own Page Directory (`cr3`).
- **Context Switch**: Changing `cr3` flushes the TLB (except for global pages).
- **COW (Copy-On-Write)**: `paging_clone_cow_range` marks pages as read-only. A write fault triggers `vm_handle_page_fault`, which allocates a new physical page and copies content.

## 5. Slab Allocator (Kernel Objects)
To reduce fragmentation and overhead for small objects, a Slab Allocator is used.

### Design
- **Caches**: Each object type (e.g., `task_t`, `inode_t`) has a dedicated `kmem_cache_t`.
- **Slabs**: Large blocks (e.g., 4KB) divided into fixed-size slots.
- **Free List**: A linked list of free slots within a slab.

### API Reference
- `kmem_cache_create(size, align)`: Creates a new cache.
- `kmem_cache_alloc(cache)`: Returns a pointer to an object.
- `kmem_cache_free(cache, obj)`: Returns an object to the slab.

## 6. Kswapd (Memory Reclamation)
The `kswapd` subsystem is a background mechanism to prevent OOM (Out Of Memory) conditions.

### Mechanism
- **Watermarks**:
  - `High`: Target free pages.
  - `Low`: Trigger threshold for reclamation.
- **Reclaimers**: Subsystems (like Inode Cache, Page Cache) register callback functions (`reclaimer_fn`).
- **Cycle**: `kswapd_tick()` checks memory pressure. If free pages < `low_watermark`, it invokes registered reclaimers until `high_watermark` is reached.

### Tuning Guide
```c
// kswapd_config.h
#define KSWAPD_LOW_MARK   128  // Pages (512KB)
#define KSWAPD_HIGH_MARK  512  // Pages (2MB)
#define SLAB_RECLAIM_AGE  30   // Seconds before cache drop
```
