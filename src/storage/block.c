#include "block.h"
#include "types.h"

#define BLOCK_MAX_DEVICES 8

static block_device_t* devices[BLOCK_MAX_DEVICES];
static uint32_t device_count = 0;

int block_register(block_device_t* dev) {
    if (!dev || device_count >= BLOCK_MAX_DEVICES) {
        return 0;
    }
    dev->id = device_count;
    devices[device_count++] = dev;
    return 1;
}

block_device_t* block_get(uint32_t id) {
    if (id >= device_count) {
        return 0;
    }
    return devices[id];
}

uint32_t block_read(block_device_t* dev, uint32_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->read) {
        return 0;
    }
    return dev->read(dev, lba, count, buffer);
}

uint32_t block_write(block_device_t* dev, uint32_t lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !dev->write) {
        return 0;
    }
    return dev->write(dev, lba, count, buffer);
}
