#include "ai/ai_model.h"
#include "diag.h"
#include "kernel.h"
#include "video/framebuffer.h"
#include "mem/heap.h"
#include "smp.h"
#include "cpu.h"
#include "drivers/usb_nexus.h"
#include "drivers/pcie_portshift.h"
#include "drivers/gfx_forge.h"
#include "drivers/input_stream.h"
#include "drivers/driver_grid.h"
#include "power/power_plane.h"
#include "power/acpi_power.h"
#include "net/net_stack.h"
#include "security/secure_hard.h"
#include "security/secure_policy.h"
#include "security/secure_caps.h"
#include "security/secure_audit.h"
#include "trace/trace_forge.h"
#include "trace/perf_meter.h"
#include "user/elf_loader.h"
#include "user/dyn_loader.h"
#include "user/init_orch.h"
#include "term/tty_core.h"
#include "term/pty_mux.h"
#include "sys/devnodes.h"
#include "sys/proc_view.h"
#include "sys/sys_view.h"
#include "sys/sys_config.h"
#include "fs/inode_cache.h"
#include "ipc/ipc.h"
#include "storage/io_sched.h"
#include "kernel/job.h"
#include "fs/journal.h"
#include "mem/kswapd.h"
#include "mem/numa.h"
#include "fs/page_cache.h"
#include "mem/pmm.h"
#include "drivers/keyboard.h"
#include "ramdisk.h"
#include "drivers/serial.h"
#include "mem/slab.h"
#include "cpu/syscall.h"
#include "kernel/sched.h"
#include "selftest.h"
#include "shell/shell.h"
#include "arch/x86/timer.h"
#include "arch/x86/hw_detect.h"
#include "util.h"
#include "fs/fs.h"
#include "fs/fs_types.h"
#include "vfs.h"
#include "drivers/vga.h"

extern uint32_t kernel_start;
extern uint32_t kernel_end;

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uint32_t align_down(uint32_t value, uint32_t align) {
    return value & ~(align - 1);
}

uint8_t rtc_read(uint8_t reg) {
    return hw_rtc_read(reg);
}

typedef hw_pci_device_t pci_device_t;

uint32_t pci_scan_bus(void) {
    hw_pci_scan();
    return hw_pci_get_device_count();
}

uint32_t pci_device_count(void) {
    return hw_pci_get_device_count();
}

uint32_t pci_device_info(uint32_t index, uint8_t* bus, uint8_t* slot, uint8_t* func, uint16_t* vendor, uint16_t* device, uint8_t* class_id, uint8_t* subclass) {
    hw_pci_device_t dev;
    if (!hw_pci_get_device_info(index, &dev)) return 0;
    if (bus) *bus = dev.bus;
    if (slot) *slot = dev.slot;
    if (func) *func = dev.func;
    if (vendor) *vendor = dev.vendor;
    if (device) *device = dev.device;
    if (class_id) *class_id = dev.class_id;
    if (subclass) *subclass = dev.subclass;
    return 1;
}

static void acpi_scan(void) {
    hw_acpi_init();
}

uint32_t acpi_rsdp_addr(void) {
    return hw_acpi_get_rsdp_addr();
}

uint32_t acpi_rsdt_addr(void) {
    return hw_acpi_get_rsdt_addr();
}

uint32_t acpi_rsdt_entries(void) {
    return hw_acpi_get_rsdt_entries();
}

uint32_t acpi_rsdt_entry(uint32_t index) {
    return hw_acpi_get_rsdt_entry(index);
}

