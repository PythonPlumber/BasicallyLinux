#include "trace/perf_meter.h"
#include "types.h"

void perf_meter_init(perf_meter_t* meter) {
    if (!meter) {
        return;
    }
    meter->samples = 0;
    meter->cycles = 0;
    meter->max_cycles = 0;
    meter->min_cycles = 0;
}

void perf_meter_sample(perf_meter_t* meter, uint64_t cycles) {
    if (!meter) {
        return;
    }
    meter->samples++;
    meter->cycles += cycles;
    if (meter->max_cycles < cycles) {
        meter->max_cycles = cycles;
    }
    if (meter->min_cycles == 0 || cycles < meter->min_cycles) {
        meter->min_cycles = cycles;
    }
}

uint64_t perf_meter_avg(const perf_meter_t* meter) {
    if (!meter || meter->samples == 0) {
        return 0;
    }
    return meter->cycles / meter->samples;
}
