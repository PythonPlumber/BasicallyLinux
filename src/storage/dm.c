#include "storage/dm.h"
#include "types.h"

int dm_create(dm_device_t* dev, block_device_t* target, uint32_t start_lba) {
    if (!dev || !target) {
        return 0;
    }
    dev->target = target;
    dev->start_lba = start_lba;
    return 1;
}

uint32_t dm_read(dm_device_t* dev, uint32_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->target) {
        return 0;
    }
    return block_read(dev->target, dev->start_lba + lba, count, buffer);
}

uint32_t dm_write(dm_device_t* dev, uint32_t lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !dev->target) {
        return 0;
    }
    return block_write(dev->target, dev->start_lba + lba, count, buffer);
}
