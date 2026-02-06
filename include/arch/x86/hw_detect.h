#ifndef HW_DETECT_H
#define HW_DETECT_H

#include "types.h"

// Generic PCI device info
typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor;
    uint16_t device;
    uint8_t class_id;
    uint8_t subclass;
} hw_pci_device_t;

// PCI detection interface
void hw_pci_scan(void);
uint32_t hw_pci_get_device_count(void);
int hw_pci_get_device_info(uint32_t index, hw_pci_device_t* out_info);

// ACPI detection interface
void hw_acpi_init(void);
uintptr_t hw_acpi_find_table(const char* signature);
uintptr_t hw_acpi_get_rsdp_addr(void);
uintptr_t hw_acpi_get_rsdt_addr(void);
uint32_t hw_acpi_get_rsdt_entries(void);
uintptr_t hw_acpi_get_rsdt_entry(uint32_t index);

// RTC/Time
uint8_t hw_rtc_read(uint8_t reg);

#endif
