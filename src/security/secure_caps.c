#include "secure_caps.h"
#include "types.h"

void secure_caps_init(void) {
}

secure_caps_t secure_caps_from_mask(uint32_t mask) {
    secure_caps_t caps;
    caps.set = mask;
    return caps;
}

uint32_t secure_caps_has(secure_caps_t caps, uint32_t cap) {
    if (cap >= SECURE_CAP_MAX) {
        return 0;
    }
    return (caps.set >> cap) & 1u;
}

secure_caps_t secure_caps_add(secure_caps_t caps, uint32_t cap) {
    if (cap < SECURE_CAP_MAX) {
        caps.set |= (1u << cap);
    }
    return caps;
}

secure_caps_t secure_caps_remove(secure_caps_t caps, uint32_t cap) {
    if (cap < SECURE_CAP_MAX) {
        caps.set &= ~(1u << cap);
    }
    return caps;
}
