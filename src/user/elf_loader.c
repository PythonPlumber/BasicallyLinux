#include "elf_loader.h"
#include "types.h"

int elf_loader_parse(const uint8_t* data, uint32_t size, elf_image_t* out) {
    if (!data || size < 4 || !out) {
        return 0;
    }
    if (data[0] != 0x7F || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
        return 0;
    }
    out->entry = 0;
    out->image_base = 0;
    return 1;
}
