#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_addr);
uint8_t rtc_read(uint8_t reg);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_scan_bus(void);
uint32_t pci_device_count(void);
uint32_t pci_device_info(uint32_t index, uint8_t* bus, uint8_t* slot, uint8_t* func, uint16_t* vendor, uint16_t* device, uint8_t* class_id, uint8_t* subclass);
uint32_t acpi_rsdp_addr(void);
uint32_t acpi_rsdt_addr(void);
uint32_t acpi_rsdt_entries(void);
uint32_t acpi_rsdt_entry(uint32_t index);
uint32_t acpi_find_table(const char* signature);

typedef struct {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oemid[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_t;

typedef struct {
    acpi_madt_entry_t header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_local_apic_t;

#endif
