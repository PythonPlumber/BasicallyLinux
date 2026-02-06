#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H

#include "types.h"

typedef struct {
    uint32_t entries;
} sys_config_state_t;

void sys_config_init(void);
sys_config_state_t sys_config_state(void);

#endif

