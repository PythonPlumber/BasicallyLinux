#include "power_plane.h"
#include "types.h"

static power_plane_t plane;

void power_plane_init(void) {
    plane.state = 0;
    plane.thermal = 0;
    plane.min_mhz = 800;
    plane.max_mhz = 3200;
    plane.freq_mhz = plane.max_mhz;
}

void power_plane_set_state(uint32_t state) {
    plane.state = state;
}

void power_plane_update_thermal(uint32_t value) {
    plane.thermal = value;
    if (plane.thermal > 85 && plane.freq_mhz > plane.min_mhz) {
        plane.freq_mhz = plane.min_mhz;
    } else if (plane.thermal < 70 && plane.freq_mhz < plane.max_mhz) {
        plane.freq_mhz = plane.max_mhz;
    }
}

void power_plane_set_freq(uint32_t mhz) {
    if (mhz < plane.min_mhz) {
        mhz = plane.min_mhz;
    }
    if (mhz > plane.max_mhz) {
        mhz = plane.max_mhz;
    }
    plane.freq_mhz = mhz;
}

power_plane_t power_plane_state(void) {
    return plane;
}
