#include "arch/x86/hw_detect.h"
#include "arch/x86/ports.h"
#include "drivers/serial.h"
#include "diag.h"
#include "util.h"
#include "kernel.h"

static hw_pci_device_t pci_devices[256];
static uint32_t pci_device_count_value = 0;

static uint32_t acpi_rsdp_phys = 0;
static uint32_t acpi_rsdt_phys = 0;
static uint32_t acpi_rsdt_count = 0;

// x86 PCI Config Space access
static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(0x80000000u | ((uint32_t)bus << 16)
        | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
}

void hw_pci_scan(void) {
    pci_device_count_value = 0;
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read((uint8_t)bus, slot, func, 0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);
                uint32_t class_reg = pci_config_read((uint8_t)bus, slot, func, 8);
                uint8_t class_id = (uint8_t)((class_reg >> 24) & 0xFF);
                uint8_t subclass = (uint8_t)((class_reg >> 16) & 0xFF);
                
                if (pci_device_count_value < 256) {
                    pci_devices[pci_device_count_value].bus = (uint8_t)bus;
                    pci_devices[pci_device_count_value].slot = slot;
                    pci_devices[pci_device_count_value].func = func;
                    pci_devices[pci_device_count_value].vendor = vendor;
                    pci_devices[pci_device_count_value].device = device;
                    pci_devices[pci_device_count_value].class_id = class_id;
                    pci_devices[pci_device_count_value].subclass = subclass;
                    pci_device_count_value++;
                }
            }
        }
    }
    diag_log_hex32(DIAG_INFO, "pci devices", pci_device_count_value);
}

uint32_t hw_pci_get_device_count(void) {
    return pci_device_count_value;
}

int hw_pci_get_device_info(uint32_t index, hw_pci_device_t* out_info) {
    if (index >= pci_device_count_value || !out_info) return 0;
    *out_info = pci_devices[index];
    return 1;
}

// ACPI Helpers
static uint8_t acpi_checksum(const uint8_t* ptr, uint32_t len) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) sum = (uint8_t)(sum + ptr[i]);
    return sum;
}

static int acpi_rsdp_valid(const uint8_t* ptr) {
    static const uint8_t sig[8] = { 'R','S','D',' ','P','T','R',' ' };
    if (memcmp(ptr, sig, 8) != 0) return 0;
    if (acpi_checksum(ptr, 20) != 0) return 0;
    return 1;
}

void hw_acpi_init(void) {
    serial_write_string("DEBUG: Scanning for ACPI RSDP (x86)...\n");
    acpi_rsdp_phys = 0;
    acpi_rsdt_phys = 0;

    // Scan extended BIOS area and BIOS ROM for RSDP
    for (uint32_t p = 0xE0000; p < 0x100000; p += 16) {
        if (acpi_rsdp_valid((const uint8_t*)p)) {
            acpi_rsdp_phys = p;
            break;
        }
    }

    if (acpi_rsdp_phys) {
        typedef struct {
            uint8_t signature[8];
            uint8_t checksum;
            uint8_t oemid[6];
            uint8_t revision;
            uint32_t rsdt_address;
        } acpi_rsdp_t;

        acpi_rsdp_t* rsdp = (acpi_rsdp_t*)acpi_rsdp_phys;
        acpi_rsdt_phys = rsdp->rsdt_address;
        
        if (acpi_rsdt_phys) {
            acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)acpi_rsdt_phys;
            acpi_rsdt_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;
            diag_log_hex32(DIAG_INFO, "ACPI RSDT found at", acpi_rsdt_phys);
        }
    }
}

uintptr_t hw_acpi_find_table(const char* signature) {
    if (!acpi_rsdt_phys) return 0;
    for (uint32_t i = 0; i < acpi_rsdt_count; ++i) {
        uint32_t entry = hw_acpi_get_rsdt_entry(i);
        if (!entry) continue;
        acpi_sdt_header_t* hdr = (acpi_sdt_header_t*)entry;
        if (memcmp(hdr->signature, signature, 4) == 0) return entry;
    }
    return 0;
}

uintptr_t hw_acpi_get_rsdp_addr(void) {
    return acpi_rsdp_phys;
}

uintptr_t hw_acpi_get_rsdt_addr(void) {
    return acpi_rsdt_phys;
}

uint32_t hw_acpi_get_rsdt_entries(void) {
    return acpi_rsdt_count;
}

uintptr_t hw_acpi_get_rsdt_entry(uint32_t index) {
    if (!acpi_rsdt_phys || index >= acpi_rsdt_count) return 0;
    uint32_t* entries = (uint32_t*)(acpi_rsdt_phys + sizeof(acpi_sdt_header_t));
    return entries[index];
}

uint8_t hw_rtc_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}
