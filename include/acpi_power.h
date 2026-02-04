#ifndef ACPI_POWER_H
#define ACPI_POWER_H

#include "types.h"

typedef struct {
    uint32_t state;
    uint32_t sleep_target;
    uint32_t last_transition;
} acpi_power_state_t;

void acpi_power_init(void);
void acpi_power_set_state(uint32_t state);
void acpi_power_request_sleep(uint32_t state);
acpi_power_state_t acpi_power_state(void);

#endif
