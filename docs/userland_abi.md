# Userland ABI & System Call Interface

## Overview
The Operating System provides a stable Application Binary Interface (ABI) for userspace programs, centered around a unified system call mechanism and the ELF binary format. This document details how user applications interact with the kernel.

## System Call Interface
System calls are the primary gateway for userspace programs to request kernel services.

- **Mechanism:** Software Interrupt (typically `int 0x80` or `sysenter`).
- **Register Usage:**
    - `EAX`: System Call Number
    - `EBX`, `ECX`, `EDX`, `ESI`, `EDI`: Arguments
    - `EAX`: Return Value

### System Call Table (`include/syscall.h`)

| ID | Name | Description |
|----|------|-------------|
| 0 | `SYS_WRITE` | Write string to console (legacy). |
| 1 | `SYS_PUTCHAR` | Write single char to console. |
| 2 | `SYS_TICKS` | Get system uptime. |
| 3 | `SYS_YIELD` | Voluntarily yield CPU time. |
| 4 | `SYS_GETPID` | Get current Process ID. |
| 9 | `SYS_OPEN` | Open a file. |
| 10 | `SYS_CLOSE` | Close a file descriptor. |
| 11 | `SYS_READ` | Read from file descriptor. |
| 12 | `SYS_FDWRITE` | Write to file descriptor. |
| 13 | `SYS_FORK` | Create a child process (COW). |
| 14 | `SYS_EXEC` | Replace process image with new binary. |
| 15 | `SYS_EXIT` | Terminate process. |
| 17 | `SYS_SETCLASS` | Change scheduling class (RT/CFS). |
| 20 | `SYS_SETRT` | Set Real-Time parameters. |
| 27 | `SYS_SETCAPS` | Modify security capabilities. |

*(See `include/syscall.h` for the full list of 33+ syscalls)*

## Binary Format (ELF)
The OS uses the Executable and Linkable Format (ELF) for binaries.

- **Loader:** `src/user/elf_loader.c`
- **Supported Features:**
    - **Architecture:** x86 (32-bit).
    - **Type:** Executable (`ET_EXEC`).
    - **Magic:** `0x7F 'E' 'L' 'F'`.
- **Loading Process:**
    1.  Kernel reads ELF header.
    2.  Verifies Magic and Architecture.
    3.  Maps segments (Text, Data, BSS) into process virtual memory.
    4.  Jumps to `e_entry` point.

## Init System (Init Orchestrator)
The Init Orchestrator (`src/user/init_orch.c`) is responsible for bootstrapping userspace.

- **Role:** The first process (PID 1).
- **Function:** `init_orch_start()`
- **Responsibilities:**
    - Mounting filesystems.
    - Starting system services.
    - Spawning the login shell.

## Dynamic Linking (Planned)
The Dynamic Loader (`src/user/dyn_loader.c`) is currently a stub for future shared library (`.so`) support, which will allow run-time symbol resolution and memory saving via shared code pages.
