#ifndef KSWAPD_H
#define KSWAPD_H

#include "types.h"

typedef uint32_t (*reclaimer_fn)(uint32_t target_pages);

typedef struct {
    uint32_t high_watermark;
    uint32_t low_watermark;
    uint32_t reclaim_cycles;
    uint32_t reclaimed_pages;
} kswapd_state_t;

void kswapd_init(void);
void kswapd_tick(void);
int kswapd_register_reclaimer(reclaimer_fn fn);
void kswapd_set_watermarks(uint32_t low_pct, uint32_t high_pct);
kswapd_state_t kswapd_state(void);

#endif
