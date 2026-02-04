#ifndef SECURE_HARD_H
#define SECURE_HARD_H

#include "types.h"

void secure_hard_init(void);
uint32_t secure_hard_kaslr_seed(void);
uint32_t secure_hard_canary(void);
void secure_hard_set_crypto(uint32_t accel_mask);
uint32_t secure_hard_crypto_mask(void);

#define SECURE_CRYPTO_AES 1u
#define SECURE_CRYPTO_SHA 2u
#define SECURE_CRYPTO_RSA 4u
#define SECURE_CRYPTO_CHACHA 8u

#endif
