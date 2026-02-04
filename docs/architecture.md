# Kernel Architecture Master Index

## 1. Introduction
This OS represents a next-generation, professional-grade freestanding x86 kernel. It is engineered for high-assurance computing, featuring a hybrid architecture that combines the raw performance of a monolithic design with the modularity and security of a microkernel.

## 2. Documentation Hierarchy
The documentation is structured into three tiers: **Architecture (Tier 1)**, **Internals (Tier 2)**, and **Developer Guides (Tier 3)**.

### Tier 1: Core Architecture & Concepts
High-level overviews of system design, philosophy, and feature sets.
- **[Memory Management Architecture](memory_management.md)**: Physical/Virtual memory philosophy, strategies.
- **[Process & Scheduling Architecture](process_management.md)**: The task model, scheduling theory.
- **[Device Subsystem Architecture](device_subsystems.md)**: Hardware abstraction layers and bus topologies.
- **[Network Stack Architecture](network_stack.md)**: Protocol stack design and data flow.
- **[Security Architecture](security.md)**: The capability model and threat model.
- **[AI Subsystem Architecture](ai_architecture.md)**: Kernel-level inference engine design.

### Tier 2: Implementation Internals (Deep Dive)
Exhaustive technical details, data structures, algorithms, and API references.
- **[Memory Manager Internals](internals/memory_manager.md)**: PMM, VMM, Slab Allocator, and Kswapd implementation.
- **[Process Scheduler Internals](internals/process_scheduler.md)**: Context switching, SMP load balancing, and runqueues.
- **[Network Stack Internals](internals/network_stack.md)**: Socket buffers, protocol state machines, and driver interfaces.
- **[Hardware Abstraction Internals](internals/hardware_abstraction.md)**: Interrupt routing (Apex), Driver Grid, and MSGI.
- **[Storage & VFS Internals](internals/storage_vfs.md)**: Filesystem nodes, inode caching, journaling, and mounting.
- **[System Call Interface](internals/syscall_interface.md)**: ABI conventions, interrupt vectors, and handler tables.
- **[Security Subsystem Internals](internals/security_subsystem.md)**: Capability sets, policy engine, and audit logs.
- **[Userland Loader & Init](internals/userland_init.md)**: ELF parsing, dynamic linking, and process bootstrapping.

### Tier 3: Developer & Operational Guides
Practical manuals for building, debugging, and extending the kernel.
- **[Kernel Developer Guide](kernel_dev_guide.md)**: Setup, build system, and contribution workflow.
- **[Driver Interface Guide](driver_interface.md)**: Step-by-step driver creation tutorial.
- **[Debugging & Diagnostics](debugging.md)**: Using Trace Forge, Perf Meter, and crash analysis.
- **[Userland ABI Reference](userland_abi.md)**: System call reference for application developers.

## 3. Core Design Principles
1.  **Strict Isolation**: Hardware-enforced separation of kernel/user space with per-process page tables and guard pages.
2.  **Deterministic Execution**: O(1) scheduling and deadline-aware processing for real-time workloads.
3.  **Defense-in-Depth**: A security-first approach using capabilities, mandatory access control, and kernel self-protection.
4.  **Modular Extensibility**: Subsystems are decoupled via standardized interfaces (Driver Grid, VFS Ops), allowing hot-pluggable expansion.

## 4. Boot Sequence Overview
1.  **MBR/GRUB**: Loads kernel binary into memory.
2.  **`kernel_main`**:
    - **CPU**: GDT/IDT setup, ISR registration.
    - **Memory**: PMM initialization -> Paging enable -> Heap ready.
    - **Core**: Scheduler init, Timer calibration.
    - **Subsystems**: Apex INTC, PCI Enumeration, USB Nexus, Network Stack.
    - **Security**: Policy load, Capability system init.
3.  **Userland Handoff**: `init_orch` loads `/bin/init` and drops to Ring 3.

## 5. Build & Deploy
- **Build System**: Cross-platform (PowerShell/Make) build automation.
- **Target**: i686-elf (32-bit Protected Mode).
- **Commands**:
    - `make`: Compiles the kernel.
    - `make iso`: Generates a bootable CD image.
    - `make qemu`: Runs the OS in emulation.
