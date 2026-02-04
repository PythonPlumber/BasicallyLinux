# Debugging & Diagnostics

## Overview
The OS provides a suite of tools for debugging kernel issues, tracing performance, and verifying system integrity.

## Trace Forge
**Trace Forge** is a high-speed, low-overhead kernel tracing facility.

- **Purpose**: Record events (interrupts, context switches, syscalls) with minimal latency.
- **Mechanism**: In-memory ring buffer (256 events).
- **API**:
  ```c
  trace_forge_emit(TAG_SCHED_SWITCH, pid);
  ```
- **Analysis**: Use `trace_forge_get()` to retrieve events from userland or a debugger.

### Common Tags
- `0x01`: Context Switch
- `0x02`: Page Fault
- `0x03`: Syscall Entry
- `0x04`: Interrupt Entry

## Diagnostics (`diag`)
The `diag` subsystem provides structured logging.

- **Output**: Serial Port (COM1) by default.
- **Format**: `[SEVERITY] Subsystem: Message`
- **Usage**:
  ```c
  diag_log(DIAG_INFO, "INIT", "Kernel started successfully.");
  ```

## Perf Meter
**Perf Meter** measures execution time in CPU cycles (RDTSC).

- **Usage**: Profiling critical sections.
- **API**:
  ```c
  perf_meter_start(METER_ID_SCHED);
  // ... critical code ...
  perf_meter_stop(METER_ID_SCHED);
  ```

## Self-Tests
The kernel includes built-in self-tests for critical algorithms.

- **Command**: Run `selftest` in the shell.
- **Coverage**:
    - Fixed-point arithmetic
    - AI Model tensor operations
    - Memory allocation sanity checks

## Debugging Tips
1.  **QEMU Monitor**: Use `Ctrl+Alt+2` to access QEMU monitor.
    - `info registers`: Dump CPU state.
    - `xp /10i $eip`: Disassemble current instruction.
2.  **Serial Output**: Always redirect serial output to a file when running QEMU (`-serial file:log.txt`) to capture boot logs.
3.  **Panic**: If the kernel encounters an unrecoverable error, it will hang. Check the last serial log entry.
