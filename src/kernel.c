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

static uint32_t acpi_rsdp_phys = 0;
static uint32_t acpi_rsdt_phys = 0;
static uint32_t acpi_rsdt_count = 0;

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(0x80000000u | ((uint32_t)bus << 16)
        | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
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
    serial_write_string("DEBUG: Scanning for ACPI RSDP (0xE0000 - 0x100000)...\n");
    acpi_rsdp_phys = 0;
    acpi_rsdt_phys = 0;
    acpi_rsdt_count = 0;
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if ((addr & 0xFFF) == 0) {
            serial_write_string("."); // Progress marker
        }
        const uint8_t* ptr = (const uint8_t*)addr;
        if (acpi_rsdp_valid(ptr)) {
            const acpi_rsdp_t* rsdp = (const acpi_rsdp_t*)ptr;
            acpi_rsdp_phys = addr;
            acpi_rsdt_phys = rsdp->rsdt_address;
            serial_write_string("\nDEBUG: Found RSDP at ");
            serial_write_hex32(addr);
            serial_write_string(", RSDT at ");
            serial_write_hex32(acpi_rsdt_phys);
            serial_write_string("\n");
            break;
        }
    }
    serial_write_string("\n");
    if (!acpi_rsdt_phys) {
        serial_write_string("DEBUG: ACPI RSDT not found!\n");
        diag_log(DIAG_WARN, "acpi rsdt not found");
        return;
    }
    serial_write_string("DEBUG: Validating RSDT signature...\n");
    const acpi_sdt_header_t* rsdt = (const acpi_sdt_header_t*)acpi_rsdt_phys;
    if (memcmp(rsdt->signature, "RSDT", 4) != 0) {
        serial_write_string("DEBUG: ACPI RSDT signature invalid!\n");
        diag_log(DIAG_WARN, "acpi rsdt signature invalid");
        acpi_rsdt_phys = 0;
        return;
    }
    serial_write_string("DEBUG: RSDT signature OK. Parsing entries...\n");
    if (rsdt->length < sizeof(acpi_sdt_header_t)) {
        serial_write_string("DEBUG: ACPI RSDT length invalid!\n");
        diag_log(DIAG_WARN, "acpi rsdt length invalid");
        acpi_rsdt_phys = 0;
        return;
    }
    acpi_rsdt_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;
    serial_write_string("DEBUG: ACPI RSDT parsed, count: ");
    serial_write_hex32(acpi_rsdt_count);
    serial_write_string("\n");
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

uint32_t acpi_find_table(const char* signature) {
    if (!acpi_rsdt_phys) return 0;
    for (uint32_t i = 0; i < acpi_rsdt_count; ++i) {
        uint32_t entry = acpi_rsdt_entry(i);
        if (!entry) continue;
        const acpi_sdt_header_t* hdr = (const acpi_sdt_header_t*)entry;
        if (memcmp(hdr->signature, signature, 4) == 0) {
            return entry;
        }
    }
    return 0;
}

static void task_a(void) {
    for (;;) {
        serial_write_string("A");
        fb_console_putc('A');
        for (volatile uint32_t d = 0; d < 10000000; ++d) { }
    }
}

