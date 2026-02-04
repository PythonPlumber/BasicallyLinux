# Security Subsystem Internals

## 1. Overview
The Security Subsystem provides a multi-layered defense mechanism, integrating Capability-Based Security, Mandatory Access Control (MAC), and Kernel Hardening.

## 2. Decision Logic
The kernel applies security checks in a strict order. All checks must pass for an action to proceed.

### Enforcement Flow
```ascii
[ Action Request ]
      |
      v
[ 1. Capability Check ] --(Fail)--> [ DENIED ]
      | Pass
      v
[ 2. DAC Check (User/Group) ] --(Fail)--> [ DENIED ]
      | Pass
      v
[ 3. MAC Policy Check ] --(Fail)--> [ DENIED ]
      | Pass
      v
[ ALLOWED ] -> [ Audit Log ]
```

## 3. Capability Model (`secure_caps`)
Capabilities break down root privileges into fine-grained rights.

### Capability Matrix (Partial)
| Cap ID | Name | Description |
| :--- | :--- | :--- |
| 0 | `CAP_CHOWN` | Change file ownership. |
| 1 | `CAP_DAC_OVERRIDE` | Bypass file permission checks. |
| 12 | `CAP_NET_ADMIN` | Configure network interfaces/routing. |
| 21 | `CAP_SYS_ADMIN` | Catch-all for administration (Mount, Swap). |

## 4. Mandatory Access Control (`secure_policy`)
The Policy Engine enforces system-wide rules using a Subject-Object-Action model.

### Rule Structure
```c
typedef struct {
    uint32_t subject_id; // Process Label
    uint32_t object_id;  // Resource Label
    uint32_t action;     // READ, WRITE, EXEC
    uint32_t allow;      // 1=Allow, 0=Deny
} secure_policy_rule_t;
```

## 5. Kernel Hardening (`secure_hard`)
- **KASLR**: Randomizes kernel base address at boot.
- **Stack Canaries**: Places guard values on the stack to detect overflows.
- **NX**: Marks data pages as non-executable.

### Configuration (`secure_config.h`)
```c
#define SECURE_AUDIT_RING_SIZE 4096
#define SECURE_DEFAULT_POLICY  POLICY_DENY
#define SECURE_STRICT_MODE     1
```
