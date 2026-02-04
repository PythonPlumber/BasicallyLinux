#include "ai_model.h"
#include "diag.h"
#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "framebuffer.h"
#include "heap.h"
#include "apex_intc.h"
#include "msgi.h"
#include "smp_rally.h"
#include "usb_nexus.h"
#include "pcie_portshift.h"
#include "gfx_forge.h"
#include "input_stream.h"
#include "driver_grid.h"
#include "power_plane.h"
#include "acpi_power.h"
#include "net_stack.h"
#include "secure_hard.h"
#include "secure_policy.h"
#include "secure_caps.h"
#include "secure_audit.h"
#include "trace_forge.h"
#include "perf_meter.h"
#include "elf_loader.h"
#include "dyn_loader.h"
#include "init_orch.h"
#include "tty_core.h"
#include "pty_mux.h"
#include "devnodes.h"
#include "proc_view.h"
#include "sys_view.h"
#include "sys_config.h"
#include "inode_cache.h"
#include "ipc.h"
#include "io_sched.h"
#include "job.h"
#include "journal.h"
#include "kswapd.h"
#include "multiboot.h"
#include "numa.h"
#include "page_cache.h"
#include "pmm.h"
#include "paging.h"
#include "keyboard.h"
#include "ramdisk.h"
#include "serial.h"
#include "slab.h"
#include "syscall.h"
#include "sched.h"
#include "selftest.h"
#include "shell.h"
#include "timer.h"
#include "util.h"
#include "fs.h"
#include "fs_types.h"
#include "vfs.h"
#include "vga.h"
#include "ports.h"

extern uint32_t kernel_start;
extern uint32_t kernel_end;

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t align_down(uint32_t value, uint32_t align) {
    return value & ~(align - 1);
}

int cpu_has_sse(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (edx >> 25) & 1u;
}

int cpu_has_sse41(void) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ecx >> 19) & 1u;
}

void sse_enable(void) {
    uint32_t cr0;
    uint32_t cr4;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x4u;
    cr0 |= 0x2u;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x600u;
    asm volatile("mov %0, %%cr4" : : "r"(cr4));
}

void enable_interrupts(void) {
    asm volatile("sti");
}

void disable_interrupts(void) {
    asm volatile("cli");
}

void fpu_enable(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x4u;
    cr0 |= 0x2u;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

void cpu_get_vendor(char* out, uint32_t max) {
    if (!out || max < 13) {
        return;
    }
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    ((uint32_t*)out)[0] = b;
    ((uint32_t*)out)[1] = d;
    ((uint32_t*)out)[2] = c;
    out[12] = 0;
}

void cpu_reboot(void) {
    disable_interrupts();
    for (uint32_t i = 0; i < 100000; ++i) { }
    outb(0x64, 0xFE);
    for (;;) {
        asm volatile("hlt");
    }
}

uint8_t rtc_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor;
    uint16_t device;
    uint8_t class_id;
    uint8_t subclass;
} pci_device_t;

static pci_device_t pci_devices[256];
static uint32_t pci_device_count_value = 0;

typedef struct {
    uint8_t signature[8];
    uint8_t checksum;
    uint8_t oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} acpi_rsdp_t;

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

static uint32_t acpi_rsdp_phys = 0;
static uint32_t acpi_rsdt_phys = 0;
static uint32_t acpi_rsdt_count = 0;

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(0x80000000u | ((uint32_t)bus << 16)
        | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
    outb(0xCF8, (uint8_t)(address & 0xFF));
    outb(0xCF8 + 1, (uint8_t)((address >> 8) & 0xFF));
    outb(0xCF8 + 2, (uint8_t)((address >> 16) & 0xFF));
    outb(0xCF8 + 3, (uint8_t)((address >> 24) & 0xFF));
    uint32_t data = 0;
    data |= (uint32_t)inb(0xCFC);
    data |= (uint32_t)inb(0xCFC + 1) << 8;
    data |= (uint32_t)inb(0xCFC + 2) << 16;
    data |= (uint32_t)inb(0xCFC + 3) << 24;
    return data;
}

