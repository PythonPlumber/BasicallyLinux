#include "types.h"

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_addr);
void enable_interrupts(void);
void disable_interrupts(void);
void fpu_enable(void);
void sse_enable(void);
int cpu_has_sse(void);
int cpu_has_sse41(void);
void cpu_get_vendor(char* out, uint32_t max);
void cpu_reboot(void);
uint8_t rtc_read(uint8_t reg);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_scan_bus(void);
uint32_t pci_device_count(void);
uint32_t pci_device_info(uint32_t index, uint8_t* bus, uint8_t* slot, uint8_t* func, uint16_t* vendor, uint16_t* device, uint8_t* class_id, uint8_t* subclass);
uint32_t acpi_rsdp_addr(void);
uint32_t acpi_rsdt_addr(void);
uint32_t acpi_rsdt_entries(void);
uint32_t acpi_rsdt_entry(uint32_t index);