uint32_t acpi_find_table(const char* signature) {
    return hw_acpi_find_table(signature);
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

typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_addr) {
    serial_init();
    serial_write_string("K-ENTRY\n");
    
    // 1. Diagnostics
    diag_init();
    diag_log(DIAG_INFO, "Kernel booting...");

    multiboot_info_t* info = 0;
    if (multiboot_magic == 0x2BADB002) {
        info = (multiboot_info_t*)multiboot_info_addr;
    }

    // 2. Physical Memory Manager Early Init
    // We'll place the PMM bitmap right after the kernel.
    uint32_t pmm_bitmap_start = align_up((uint32_t)&kernel_end, 4096);
    // Assume max 4GB for now, but limit to mem_upper if available to save bitmap space.
    uint32_t max_mem = 0xFFFFFFFF;
    if (info && (info->flags & 0x1)) {
        max_mem = (info->mem_upper + 1024) * 1024;
    }
    
    // pmm_init now marks everything as USED by default.
    pmm_init(pmm_bitmap_start, max_mem);
    
    // 3. Add usable regions to PMM
    if (info && (info->flags & 0x40)) { // Memory Map available
        multiboot_mmap_entry_t* mmap = (multiboot_mmap_entry_t*)info->mmap_addr;
        while ((uint32_t)mmap < info->mmap_addr + info->mmap_length) {
            if (mmap->type == 1) { // Available
                pmm_add_region((uint32_t)mmap->addr, (uint32_t)mmap->len);
            }
            mmap = (multiboot_mmap_entry_t*)((uint32_t)mmap + mmap->size + 4);
        }
    } else if (info && (info->flags & 0x1)) {
        // Fallback to basic memory info
        pmm_add_region(0x100000, (info->mem_upper * 1024));
    } else {
        // Assume at least 16MB if no info
        pmm_add_region(0x100000, 15 * 1024 * 1024);
    }

    // 4. Reserve critical regions
    // Reserve kernel and PMM bitmap
    uint32_t k_start = (uint32_t)&kernel_start;
    uint32_t k_end = (uint32_t)&kernel_end;
    uint32_t pmm_bitmap_size = ((max_mem / 4096 + 31) / 32) * 4;
    pmm_reserve_region(k_start, (k_end - k_start) + pmm_bitmap_size + 4096);
    
    // Reserve multiboot info and modules
    if (info) {
        pmm_reserve_region(multiboot_info_addr, sizeof(multiboot_info_t));
        if (info->flags & 0x8) { // Modules
            multiboot_module_t* mods = (multiboot_module_t*)info->mods_addr;
            for (uint32_t i = 0; i < info->mods_count; ++i) {
                pmm_reserve_region(mods[i].mod_start, mods[i].mod_end - mods[i].mod_start);
            }
        }
    }

    // 5. Early Hardware Detection (Needed for MMU/ACPI)
    acpi_scan();

    // 6. Kernel Heap & Slab
    heap_init();
    slab_init();
    kswapd_init();

    // 7. Architecture Initialization (GDT, IDT, Paging)
    arch_init();
    
    // 8. Interrupt Controller Generic Init
    apex_intc_init();

    // 9. Framebuffer & Console
    fb_init(info);
    fb_clear(0);
    fb_console_init(0xFFFFFF, 0);
    fb_console_write("Kernel Booting...\n");
    fb_console_write("Memory Management Initialized\n");

    // 10. Remaining Subsystems
    msgi_init();
    smp_init();
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

    if (cpu_has_feature(CPU_FEATURE_SSE)) {
        cpu_enable_feature(CPU_FEATURE_SSE);
        ai_set_simd_enabled(cpu_has_feature(CPU_FEATURE_SSE41));
    } else {
        ai_set_simd_enabled(0);
    }
    ai_model_init();

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
    vfs_init();

    // 11. Ramdisk / Init
    if (info && info->mods_count > 0) {
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

    serial_write_string("DEBUG: Creating tasks...\n");
    process_create(task_a, 0);
    serial_write_string("DEBUG: Task A Created\n");
    process_create(task_b, 0);
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

    serial_write_string("DEBUG: Kernel Main complete, entering scheduler loop...\n");
    fb_console_write("Entering Scheduler Loop...\n");
    interrupts_enable();
    
    scheduler_loop();
}
