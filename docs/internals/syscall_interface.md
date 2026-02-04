# System Call Interface

## 1. Overview
The System Call Interface serves as the boundary between user mode applications and the kernel. It allows applications to request services such as process management, I/O, and networking.

## 2. Transition Mechanism

### Execution Flow
```ascii
[ User Mode ]
    | Call libc wrapper (e.g., write())
    v
[ Libc Stub ]
    | Set EAX = SYS_WRITE
    | Set EBX = fd, ECX = buf, EDX = len
    | INT 0x80
    v
----------------( Ring 3 -> Ring 0 )----------------
    v
[ IDT Handler (isr_80) ]
    | Push Registers
    v
[ syscall_handler() ]
    | Verify EAX range
    | Call syscall_table[EAX]
    v
[ Implementation (sys_write) ]
    | Validate pointers
    | Perform I/O
    | Return result
    v
[ IRET ]
----------------( Ring 0 -> Ring 3 )----------------
    v
[ User Mode ] -> Resume execution
```

## 3. ABI Specification
- **Interrupt Vector**: `0x80`
- **Register Usage**:

| Register | Purpose |
| :--- | :--- |
| `EAX` | Syscall Number (Input) / Return Value (Output) |
| `EBX` | Argument 1 |
| `ECX` | Argument 2 |
| `EDX` | Argument 3 |
| `ESI` | Argument 4 |
| `EDI` | Argument 5 |

## 4. Core System Calls

### Process Management
| ID | Name | Description |
| :--- | :--- | :--- |
| 13 | `SYS_FORK` | Clones the current process (COW). |
| 14 | `SYS_EXEC` | Replaces current process image. |
| 15 | `SYS_EXIT` | Terminates the current process. |
| 16 | `SYS_WAIT` | Waits for a child process to exit. |

### File I/O
| ID | Name | Description |
| :--- | :--- | :--- |
| 9 | `SYS_OPEN` | Opens a file or device. |
| 10 | `SYS_CLOSE` | Closes an FD. |
| 11 | `SYS_READ` | Reads data from an FD. |
| 12 | `SYS_FDWRITE` | Writes data to an FD. |

### Security & Identity
| ID | Name | Description |
| :--- | :--- | :--- |
| 22 | `SYS_GETUID` | Get User ID. |
| 27 | `SYS_SETCAPS` | Drop or acquire capabilities. |
| 32 | `SYS_POLICY_ADD`| Load a new security policy rule. |

## 5. Adding a System Call
1.  Define a unique ID in `syscall.h`.
2.  Implement the function `sys_mycall` in `syscall.c`.
3.  Add the function pointer to `syscall_table` in `syscall.c`.
