# Userland Loader & Init Internals

## 1. Overview
The Userland Subsystem handles the transition from kernel mode to user mode, managing executable loading, dynamic linking, and process bootstrapping.

## 2. Address Space Layout
When an ELF binary is loaded, the virtual address space is organized as follows:

```ascii
0xFFFFFFFF +----------------------+
           | Kernel Space         |
0xC0000000 +----------------------+
           | Stack (Grows Down)   |
           |                      |
           v                      v
           |                      |
           | Heap (Grows Up)      |
           +----------------------+
           | .bss (Uninitialized) |
           +----------------------+
           | .data (Initialized)  |
           +----------------------+
           | .text (Code)         |
0x08048000 +----------------------+
           | Null Guard Page      |
0x00000000 +----------------------+
```

## 3. ELF Loader (`elf_loader`)
Parses ELF headers and maps segments to VMM regions.

### Loading Steps
1.  **Verify Header**: Check Magic `0x7F ELF`.
2.  **Scan Program Headers**: Look for `PT_LOAD`.
3.  **Map Segments**: call `vm_map_region` for each loadable segment.
4.  **Zero BSS**: Clear uninitialized data area.

## 4. Dynamic Linker (`dyn_loader`)
Resolves shared library dependencies at runtime.

### Data Structures
```c
struct dyn_module {
    char name[32];
    uint32_t base_addr;
    uint32_t entry_point;
    struct dyn_module* next;
};
```

## 5. Init Orchestration (`init_orch`)
The bootstrap process for PID 1.

### Init Sequence
1.  `kernel_main` completes.
2.  `init_orch_start` sets up the first task TSS.
3.  Cpu switches to Ring 3.
4.  `sys_exec("/bin/init")` is called.

## 6. Security Features
- **ASLR**: Randomizes the base of the Stack, Heap, and Libraries.
- **Relro**: Read-only relocation tables after binding.
