#include "secure_hard.h"
#include "types.h"

static uint32_t kaslr_seed = 0;
static uint32_t canary_value = 0;
static uint32_t crypto_mask = 0;

void secure_hard_init(void) {
    kaslr_seed = 0xA5A55A5Au;
    canary_value = 0xC0FFEEu;
    crypto_mask = 0;
}

uint32_t secure_hard_kaslr_seed(void) {
    return kaslr_seed;
}

uint32_t secure_hard_canary(void) {
    return canary_value;
}

void secure_hard_set_crypto(uint32_t accel_mask) {
    crypto_mask = accel_mask;
}

uint32_t secure_hard_crypto_mask(void) {
    return crypto_mask;
}
