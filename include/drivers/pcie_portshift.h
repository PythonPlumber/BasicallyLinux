#ifndef PCIE_PORTSHIFT_H
#define PCIE_PORTSHIFT_H

#include "types.h"

void pcie_portshift_init(void);
int pcie_portshift_hotplug(uint32_t port_id);

#endif
