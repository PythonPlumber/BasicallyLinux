#include "drivers/gfx_forge.h"
#include "types.h"

static uint32_t mode_width = 0;
static uint32_t mode_height = 0;

void gfx_forge_init(void) {
    mode_width = 0;
    mode_height = 0;
}

void gfx_forge_modeset(uint32_t width, uint32_t height) {
    mode_width = width;
    mode_height = height;
}