static void task_b(void) {
    for (;;) {
        serial_write_string("B");
        fb_console_putc('B');
        for (volatile uint32_t d = 0; d < 10000000; ++d) { }
    }
}

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_addr) {
    serial_init();
    serial_write_string("K-ENTRY\n");
    
    uint16_t* vga_ptr = (uint16_t*)0xB8000;
    vga_ptr[0] = 0x0F31; // '1'
    
    serial_write_string("DEBUG: GDT Init...\n");
    gdt_init();
    vga_ptr[1] = 0x0F32; // '2'
    serial_write_string("DEBUG: IDT Init...\n");
    idt_init();
    vga_ptr[2] = 0x0F33; // '3'
    serial_write_string("DEBUG: ISR Init...\n");
    isr_init();
    vga_ptr[3] = 0x0F34; // '4'
    syscall_init();
    serial_write_string("DEBUG: Syscall Initialized\n");
    vga_ptr[4] = 0x0F35; // '5'
    diag_init();
    vga_ptr[5] = 0x0F36; // '6'
    serial_write_string("DEBUG: Diag Initialized\n");
    diag_log(DIAG_INFO, "boot start");

    acpi_scan();

    multiboot_info_t* info = 0;
    uint32_t mem_bytes = 16u * 1024u * 1024u;
    if (multiboot_magic == 0x2BADB002) {
        info = (multiboot_info_t*)multiboot_info_addr;
        if (info->flags & 0x1) {
            mem_bytes = (info->mem_upper + 1024) * 1024;
        }
    }
    serial_write_string("DEBUG: Multiboot info parsed, Memory: ");
    serial_write_hex32(mem_bytes);
    serial_write_string(" bytes\n");

    serial_write_string("DEBUG: Starting Paging initialization...\n");
    vga_ptr[6] = 0x0F37; // '7'
    paging_init();
    vga_ptr[7] = 0x0F38; // '8'
    serial_write_string("DEBUG: Paging Initialized\n");
    asm volatile("mov %cr3, %eax; mov %eax, %cr3"); // Flush TLB just in case
    serial_write_string("DEBUG: CR3 Reloaded\n");

    serial_write_string("DEBUG: Initializing Framebuffer...\n");
    vga_ptr[8] = 0x0F39; // '9'
    fb_init(info);
    vga_ptr[9] = 0x0F41; // 'A'
    serial_write_string("DEBUG: Framebuffer Initialized\n");
    fb_clear(0);
    vga_ptr[10] = 0x0F42; // 'B'
    serial_write_string("DEBUG: Framebuffer Cleared\n");
    fb_console_init(0xFFFFFF, 0);
    serial_write_string("DEBUG: Console Initialized\n");
    fb_console_write("Kernel Loading...\n");
    fb_console_write("Paging Initialized\n");
    serial_write_string("DEBUG: First Console Writes Done\n");

    // Direct VGA Hardware Test
    uint16_t* vga = (uint16_t*)0xB8000;
    vga[0] = 0x0F48; // 'H' white on black
    vga[1] = 0x0F49; // 'I' white on black
    serial_write_string("DEBUG: Direct VGA Test written to 0xB8000\n");

    serial_write_string("DEBUG: Initializing Interrupt Controller...\n");
    apex_intc_init();
    serial_write_string("DEBUG: Initializing MSGI...\n");
    msgi_init();
    serial_write_string("DEBUG: Initializing SMP...\n");
    smp_rally_init();
    serial_write_string("DEBUG: SMP/Interrupt Controller Initialized\n");
    fb_console_write("SMP/Intc Initialized\n");

    serial_write_string("DEBUG: Initializing Power Subsystems...\n");
    power_plane_init();
    serial_write_string("DEBUG: Power Plane Initialized\n");
    acpi_power_init();
    serial_write_string("DEBUG: ACPI Power Initialized\n");
    serial_write_string("DEBUG: Initializing Security...\n");
    secure_hard_init();
    serial_write_string("DEBUG: Secure Hard Initialized\n");
    secure_policy_init();
    serial_write_string("DEBUG: Secure Policy Initialized\n");
    secure_caps_init();
    serial_write_string("DEBUG: Secure Caps Initialized\n");
    secure_audit_init();
    serial_write_string("DEBUG: Secure Audit Initialized\n");
    serial_write_string("DEBUG: Initializing Trace/USB/PCIe/GFX...\n");
    trace_forge_init();
    serial_write_string("DEBUG: Trace Forge Initialized\n");
    usb_nexus_init();
    serial_write_string("DEBUG: USB Nexus Initialized\n");
    pcie_portshift_init();
    serial_write_string("DEBUG: PCIe Portshift Initialized\n");
    gfx_forge_init();
    serial_write_string("DEBUG: GFX Forge Initialized\n");
    serial_write_string("DEBUG: Initializing Input/DriverGrid/Net/Loader...\n");
    input_stream_init();
    serial_write_string("DEBUG: Input Stream Initialized\n");
    driver_grid_init();
    serial_write_string("DEBUG: Driver Grid Initialized\n");
    net_stack_init();
    serial_write_string("DEBUG: Net Stack Initialized\n");
    dyn_loader_init();
    serial_write_string("DEBUG: Dyn Loader Initialized\n");
    init_orch_init();
    serial_write_string("DEBUG: Init Orch Initialized\n");
    serial_write_string("DEBUG: Initializing TTY/PTY/Devnodes...\n");
    tty_core_init();
    serial_write_string("DEBUG: TTY Core Initialized\n");
    pty_mux_init();
    serial_write_string("DEBUG: PTY Mux Initialized\n");
    devnodes_init();
    serial_write_string("DEBUG: Devnodes Initialized\n");
    serial_write_string("DEBUG: Initializing View/Config...\n");
    proc_view_init();
    serial_write_string("DEBUG: Proc View Initialized\n");
    sys_view_init();
    serial_write_string("DEBUG: Sys View Initialized\n");
    sys_config_init();
    serial_write_string("DEBUG: Sys Config Initialized\n");
    serial_write_string("DEBUG: Subsystems Initialized\n");
    fb_console_write("Subsystems Initialized\n");

    serial_write_string("DEBUG: Checking SSE...\n");
    if (cpu_has_sse()) {
        serial_write_string("DEBUG: Enabling SSE...\n");
        sse_enable();
        ai_set_simd_enabled(cpu_has_sse41());
    } else {
        serial_write_string("DEBUG: SSE not available\n");
        ai_set_simd_enabled(0);
    }
    serial_write_string("DEBUG: Initializing AI Model...\n");
    ai_model_init();
    serial_write_string("DEBUG: AI Model Initialized\n");
    fb_console_write("AI Model Initialized\n");

    serial_write_string("DEBUG: Initializing Heap/Slab...\n");
    heap_init();
    slab_init();
    serial_write_string("DEBUG: Initializing kswapd...\n");
    kswapd_init();
    serial_write_string("DEBUG: Memory Management (Heap/Slab) Initialized\n");
    fb_console_write("Heap/Slab Initialized\n");

    serial_write_string("DEBUG: Initializing NUMA/IPC/IO/Journal/Cache/FS...\n");
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
    serial_write_string("DEBUG: FS/IPC/Jobs Initialized\n");
    fb_console_write("FS/IPC/Jobs Initialized\n");

    serial_write_string("DEBUG: Initializing VFS...\n");
    vfs_init();
    serial_write_string("DEBUG: VFS Initialized\n");
    fb_console_write("VFS Initialized\n");

    serial_write_string("DEBUG: Checking for Ramdisk modules...\n");
    if (info && info->mods_count > 0) {
        serial_write_string("DEBUG: Initializing Ramdisk...\n");
        multiboot_module_t* module = (multiboot_module_t*)info->mods_addr;
        uint32_t size = module->mod_end - module->mod_start;
        vfs_set_root(ramdisk_init(module->mod_start, size));
        serial_write_string("DEBUG: Ramdisk mounted\n");
        fb_console_write("Ramdisk Mounted\n");
    } else {
        serial_write_string("DEBUG: No Ramdisk modules found\n");
    }

    serial_write_string("DEBUG: Initializing Scheduler...\n");
    scheduler_init();
    serial_write_string("DEBUG: Scheduler Initialized\n");
    fb_console_write("Scheduler Initialized\n");

    serial_write_string("DEBUG: Initializing PIT...\n");
    pit_init(100);
    serial_write_string("DEBUG: PIT Initialized\n");
    fb_console_write("PIT Initialized\n");

    serial_write_string("DEBUG: Creating tasks...\n");
    process_create(task_a, "task_a");
    serial_write_string("DEBUG: Task A Created\n");
    process_create(task_b, "task_b");
    serial_write_string("DEBUG: Task B Created\n");
    fb_console_write("Tasks Created\n");
    
    fb_console_write("Shell Initializing...\n");
    serial_write_string("DEBUG: Initializing Keyboard...\n");
    keyboard_init();
    keyboard_set_callback(shell_on_input);
    serial_write_string("DEBUG: Initializing Shell...\n");
    shell_init();
    serial_write_string("DEBUG: Shell Initialized\n");
    fb_console_write("Shell Initialized\n");

    serial_write_string("DEBUG: Kernel Main complete, enabling interrupts...\n");
    fb_console_write("Enabling Interrupts...\n");
    enable_interrupts();
    serial_write_string("DEBUG: Interrupts enabled\n");
    fb_console_write("Interrupts Enabled\n");
#ifdef SELFTEST_AUTORUN
    serial_write_string("DEBUG: Running Selftests...\n");
    selftest_run();
#endif

    serial_write_string("DEBUG: Entering Kernel Idle Loop\n");
    for (;;) {
        asm volatile("hlt");
    }
}
