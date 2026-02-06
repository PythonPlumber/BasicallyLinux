#include "selftest.h"
#include "ai/ai_model.h"
#include "diag.h"
#include "fixedpoint.h"
#include "ai/gguf.h"

// Tracks the most recent self-test failure count.
static uint32_t selftest_failures = 0;

static int selftest_check_int(const char* name, int32_t expected, int32_t actual) {
    if (expected == actual) {
        return 1;
    }
    diag_log(DIAG_ERROR, name);
    diag_log_hex32(DIAG_ERROR, "expected", (uint32_t)expected);
    diag_log_hex32(DIAG_ERROR, "actual", (uint32_t)actual);
    return 0;
}

static void selftest_fixedpoint(uint32_t* failures) {
    if (!selftest_check_int("q16_from_int", 3, q16_to_int(q16_from_int(3)))) {
        (*failures)++;
    }
    if (!selftest_check_int("q16_add", 7, q16_to_int(q16_add(q16_from_int(3), q16_from_int(4))))) {
        (*failures)++;
    }
    if (!selftest_check_int("q16_mul", 6, q16_to_int(q16_mul(q16_from_int(3), q16_from_int(2))))) {
        (*failures)++;
    }
}

static void selftest_quant(uint32_t* failures) {
    uint8_t block[20];
    for (uint32_t i = 0; i < sizeof(block); ++i) {
        block[i] = 0;
    }
    *(uint32_t*)block = 0x3F800000u;
    block[4] = 0x10;
    q16_16_t out[2];
    uint32_t read = ai_quant_read_q16(GGML_TYPE_Q4_0, block, out, 2);
    if (read != 2) {
        diag_log(DIAG_ERROR, "q4_0 read count");
        (*failures)++;
        return;
    }
    if (!selftest_check_int("q4_0 val0", -8, q16_to_int(out[0]))) {
        (*failures)++;
    }
    if (!selftest_check_int("q4_0 val1", -7, q16_to_int(out[1]))) {
        (*failures)++;
    }
}

static void selftest_gguf(uint32_t* failures) {
    gguf_header_t header;
    if (!ai_model_read_header(&header)) {
        diag_log(DIAG_WARN, "gguf header not available");
        return;
    }
    if (header.tensor_count == 0) {
        diag_log(DIAG_WARN, "gguf tensor count is zero");
    }
    uint64_t data_start = 0;
    if (!ai_tensor_data_start(&data_start)) {
        diag_log(DIAG_ERROR, "gguf data start failed");
        (*failures)++;
    }
}

uint32_t selftest_run(void) {
    uint32_t failures = 0;
    diag_log(DIAG_INFO, "selftest start");
    selftest_fixedpoint(&failures);
    selftest_quant(&failures);
    selftest_gguf(&failures);
    if (failures == 0) {
        diag_log(DIAG_INFO, "selftest ok");
    } else {
        diag_log_hex32(DIAG_ERROR, "selftest failures", failures);
    }
    selftest_failures = failures;
    return failures;
}

uint32_t selftest_last_failures(void) {
    return selftest_failures;
}
