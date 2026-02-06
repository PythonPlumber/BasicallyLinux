#include "bench.h"
#include "ai/ai_model.h"
#include "diag.h"
#include "fixedpoint.h"
#include "mem/heap.h"
#include "arch/x86/timer.h"

// Stores the most recent benchmark duration in ticks.
static uint64_t bench_ticks_last = 0;

uint64_t bench_matmul(uint32_t size) {
    if (size < 2) {
        size = 2;
    }
    if (size > 64) {
        size = 64;
    }
    uint32_t m = size;
    uint32_t n = size;
    uint32_t k = size;
    uint32_t a_count = m * k;
    uint32_t b_count = k * n;
    uint32_t c_count = m * n;
    q16_16_t* a = (q16_16_t*)kmalloc(a_count * sizeof(q16_16_t));
    q16_16_t* b = (q16_16_t*)kmalloc(b_count * sizeof(q16_16_t));
    q16_16_t* c = (q16_16_t*)kmalloc(c_count * sizeof(q16_16_t));
    if (!a || !b || !c) {
        diag_log(DIAG_ERROR, "bench alloc failed");
        if (a) {
            kfree(a);
        }
        if (b) {
            kfree(b);
        }
        if (c) {
            kfree(c);
        }
        return 0;
    }
    for (uint32_t i = 0; i < a_count; ++i) {
        a[i] = q16_from_int((int32_t)((i & 3u) - 1));
    }
    for (uint32_t i = 0; i < b_count; ++i) {
        b[i] = q16_from_int((int32_t)((i & 3u) - 1));
    }
    uint64_t start = timer_get_ticks();
    ai_matmul_q16_16(a, b, c, m, n, k);
    uint64_t end = timer_get_ticks();
    kfree(a);
    kfree(b);
    kfree(c);
    bench_ticks_last = end - start;
    return bench_ticks_last;
}

uint64_t bench_last_ticks(void) {
    return bench_ticks_last;
}
