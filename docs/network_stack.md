# Network Stack Architecture

## Overview
The OS features a modular, custom-built network stack designed for security, performance, and flexibility. It implements a layered architecture separating Data Link, Network, and Transport concerns, integrated with a built-in firewall (NetGuard).

## Architecture Layers

### 1. NetStack (Orchestrator)
The central hub that initializes and coordinates all network layers.
- **File:** `src/net/net_stack.c`
- **Responsibilities:**
    - Initialization sequence (`net_stack_init`).
    - High-level Packet I/O (`net_stack_send`, `net_stack_recv`).
    - Integration with Routing and Firewall.

### 2. NetLink (Data Link Layer - L2)
Handles physical addressing and interface management.
- **File:** `src/net/net_link.c`
- **Features:**
    - MAC Address management (`primary_link`).
    - Link status tracking (Up/Down).
    - Abstraction for Ethernet drivers.

### 3. NetIP (Network Layer - L3)
Manages logical addressing and routing state.
- **File:** `src/net/net_ip.c`
- **Features:**
    - Dual-stack support (IPv4 and IPv6 storage).
    - Routing integration via `NetRoute`.

### 4. NetTransport (Transport Layer - L4)
Manages connection-oriented (TCP) and connectionless (UDP) communications.
- **File:** `src/net/net_transport.c`
- **Features:**
    - **Connection Tracking:** Manages a table of `net_conn_t` structures.
    - **API:** `net_transport_open()` / `net_transport_close()`.
    - **State Machine:** Tracks connection states (Closed, Listen, Established).

### 5. NetGuard (Firewall & Security)
An integrated security layer that filters traffic before processing.
- **Integration:** Called within `net_stack_send()` via `net_guard_check()`.
- **Policy:** enforces access control lists (ACLs) based on Source, Destination, and Port.

## Data Flow
1.  **Transmission:**
    - Application calls `net_stack_send()`.
    - **NetGuard** verifies permission.
    - **NetRoute** determines the gateway.
    - Packet is constructed and passed to the driver.

2.  **Reception:**
    - Driver receives frame.
    - **NetLink** validates MAC.
    - **NetIP** validates IP checksum and destination.
    - **NetTransport** demultiplexes to the correct socket/connection.

## Future Extensions
- **Zero-Copy I/O:** Ring buffer DMA integration.
- **Hardware Offload:** Checksum and Segmentation offloading.
- **Congestion Control:** TCP Cubic/Reno implementation.