uint32_t pci_scan_bus(void) {
    pci_device_count_value = 0;
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read((uint8_t)bus, slot, func, 0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    if (func == 0) {
                        break;
                    }
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
    return pci_device_count_value;
}

uint32_t pci_device_count(void) {
    return pci_device_count_value;
}

uint32_t pci_device_info(uint32_t index, uint8_t* bus, uint8_t* slot, uint8_t* func, uint16_t* vendor, uint16_t* device, uint8_t* class_id, uint8_t* subclass) {
    if (index >= pci_device_count_value) {
        return 0;
    }
    pci_device_t* dev = &pci_devices[index];
    if (bus) {
        *bus = dev->bus;
    }
    if (slot) {
        *slot = dev->slot;
    }
    if (func) {
        *func = dev->func;
    }
    if (vendor) {
        *vendor = dev->vendor;
    }
    if (device) {
        *device = dev->device;
    }
    if (class_id) {
        *class_id = dev->class_id;
    }
    if (subclass) {
        *subclass = dev->subclass;
    }
    return 1;
}

static uint8_t acpi_checksum(const uint8_t* ptr, uint32_t len) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) {
        sum = (uint8_t)(sum + ptr[i]);
    }
    return sum;
}

static int acpi_rsdp_valid(const uint8_t* ptr) {
    static const uint8_t sig[8] = { 'R','S','D',' ','P','T','R',' ' };
    if (memcmp(ptr, sig, 8) != 0) {
        return 0;
    }
    if (acpi_checksum(ptr, 20) != 0) {
        return 0;
    }
    return 1;
}

static void acpi_scan(void) {
    acpi_rsdp_phys = 0;
    acpi_rsdt_phys = 0;
    acpi_rsdt_count = 0;
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const uint8_t* ptr = (const uint8_t*)addr;
        if (acpi_rsdp_valid(ptr)) {
            const acpi_rsdp_t* rsdp = (const acpi_rsdp_t*)ptr;
            acpi_rsdp_phys = addr;
            acpi_rsdt_phys = rsdp->rsdt_address;
            break;
        }
    }
    if (!acpi_rsdt_phys) {
        diag_log(DIAG_WARN, "acpi rsdt not found");
        return;
    }
    const acpi_sdt_header_t* rsdt = (const acpi_sdt_header_t*)acpi_rsdt_phys;
    if (memcmp(rsdt->signature, "RSDT", 4) != 0) {
        diag_log(DIAG_WARN, "acpi rsdt signature invalid");
        acpi_rsdt_phys = 0;
        return;
    }
    if (rsdt->length < sizeof(acpi_sdt_header_t)) {
        diag_log(DIAG_WARN, "acpi rsdt length invalid");
        acpi_rsdt_phys = 0;
        return;
    }
    acpi_rsdt_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;
    diag_log_hex32(DIAG_INFO, "acpi rsdp", acpi_rsdp_phys);
    diag_log_hex32(DIAG_INFO, "acpi rsdt", acpi_rsdt_phys);
}

uint32_t acpi_rsdp_addr(void) {
    return acpi_rsdp_phys;
}

uint32_t acpi_rsdt_addr(void) {
    return acpi_rsdt_phys;
}

uint32_t acpi_rsdt_entries(void) {
    return acpi_rsdt_count;
}

uint32_t acpi_rsdt_entry(uint32_t index) {
    if (!acpi_rsdt_phys || index >= acpi_rsdt_count) {
        return 0;
    }
    const acpi_sdt_header_t* rsdt = (const acpi_sdt_header_t*)acpi_rsdt_phys;
    const uint32_t* entries = (const uint32_t*)((const uint8_t*)rsdt + sizeof(acpi_sdt_header_t));
    return entries[index];
}

static void task_a(void) {
    const char spin[] = "|/-\\";
    uint32_t i = 0;
    for (;;) {
        fb_console_putc(spin[i & 3]);
        fb_console_putc('\b');
        for (volatile uint32_t d = 0; d < 200000; ++d) { }
        ++i;
    }
}

