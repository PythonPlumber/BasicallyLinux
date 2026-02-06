#include "power/acpi_power.h"
#include "arch/x86/timer.h"
#include "types.h"

static acpi_power_state_t power_state;

void acpi_power_init(void) {
    power_state.state = 0;
    power_state.sleep_target = 0;
    power_state.last_transition = 0;
}

void acpi_power_set_state(uint32_t state) {
    power_state.state = state;
    power_state.last_transition = (uint32_t)timer_get_ticks();
}

void acpi_power_request_sleep(uint32_t state) {
    power_state.sleep_target = state;
    power_state.last_transition = (uint32_t)timer_get_ticks();
}

acpi_power_state_t acpi_power_state(void) {
    return power_state;
}
