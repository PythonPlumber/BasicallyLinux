#ifndef FIXEDPOINT_H
#define FIXEDPOINT_H

#include "types.h"

typedef int32_t q16_16_t;

static inline q16_16_t q16_from_int(int32_t value) {
    return (q16_16_t)(value << 16);
}

static inline int32_t q16_to_int(q16_16_t value) {
    return (int32_t)(value >> 16);
}

static inline q16_16_t q16_mul(q16_16_t a, q16_16_t b) {
    int64_t tmp = (int64_t)a * (int64_t)b;
    return (q16_16_t)(tmp >> 16);
}

static inline q16_16_t q16_add(q16_16_t a, q16_16_t b) {
    return a + b;
}

static inline q16_16_t q16_sub(q16_16_t a, q16_16_t b) {
    return a - b;
}

#endif
