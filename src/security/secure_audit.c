#include "security/secure_audit.h"
#include "arch/x86/timer.h"
#include "types.h"

static secure_audit_state_t state;
static secure_audit_entry_t entries[128];
static uint32_t head = 0;
static uint32_t count = 0;

void secure_audit_init(void) {
    state.count = 0;
    state.dropped = 0;
    head = 0;
    count = 0;
    for (uint32_t i = 0; i < 128; ++i) {
        entries[i].code = 0;
        entries[i].tick = 0;
    }
}

void secure_audit_log(uint32_t code) {
    state.count++;
    entries[head].code = code;
    entries[head].tick = timer_get_ticks();
    head = (head + 1) % 128;
    if (count < 128) {
        count++;
    } else {
        state.dropped++;
    }
}

secure_audit_state_t secure_audit_state(void) {
    return state;
}

uint32_t secure_audit_get(uint32_t index, secure_audit_entry_t* out) {
    if (!out || index >= count) {
        return 0;
    }
    uint32_t start = (head + 128 - count) % 128;
    uint32_t slot = (start + index) % 128;
    *out = entries[slot];
    return 1;
}
