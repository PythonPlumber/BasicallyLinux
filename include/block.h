#include "types.h"

typedef struct block_device block_device_t;

typedef uint32_t (*block_read_t)(block_device_t* dev, uint32_t lba, uint32_t count, uint8_t* buffer);
typedef uint32_t (*block_write_t)(block_device_t* dev, uint32_t lba, uint32_t count, const uint8_t* buffer);

struct block_device {
    uint32_t id;
    uint32_t block_size;
    uint32_t block_count;
    block_read_t read;
    block_write_t write;
    void* impl;
};

int block_register(block_device_t* dev);
block_device_t* block_get(uint32_t id);
uint32_t block_read(block_device_t* dev, uint32_t lba, uint32_t count, uint8_t* buffer);
uint32_t block_write(block_device_t* dev, uint32_t lba, uint32_t count, const uint8_t* buffer);
