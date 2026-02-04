#include "kswapd.h"
#include "pmm.h"
#include "types.h"

#define KSWAPD_MAX_RECLAIMERS 8

static reclaimer_fn reclaimers[KSWAPD_MAX_RECLAIMERS];
static uint32_t reclaimer_count = 0;
static uint32_t high_watermark = 80;
static uint32_t low_watermark = 60;
static kswapd_state_t state;

void kswapd_init(void) {
    reclaimer_count = 0;
    high_watermark = 80;
    low_watermark = 60;
    state.high_watermark = high_watermark;
    state.low_watermark = low_watermark;
    state.reclaim_cycles = 0;
    state.reclaimed_pages = 0;
}

int kswapd_register_reclaimer(reclaimer_fn fn) {
    if (!fn || reclaimer_count >= KSWAPD_MAX_RECLAIMERS) {
        return 0;
    }
    reclaimers[reclaimer_count++] = fn;
    return 1;
}

void kswapd_tick(void) {
    uint32_t usage = mem_usage_pct();
    if (usage < high_watermark) {
        return;
    }
    uint32_t target = 4 + (usage - high_watermark);
    state.reclaim_cycles++;
    for (uint32_t i = 0; i < reclaimer_count; ++i) {
        uint32_t freed = reclaimers[i](target);
        if (freed > target) {
            freed = target;
        }
        state.reclaimed_pages += freed;
        if (freed >= target) {
            break;
        }
        target -= freed;
    }
    if (usage > low_watermark && target > 0) {
        for (uint32_t i = 0; i < reclaimer_count; ++i) {
            uint32_t freed = reclaimers[i](target);
            if (freed > target) {
                freed = target;
            }
            state.reclaimed_pages += freed;
            if (freed >= target) {
                break;
            }
            target -= freed;
        }
    }
}

void kswapd_set_watermarks(uint32_t low_pct, uint32_t high_pct) {
    if (low_pct > 95) {
        low_pct = 95;
    }
    if (high_pct > 99) {
        high_pct = 99;
    }
    if (low_pct > high_pct) {
        uint32_t tmp = low_pct;
        low_pct = high_pct;
        high_pct = tmp;
    }
    low_watermark = low_pct;
    high_watermark = high_pct;
    state.low_watermark = low_watermark;
    state.high_watermark = high_watermark;
}

kswapd_state_t kswapd_state(void) {
    return state;
}
