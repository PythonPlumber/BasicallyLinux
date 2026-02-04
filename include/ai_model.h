#include "fixedpoint.h"
#include "gguf.h"
#include "types.h"

#define AI_MODEL_VIRT_BASE 0xE0000000u

enum {
    GGML_TYPE_F32 = 0,
    GGML_TYPE_F16 = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q8_1 = 9,
    GGML_TYPE_Q2_K = 10,
    GGML_TYPE_Q3_K = 11,
    GGML_TYPE_Q4_K = 12,
    GGML_TYPE_Q5_K = 13,
    GGML_TYPE_Q6_K = 14,
    GGML_TYPE_Q8_K = 15,
    GGML_TYPE_I8 = 16,
    GGML_TYPE_I16 = 17,
    GGML_TYPE_I32 = 18,
    GGML_TYPE_I64 = 19,
    GGML_TYPE_F64 = 20
};

typedef struct {
    uint64_t mem_total;
    uint64_t mem_used;
    uint64_t heap_total;
    uint64_t heap_free;
    uint64_t uptime_ticks;
    
    // Inference Context
    uint32_t n_threads;
    uint32_t context_size;
    uint32_t n_past;
    q16_16_t* kv_cache_k;
    q16_16_t* kv_cache_v;
    q16_16_t* scratch_buffer;
} ai_context_t;

typedef struct ai_job_s {
    struct ai_job_s* next;
    void (*func)(void* arg);
    void* arg;
    uint32_t priority;
    volatile int completed;
} ai_job_t;

void ai_scheduler_init(uint32_t n_workers);
void ai_scheduler_submit(ai_job_t* job);
void ai_scheduler_wait(ai_job_t* job);

typedef struct {
    char name[64];
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset;
    uint64_t data_offset;
    uint64_t data_bytes;
} ai_tensor_desc_t;

typedef struct {
    uint32_t block_elems;
    uint32_t block_bytes;
} ai_quant_info_t;

void ai_model_init(void);
const void* ai_model_data(void);
uint32_t ai_model_size(void);
int ai_model_read_header(gguf_header_t* out);
int ai_tensor_get(uint32_t index, ai_tensor_desc_t* out);
int ai_tensor_data_start(uint64_t* out);
const char* ai_tensor_type_name(uint32_t type);
int ai_quant_info(uint32_t type, ai_quant_info_t* out);
uint32_t ai_quant_read_q16(uint32_t type, const uint8_t* src, q16_16_t* dst, uint32_t max);
void ai_set_simd_enabled(int enabled);
int ai_matmul_q16_16(const q16_16_t* a, const q16_16_t* b, q16_16_t* c, uint32_t m, uint32_t n, uint32_t k);
int gguf_parse(gguf_header_t* out);
int gguf_get_tensor(uint32_t index, ai_tensor_desc_t* out);
int matmul(const q16_16_t* a, const q16_16_t* b, q16_16_t* c, uint32_t m, uint32_t n, uint32_t k);
q16_16_t dot_product(const q16_16_t* a, const q16_16_t* b, uint32_t n);
void rmsnorm(q16_16_t* data, uint32_t n);
void softmax(q16_16_t* data, uint32_t n);
void rope_apply(q16_16_t* data, uint32_t n);
uint32_t tokenizer_encode(const char* text, uint32_t* out, uint32_t max);
uint32_t tokenizer_decode(uint32_t token, char* out, uint32_t max);
void ai_context_init(ai_context_t* context);
void ai_stream_token(uint32_t token);
int kv_cache_init(uint32_t tokens);
uint32_t sample_top_p(const q16_16_t* probs, uint32_t count, q16_16_t p);
int transformer_step(const uint32_t* input, uint32_t input_len, uint32_t* output_token);
int ai_load_vocab(void);
void ai_panic_handler(const char* msg);
float fixed_to_float(q16_16_t value);
q16_16_t activation_silu(q16_16_t x);
void ai_set_temp(q16_16_t temp);
void get_system_context(ai_context_t* context);
void mmap_ai_weights(void);

void ai_stream_putc(char ch);
void ai_stream_write(const char* text);
void ai_explain_page_fault(uint32_t err_code);
void ai_ask(const char* prompt, const ai_context_t* context);
void ai_infer(const char* prompt, const ai_context_t* context);
uint32_t ai_debug_last_tokens(char* out, uint32_t max);
