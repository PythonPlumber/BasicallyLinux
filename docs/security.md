# Security Architecture

## Overview
The OS implements a defense-in-depth security model, integrating Mandatory Access Control (MAC), Capability-based privileges, and kernel hardening features. This architecture is designed to minimize the attack surface and enforce strict isolation.

## Components

### 1. Secure Policy (MAC Engine)
The core access control engine that enforces rules regardless of user discretion.
- **File:** `src/security/secure_policy.c`
- **Model:** Subject-Object-Action
    - **Subject:** Process ID or Security ID.
    - **Object:** Resource ID (File, Port, Memory Region).
    - **Action:** Read, Write, Execute, Connect, etc.
- **Mechanism:**
    - `secure_policy_check(subject, object, action)` is called by subsystems before granting access.
    - If `state.enforced` is true, access is denied unless an explicit rule allows it.

### 2. Secure Capabilities
A fine-grained privilege model replacing the all-or-nothing "root" user.
- **File:** `src/security/secure_caps.c`
- **Structure:** `secure_caps_t` (Bitmask).
- **Functionality:**
    - Capabilities are attached to `process_t`.
    - Checked via `secure_caps_has()`.
    - Examples: `CAP_NET_BIND` (bind low ports), `CAP_SYS_ADMIN` (admin tasks), `CAP_RAW_IO`.

### 3. Kernel Hardening (`secure_hard`)
Proactive measures to make exploitation difficult.
- **File:** `src/security/secure_hard.c`
- **Features:**
    - **KASLR (Kernel Address Space Layout Randomization):** Manages entropy seeds (`secure_hard_kaslr_seed`) to randomize memory layout at boot.
    - **Stack Canaries:** Provides random values (`secure_hard_canary`) placed on the stack to detect buffer overflows.
    - **Crypto Acceleration:** Manages hardware crypto support.

### 4. Audit Logging
A tamper-resistant logging system for security-critical events.
- **File:** `src/security/secure_audit.c`
- **Storage:** Fixed-size ring buffer (`entries[128]`) in kernel memory.
- **Metrics:** Tracks total events and dropped events (to detect DoS attempts on the logger).
- **API:** `secure_audit_log(code)` records the event code and timestamp.

## Security Flow
1.  **Privilege Check:** System call handler checks `secure_caps_has()`.
2.  **Policy Enforcement:** If privileged, `secure_policy_check()` verifies the specific action against the active MAC policy.
3.  **Auditing:** Result (Success/Denial) is logged via `secure_audit_log()`.

## Future Roadmap
- **Namespaces:** Complete integration of PID, Network, and User namespaces (scaffolded in `process_t`).
- **Verified Boot:** Chain of trust from bootloader to kernel.