static void task_b(void) {
    const char spin[] = "|/-\\";
    uint32_t i = 0;
    for (;;) {
        fb_console_putc(spin[i & 3]);
        fb_console_putc('\b');
        for (volatile uint32_t d = 0; d < 200000; ++d) { }
        ++i;
    }
}

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_addr) {
    gdt_init();
    idt_init();
    isr_init();
    syscall_init();
    serial_init();
    diag_init();
    diag_log(DIAG_INFO, "boot start");

    multiboot_info_t* info = 0;
    uint32_t mem_bytes = 16u * 1024u * 1024u;
    if (multiboot_magic == 0x2BADB002) {
        info = (multiboot_info_t*)multiboot_info_addr;
        if (info->flags & 0x1) {
            mem_bytes = (info->mem_upper + 1024) * 1024;
        }
    }
    if (mem_bytes > 0x100000) {
        pmm_init(0x100000, mem_bytes - 0x100000);
    } else {
        pmm_init(0x100000, 0);
    }
    diag_log_hex32(DIAG_INFO, "mem_bytes", mem_bytes);
    pmm_reserve_region((uint32_t)&kernel_start, (uint32_t)&kernel_end - (uint32_t)&kernel_start);
    if (info) {
        pmm_reserve_region(align_down((uint32_t)info, 4096), align_up(sizeof(multiboot_info_t), 4096));
        if (info->mods_count > 0) {
            pmm_reserve_region(align_down(info->mods_addr, 4096), align_up(info->mods_count * sizeof(multiboot_module_t), 4096));
            multiboot_module_t* module = (multiboot_module_t*)info->mods_addr;
            for (uint32_t i = 0; i < info->mods_count; ++i) {
                uint32_t start = module[i].mod_start;
                uint32_t size = module[i].mod_end - module[i].mod_start;
                pmm_reserve_region(start, size);
            }
        }
    }

    paging_init();
    acpi_scan();
    apex_intc_init();
    msgi_init();
    smp_rally_init();
    power_plane_init();
    acpi_power_init();
    secure_hard_init();
    secure_policy_init();
    secure_caps_init();
    secure_audit_init();
    trace_forge_init();
    usb_nexus_init();
    pcie_portshift_init();
    gfx_forge_init();
    input_stream_init();
    driver_grid_init();
    net_stack_init();
    dyn_loader_init();
    init_orch_init();
    tty_core_init();
    pty_mux_init();
    devnodes_init();
    proc_view_init();
    sys_view_init();
    sys_config_init();
    if (cpu_has_sse()) {
        sse_enable();
        ai_set_simd_enabled(cpu_has_sse41());
    } else {
        ai_set_simd_enabled(0);
    }
    ai_model_init();
    heap_init();
    slab_init();
    kswapd_init();
    numa_init(get_ram_size());
    ipc_init();
    io_sched_init();
    journal_init();
    inode_cache_init();
    page_cache_init();
    fs_init();
    ext4_init();
    xfs_init();
    btrfs_init();
    job_init();
    fb_init(info);
    fb_clear(0);
    fb_console_init(0xFFFFFF, 0);
    fb_console_write("Kernel Loaded Successfully\n");
    vfs_init();

    if (info && info->mods_count > 0) {
        multiboot_module_t* module = (multiboot_module_t*)info->mods_addr;
        uint32_t size = module->mod_end - module->mod_start;
        vfs_set_root(ramdisk_init(module->mod_start, size));
    }

    scheduler_init();
    process_create(task_a, 0);
    process_create(task_b, 0);
    pit_init(100);
    enable_interrupts();
    vga_display_splash();
    fb_clear(0);
    fb_console_init(0xFFFFFF, 0);
    keyboard_init();
    keyboard_set_callback(shell_on_input);
    shell_init();
#ifdef SELFTEST_AUTORUN
    selftest_run();
#endif

    for (;;) {
        asm volatile("hlt");
    }
}
