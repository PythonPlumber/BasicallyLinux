#include "smp_rally.h"
#include "types.h"

static uint32_t cpu_count = 1;
static uint32_t online_mask = 1;

void smp_rally_init(void) {
    cpu_count = 1;
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
