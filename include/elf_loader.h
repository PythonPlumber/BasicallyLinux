#include "types.h"

typedef struct {
    uint32_t entry;
    uint32_t image_base;
} elf_image_t;

int elf_loader_parse(const uint8_t* data, uint32_t size, elf_image_t* out);
