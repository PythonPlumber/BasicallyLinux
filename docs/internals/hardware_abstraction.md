# Hardware Abstraction & Driver Internals

## 1. Overview
The Kernel manages hardware diversity through a set of abstraction layers, ensuring that the core kernel remains hardware-agnostic while supporting specific devices via drivers.

## 2. Driver Grid (`driver_grid`)
The **Driver Grid** is the central registry for all device drivers. It uses a publish-subscribe model for hardware discovery.

### Registration Flow
```ascii
[ Kernel Boot ]
      |
      v
[ Driver Init Phase ] <--- Drivers call driver_grid_register()
      |
      v
[ Probe Phase ]
      | Iterate all registered drivers
      v
[ Hardware Match? ] --(Yes)--> [ Call driver->init() ]
      | No
      v
[ Next Driver ]
```

### Data Structures
```c
typedef struct {
    char name[32];
    uint32_t device_id;
    uint32_t vendor_id;
    int (*probe)(void);
    int (*init)(void);
} driver_node_t;
```

### API Reference
- `driver_grid_register(node)`: Adds a driver to the global list.
- `driver_grid_find(name)`: Locates a driver by name.

## 3. Interrupt Subsystem
The kernel uses a custom interrupt architecture designed for SMP and modern PCIe devices.

### APEX INTC (Advanced Programmable Interrupt Controller)
- **Role**: Replaces the legacy PIC. Handles external interrupts and routes them to specific CPUs.
- **Routing**: `apex_intc_route(irq, cpu)` directs hardware events to the appropriate core.

### MSGI (Message Signaled Interrupts)
- **Role**: Supports PCIe MSI/MSI-X.
- **Mechanism**: Devices write to a specific memory address to trigger interrupts.

### Interrupt Routing Diagram
```ascii
[ Device (e.g., NIC) ]
      | MSI Write
      v
[ PCIe Bus ]
      |
      v
[ APEX INTC ] --(Vector 40)--> [ CPU Core 0 ]
              --(Vector 41)--> [ CPU Core 1 ]
```

## 4. Device Mapper (`dm`)
The **Device Mapper** provides a logical volume management layer for storage devices.

### Functionality
- **Abstraction**: Wraps physical `block_device_t` drivers (NVMe, AHCI, IDE).
- **Partitioning**: Can map logical devices to specific LBA ranges on physical disks (`dm_create`).

### Configuration Template
```c
// device_config.h
#define MAX_DRIVERS 256
#define APEX_BASE_ADDR 0xFEE00000
#define MSGI_BASE_VECTOR 0x40
```

## 5. Bus Subsystems
- **PCIe Portshift**: Enumerates the PCI Express bus, configuring BARs.
- **USB Nexus**: Manages the USB tree, handling root hubs and device enumeration.

## 6. Input Stream
- **Event Flow**: Keyboard/Mouse drivers push raw data to `input_stream`.
- **Processing**: The stream aggregates events and dispatches them to the active TTY.
