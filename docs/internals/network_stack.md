# Network Stack Internals

## 1. Overview
The Kernel implements a modular, layered network stack following the TCP/IP model. It is designed for zero-copy packet processing and high concurrency.

## 2. Packet Flow Architecture

### Ingress Flow (Receive)
```ascii
[ NIC Hardware ]
      | DMA
      v
[ Ring Buffer ]
      | ISR Trigger
      v
[ Link Layer (Eth) ] -> Strip Header, Check MAC
      | EtherType (0x0800)
      v
[ Network Layer (IP) ] -> Check Checksum, Route
      | Protocol (TCP/UDP)
      v
[ Transport Layer ] -> Demux by Port
      | Socket Buffer
      v
[ User Socket ] -> recv() returns data
```

## 3. Link Layer (`net_link`)
The bottom layer handles interaction with network interface cards (NICs).

### Abstraction
- **Driver Interface**: Drivers register via `driver_grid` and provide callback functions for packet transmission.
- **DMA**: Drivers are expected to use DMA rings to write packets directly to memory, minimizing CPU overhead.

### API
- `net_link_init()`: Probes for supported NICs (e.g., RTL8139, E1000).
- `net_link_primary()`: Returns the default interface for routing.

## 4. Network Layer (`net_ip`)
Handles addressing and routing (IPv4/IPv6).

### IPv4 Support
- **Packet Structure**: Version, IHL, TOS, Length, ID, Flags, TTL, Protocol, Checksum, Source, Dest.
- **ICMP**: Handles Ping (Echo Request/Reply) and error reporting.

### Routing Table Example
| Destination | Netmask | Gateway | Interface | Metric |
| :--- | :--- | :--- | :--- | :--- |
| 0.0.0.0 | 0.0.0.0 | 192.168.1.1 | eth0 | 10 |
| 192.168.1.0 | 255.255.255.0 | *On-Link* | eth0 | 1 |
| 10.0.0.0 | 255.0.0.0 | 10.0.0.1 | tun0 | 5 |

## 5. Transport Layer (`net_transport`)
Manages connections and data streams.

### TCP State Machine
- **LISTEN**: Waiting for connection.
- **SYN_SENT**: Connection started, waiting for ACK.
- **ESTABLISHED**: Data transfer active.
- **FIN_WAIT**: Closing connection.

### Socket API
- `net_transport_open(...)`: Creates a new connection entry.
- `net_transport_close(...)`: Sends FIN/RST and cleans up resources.

## 6. Socket Interface
Userland applications interact with the stack via file descriptors.
- **Syscalls**: `socket`, `bind`, `connect`, `listen`, `accept`, `send`, `recv`.

## 7. Configuration Template (`net_config.h`)
```c
// Network Buffer Configuration
#define NET_RX_RING_SIZE    256   // Descriptors per NIC
#define NET_TX_RING_SIZE    128
#define NET_PACKET_MTU      1500  // Ethernet Standard

// TCP Tuning
#define TCP_WINDOW_SIZE     65535
#define TCP_MAX_RETRIES     5
#define TCP_KEEPALIVE_MS    7200000
```

## 8. Security & Filtering (`net_guard`)
- **Firewall**: A packet filtering hook allowing rules based on IP/Port/Protocol.
- **NAT**: (Planned) Network Address Translation for sharing connections.
