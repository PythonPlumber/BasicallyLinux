#include "smp_rally.h"
#include "kernel.h"
#include "types.h"
#include "util.h"

static uint32_t cpu_count = 1;
static uint32_t online_mask = 1;

void smp_rally_init(void) {
    cpu_count = 1;
    online_mask = 1;

    uint32_t madt_addr = acpi_find_table("APIC");
    if (!madt_addr) {
        return;
    }

    acpi_madt_t* madt = (acpi_madt_t*)madt_addr;
    uint32_t length = madt->header.length;
    uint32_t current = madt_addr + sizeof(acpi_madt_t);
    uint32_t end = madt_addr + length;

    cpu_count = 0;
    while (current < end) {
        acpi_madt_entry_t* entry = (acpi_madt_entry_t*)current;
        if (entry->length == 0) break; // Prevent infinite loop

        if (entry->type == 0) { // Local APIC
            acpi_madt_local_apic_t* lapic = (acpi_madt_local_apic_t*)current;
            if (lapic->flags & 1) { // Enabled
                cpu_count++;
            }
        }
        current += entry->length;
    }

    if (cpu_count == 0) cpu_count = 1;
    online_mask = 1;
}

uint32_t smp_rally_cpu_count(void) {
    return cpu_count;
}

int smp_rally_boot(uint32_t cpu_id) {
    if (cpu_id == 0) {
        return 1;
    }
    if (cpu_id >= cpu_count) {
        return 0;
    }
    online_mask |= (1u << cpu_id);
    return 1;
}

uint32_t smp_rally_online_mask(void) {
    return online_mask;
}

int smp_rally_add_cpu(uint32_t cpu_id) {
    if (cpu_id >= 32) {
        return 0;
    }
    if (cpu_id >= cpu_count) {
        cpu_count = cpu_id + 1;
    }
    online_mask |= (1u << cpu_id);
    return 1;
}

int smp_rally_remove_cpu(uint32_t cpu_id) {
    if (cpu_id == 0 || cpu_id >= cpu_count) {
        return 0;
    }
    online_mask &= ~(1u << cpu_id);
    return 1;
}
