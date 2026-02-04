#ifndef DM_H
#define DM_H

#include "block.h"
#include "types.h"

typedef struct {
    block_device_t* target;
    uint32_t start_lba;
} dm_device_t;

int dm_create(dm_device_t* dev, block_device_t* target, uint32_t start_lba);
uint32_t dm_read(dm_device_t* dev, uint32_t lba, uint32_t count, uint8_t* buffer);
uint32_t dm_write(dm_device_t* dev, uint32_t lba, uint32_t count, const uint8_t* buffer);

#endif
