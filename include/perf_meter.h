#include "types.h"

typedef struct {
    uint64_t samples;
    uint64_t cycles;
    uint64_t max_cycles;
    uint64_t min_cycles;
} perf_meter_t;

void perf_meter_init(perf_meter_t* meter);
void perf_meter_sample(perf_meter_t* meter, uint64_t cycles);
uint64_t perf_meter_avg(const perf_meter_t* meter);
