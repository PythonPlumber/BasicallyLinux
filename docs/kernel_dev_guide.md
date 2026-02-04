# Kernel Developer Guide

## Introduction
This guide is intended for kernel developers contributing to the OS. It covers the build system, coding standards, and common development workflows.

## Build System

The build system is designed to be cross-platform, supporting both Linux (Makefile) and Windows (PowerShell).

### Prerequisites
- **Compiler**: `i686-elf-gcc` or `gcc` with `-m32` support.
- **Linker**: `i686-elf-ld`.
- **Assembler**: `nasm`.
- **Emulator**: `qemu-system-i386` (for testing).

### Commands
- **Build**: `pwsh ./build.ps1` (Windows) or `make` (Linux).
- **Clean**: `make clean` or manual deletion of `*.o` files.
- **Run**: `qemu-system-i386 -kernel kernel.bin`.

## Coding Standards

### Style
- **Indentation**: 4 spaces (no tabs).
- **Braces**: K&R style (opening brace on the same line).
- **Naming**:
    - Functions: `module_action_subject` (e.g., `paging_map_page`).
    - Types: `snake_case_t` (e.g., `process_t`, `vm_region_t`).
    - Macros: `UPPER_CASE` (e.g., `PAGE_SIZE`).

### Types
Always use fixed-width integer types from `types.h`:
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- `int32_t`, etc.
- Avoid raw `int`, `short`, `long` unless interacting with legacy headers.

## Workflow: Adding a System Call

To add a new system call (e.g., `SYS_CUSTOM`):

1.  **Update Kernel Header**:
    - Edit `include/syscall.h`.
    - Add `SYS_CUSTOM = N` to the enum (increment `SYS_MAX`).

2.  **Implement Handler**:
    - Edit `src/cpu/syscall.c`.
    - Add a case in `syscall_handler`:
      ```c
      case SYS_CUSTOM:
          state->eax = sys_custom_handler(state->ebx);
          break;
      ```

3.  **Implement Logic**:
    - Create the implementation function (e.g., `sys_custom_handler`) in the appropriate subsystem.

4.  **Update Userland Library**:
    - Add a wrapper function in `src/usermode.c` that invokes `int 0x80` with the new ID.

## Workflow: Adding a Driver

1.  **Define Driver Structure**:
    - Create `src/drivers/my_driver.c`.
    - Define a `driver_node_t` with a name and probe function.

2.  **Register Driver**:
    - In `kernel.c` (or a subsystem init), call `driver_grid_register(&my_driver_node)`.

3.  **Implement Probe**:
    - The probe function should check for hardware presence (e.g., PCI ID or I/O port response) and return 1 on success.
