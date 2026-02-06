#ifndef POWER_PLANE_H
#define POWER_PLANE_H

#include "types.h"

typedef struct {
    uint32_t state;
    uint32_t thermal;
    uint32_t freq_mhz;
    uint32_t min_mhz;
    uint32_t max_mhz;
} power_plane_t;

void power_plane_init(void);
void power_plane_set_state(uint32_t state);
void power_plane_update_thermal(uint32_t value);
void power_plane_set_freq(uint32_t mhz);
power_plane_t power_plane_state(void);

#endif
