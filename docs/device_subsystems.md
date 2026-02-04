# Device Subsystems & Hardware Abstraction

## Overview
The OS utilizes a modular, professional-grade hardware abstraction layer (HAL) with custom subsystems designed to replace legacy standards with modern, efficient interfaces. This architecture avoids direct Linux cloning by implementing unique management paradigms for interrupts, buses, and processors.

## Apex INTC (Interrupt Controller)
Apex INTC is the centralized interrupt management subsystem, abstracting Local APIC and IOAPIC complexities.

- **Purpose:** Manage IRQ routing, masking, and delivery to specific CPUs.
- **Implementation:** `src/cpu/apex_intc.c`
- **Key Features:**
    - **Dynamic Routing:** `apex_intc_route(irq, cpu)` allows real-time reassignment of IRQs for load balancing.
    - **Vector Management:** Manages IDT vectors 32-255.

## MSGI (Message Signaled Global Interrupts)
MSGI provides a unified interface for Message Signaled Interrupts (MSI/MSI-X) and Inter-Processor Interrupts (IPIs).

- **Purpose:** Bypass legacy interrupt lines for high-speed device signaling.
- **Implementation:** `src/cpu/msgi.c`
- **Key Features:**
    - **Direct Delivery:** `msgi_send(vector, cpu)` triggers interrupts directly on target cores without IOAPIC overhead.
    - **PCIe Integration:** Works in tandem with PCIe Portshift to configure device MSI capabilities.

## SMP Rally (Multiprocessor Management)
SMP Rally is the subsystem responsible for discovering, booting, and managing multiple CPU cores.

- **Purpose:** Symmetric Multiprocessing (SMP) bring-up and lifecycle management.
- **Implementation:** `src/cpu/smp_rally.c`
- **Key Features:**
    - **Core Discovery:** `smp_rally_add_cpu()` dynamically registers cores found via ACPI/MADT.
    - **Hotplug Support:** `smp_rally_boot()` and `smp_rally_remove_cpu()` support logical online/offline operations.
    - **Affinity Masks:** Maintains `online_mask` for scheduler reference.

## PCIe Portshift
PCIe Portshift is the PCI Express root complex and bridge manager, designed with native hotplug support.

- **Purpose:** Enumerate PCI/PCIe buses and handle hot-plug events.
- **Implementation:** `src/drivers/pcie_portshift.c`
- **Key Features:**
    - **Enumeration:** Scans configuration space (ECAM/Legacy) to build a device tree.
    - **Hotplug:** `pcie_portshift_hotplug(port_id)` handles device insertion/removal interrupts.
    - **Resource Allocation:** Assigns MMIO and IO BARs to devices.

## USB Nexus
USB Nexus acts as the central hub for the Universal Serial Bus stack.

- **Purpose:** Manage host controllers (UHCI/EHCI/XHCI) and enumerate connected devices.
- **Implementation:** `src/drivers/usb_nexus.c`
- **Key Features:**
    - **Port Management:** `usb_nexus_ports()` tracks available connection points.
    - **Driver Matching:** Matches device classes/IDs to loaded drivers (e.g., HID, Mass Storage).
