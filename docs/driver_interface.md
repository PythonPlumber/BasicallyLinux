# Driver Interface & Grid

## Overview
The OS employs a unified driver model called **Driver Grid**. This abstraction simplifies device discovery, registration, and lifecycle management.

## The Driver Grid (`driver_grid`)
The Driver Grid acts as a registry for all available hardware drivers.

- **File**: `src/drivers/driver_grid.c`
- **Header**: `include/driver_grid.h`

### Driver Node Structure
Each driver is represented by a `driver_node_t`:

```c
typedef struct {
    const char* name;    // Human-readable name (e.g., "RTL8139 Network")
    int (*probe)(void);  // Function to check hardware presence
} driver_node_t;
```

## Lifecycle

### 1. Registration
Drivers register themselves during kernel initialization (typically in `kernel_main` or subsystem inits).

```c
// Example Registration
driver_node_t my_driver = { .name = "My Device", .probe = my_probe };
driver_grid_register(&my_driver);
```

### 2. Probing
The kernel invokes `driver_grid_probe_all()` to iterate through all registered drivers.
- The `probe()` function returns `1` if the hardware is found and initialized.
- Returns `0` if the hardware is missing or initialization failed.

## Device Categories

### Character Devices (`devnodes`)
- **Usage**: TTYs, Serial Ports.
- **Interface**: `read`, `write`, `ioctl`.
- **Path**: `/dev/tty0`, `/dev/serial`.

### Block Devices (`block`)
- **Usage**: Disks, Ramdisks.
- **Interface**: `read_sector`, `write_sector`.
- **Buffer Cache**: Integrated with the Page Cache.

### Network Devices (`net_link`)
- **Usage**: Ethernet cards.
- **Interface**: `tx_packet`, `rx_callback`.
- **Stack Integration**: Drivers push packets to `net_link` layer.
