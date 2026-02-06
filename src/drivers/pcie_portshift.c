#include "drivers/pcie_portshift.h"
#include "types.h"

void pcie_portshift_init(void) {
}

int pcie_portshift_hotplug(uint32_t port_id) {
    (void)port_id;
    return 1;
}
