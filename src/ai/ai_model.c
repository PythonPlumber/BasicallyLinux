#include "ai/ai_model.h"
#include "debug.h"
#include "diag.h"
#include "fixedpoint.h"
#include "ai/gguf.h"
#include "mem/heap.h"
#include "paging.h"
#include "mem/pmm.h"
#include "drivers/serial.h"
#include "arch/x86/timer.h"
#include "types.h"
#include "util.h"
#include "drivers/vga.h"
#include "arch/x86/smp_rally.h"
#include "kernel/sched.h"

#ifndef NO_AI_MODEL
extern uint8_t _ai_model_start;
extern uint8_t _ai_model_end;
#else
static uint8_t _ai_model_start = 0;
static uint8_t _ai_model_end = 0;
#endif

static uint8_t* ai_mapped_base = 0;
static uint32_t ai_mapped_size = 0;

// Neural Dispatch Scheduler
static ai_job_t* job_queue_head = 0;
static ai_job_t* job_queue_tail = 0;
static int ai_scheduler_running = 0;
static uint32_t ai_worker_count = 0;
// Spinlock for queue
static spinlock_t job_queue_lock = 0;

void ai_scheduler_submit(ai_job_t* job) {
    if (!job) return;
    job->completed = 0;
    job->next = 0;
    
    uint32_t flags = spin_lock_irqsave(&job_queue_lock);
    if (job_queue_tail) {
        job_queue_tail->next = job;
        job_queue_tail = job;
    } else {
        job_queue_head = job;
        job_queue_tail = job;
    }
    spin_unlock_irqrestore(&job_queue_lock, flags);
}

void ai_scheduler_wait(ai_job_t* job) {
    while (!job->completed) {
        // If we are waiting, we might as well do some work if available
        // Simple "help yourself" strategy
        asm volatile("pause");
    }
}

static void ai_worker_loop(void* arg) {
    (void)arg;
    while (ai_scheduler_running) {
        ai_job_t* job = 0;
        
        uint32_t flags = spin_lock_irqsave(&job_queue_lock);
        if (job_queue_head) {
            job = job_queue_head;
            job_queue_head = job->next;
            if (!job_queue_head) {
                job_queue_tail = 0;
            }
        }
        spin_unlock_irqrestore(&job_queue_lock, flags);
        
        if (job) {
            if (job->func) {
                job->func(job->arg);
            }
            job->completed = 1;
        } else {
            asm volatile("pause");
        }
    }
}

void ai_scheduler_init(uint32_t n_workers) {
    ai_scheduler_running = 1;
    ai_worker_count = n_workers;
    vga_set_color(0x0A);
    diag_log(DIAG_INFO, "AI Neural Scheduler: Initializing workers...");
    
    // In a real implementation, we would spawn kernel threads here
    // For now, we assume this is called during SMP init
    // for (uint32_t i = 0; i < n_workers; ++i) {
    //     thread_create(ai_worker_loop, 0);
    // }
}

// Moved matmul_worker to after ai_dot_q16_simd

static char token_history[10];
static uint32_t token_count = 0;
static uint32_t token_head = 0;
static int simd_enabled = 0;
static q16_16_t ai_temp = 1 << 16;
static uint32_t kv_cache_tokens = 0;

static void vga_put(char ch) {
    vga_putc(ch);
}

void ai_stream_putc(char ch) {
    vga_put(ch);
    token_history[token_head] = ch;
    token_head = (token_head + 1) % 10;
    if (token_count < 10) {
        token_count++;
    }
}

void ai_stream_write(const char* text) {
    for (uint32_t i = 0; text[i] != 0; ++i) {
        vga_put(text[i]);
        token_history[token_head] = text[i];
        token_head = (token_head + 1) % 10;
        if (token_count < 10) {
            token_count++;
        }
    }
}

static void ai_stream_write_uint64(uint64_t value) {
    char buf[24];
    int pos = 0;
    if (value == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[20];
        int tmp_pos = 0;
        while (value > 0 && tmp_pos < 20) {
            tmp[tmp_pos++] = (char)('0' + (value % 10));
            value /= 10;
        }
        for (int i = tmp_pos - 1; i >= 0; --i) {
            buf[pos++] = tmp[i];
        }
    }
    buf[pos] = 0;
    ai_stream_write(buf);
}

const void* ai_model_data(void) {
    return ai_mapped_base;
}

uint32_t ai_model_size(void) {
    return ai_mapped_size;
}

int ai_model_read_header(gguf_header_t* out) {
    if (!ai_mapped_base) {
        return 0;
    }
    return gguf_read_header(ai_mapped_base, out);
}

static uint32_t ggml_type_size(uint32_t type) {
    switch (type) {
        case GGML_TYPE_F32:
            return 4;
        case GGML_TYPE_F16:
            return 2;
        case GGML_TYPE_I8:
            return 1;
        case GGML_TYPE_I16:
            return 2;
        case GGML_TYPE_I32:
            return 4;
        case GGML_TYPE_I64:
            return 8;
        case GGML_TYPE_F64:
            return 8;
        default:
            return 0;
    }
}

const char* ai_tensor_type_name(uint32_t type) {
    switch (type) {
        case GGML_TYPE_F32:
            return "f32";
        case GGML_TYPE_F16:
            return "f16";
        case GGML_TYPE_Q4_0:
            return "q4_0";
        case GGML_TYPE_Q4_1:
            return "q4_1";
        case GGML_TYPE_Q5_0:
            return "q5_0";
        case GGML_TYPE_Q5_1:
            return "q5_1";
        case GGML_TYPE_Q8_0:
            return "q8_0";
        case GGML_TYPE_Q8_1:
            return "q8_1";
        case GGML_TYPE_Q2_K:
            return "q2_k";
        case GGML_TYPE_Q3_K:
            return "q3_k";
        case GGML_TYPE_Q4_K:
            return "q4_k";
        case GGML_TYPE_Q5_K:
            return "q5_k";
        case GGML_TYPE_Q6_K:
            return "q6_k";
        case GGML_TYPE_Q8_K:
            return "q8_k";
        case GGML_TYPE_I8:
            return "i8";
        case GGML_TYPE_I16:
            return "i16";
        case GGML_TYPE_I32:
            return "i32";
        case GGML_TYPE_I64:
            return "i64";
        case GGML_TYPE_F64:
            return "f64";
        default:
            return "unknown";
    }
}

int ai_quant_info(uint32_t type, ai_quant_info_t* out) {
    if (!out) {
        return 0;
    }
    switch (type) {
        case GGML_TYPE_Q4_0:
            out->block_elems = 32;
            out->block_bytes = 20;
            return 1;
        case GGML_TYPE_Q4_1:
            out->block_elems = 32;
            out->block_bytes = 24;
            return 1;
        case GGML_TYPE_Q5_0:
            out->block_elems = 32;
            out->block_bytes = 24;
            return 1;
        case GGML_TYPE_Q5_1:
            out->block_elems = 32;
            out->block_bytes = 28;
            return 1;
        case GGML_TYPE_Q8_0:
            out->block_elems = 32;
            out->block_bytes = 36;
            return 1;
        case GGML_TYPE_Q8_1:
            out->block_elems = 32;
            out->block_bytes = 40;
            return 1;
        case GGML_TYPE_Q4_K:
            out->block_elems = 256;
            out->block_bytes = 144;
            return 1;
        default:
            out->block_elems = 0;
            out->block_bytes = 0;
            return 0;
    }
}

static q16_16_t q16_from_f32_bits(uint32_t bits) {
    uint32_t sign = bits >> 31;
    int32_t exp = (int32_t)((bits >> 23) & 0xFF);
    uint32_t frac = bits & 0x7FFFFF;
    if (exp == 0 || exp == 255) {
        return 0;
    }
    exp -= 127;
    uint32_t mant = frac | 0x800000;
    int32_t shift = exp - 7;
    int64_t value;
    if (shift >= 0) {
        value = (int64_t)mant << shift;
    } else {
        value = (int64_t)mant >> (-shift);
    }
    if (sign) {
        value = -value;
    }
    return (q16_16_t)value;
}

static q16_16_t q16_from_f16_bits(uint16_t bits) {
    uint32_t sign = (bits >> 15) & 1u;
    int32_t exp = (int32_t)((bits >> 10) & 0x1F);
    uint32_t frac = bits & 0x3FF;
    if (exp == 0) {
        return 0;
    }
    exp -= 15;
    uint32_t mant = frac | 0x400;
    int32_t shift = exp - 9;
    int64_t value;
    if (shift >= 0) {
        value = (int64_t)mant << shift;
    } else {
        value = (int64_t)mant >> (-shift);
    }
    if (sign) {
        value = -value;
    }
    return (q16_16_t)value;
}

static uint32_t read_bits_6(const uint8_t* data, uint32_t index) {
    uint32_t bit = index * 6;
    uint32_t byte = bit >> 3;
    uint32_t shift = bit & 7;
    uint32_t v = (uint32_t)data[byte] | ((uint32_t)data[byte + 1] << 8) | ((uint32_t)data[byte + 2] << 16);
    return (v >> shift) & 0x3F;
}

static int gguf_read_tensor_desc(gguf_reader_t* r, ai_tensor_desc_t* out, uint64_t* elem_count) {
    const uint8_t* name_ptr;
    uint64_t name_len;
    uint32_t n_dims;
    uint32_t ttype;
    uint64_t toffset;
    if (!gguf_read_string(r, &name_ptr, &name_len)) {
        return 0;
    }
    if (!gguf_read_u32(r, &n_dims)) {
        return 0;
    }
    uint64_t count = 1;
    for (uint32_t d = 0; d < n_dims; ++d) {
        uint64_t dim;
        if (!gguf_read_u64(r, &dim)) {
            return 0;
        }
        if (out && d < 4) {
            out->dims[d] = dim;
        }
        count = count * dim;
    }
    if (!gguf_read_u32(r, &ttype)) {
        return 0;
    }
    if (!gguf_read_u64(r, &toffset)) {
        return 0;
    }
    if (out) {
        uint32_t n = (name_len > 63) ? 63 : (uint32_t)name_len;
        for (uint32_t i = 0; i < n; ++i) {
            out->name[i] = (char)name_ptr[i];
        }
        out->name[n] = 0;
        out->n_dims = n_dims;
        out->type = ttype;
        out->offset = toffset;
        for (uint32_t i = n_dims; i < 4; ++i) {
            out->dims[i] = 1;
        }
    }
    if (elem_count) {
        *elem_count = count;
    }
    return 1;
}

int ai_tensor_data_start(uint64_t* out) {
    if (!out || !ai_mapped_base) {
        return 0;
    }
    gguf_header_t header;
    if (!gguf_read_header(ai_mapped_base, &header)) {
        return 0;
    }
    gguf_reader_t r;
    if (!gguf_reader_init(&r, (const uint8_t*)ai_mapped_base, ai_mapped_size)) {
        return 0;
    }
    if (!gguf_reader_seek(&r, sizeof(gguf_header_t))) {
        return 0;
    }
    if (!gguf_skip_kv_pairs(&r, header.kv_count)) {
        return 0;
    }
    for (uint64_t i = 0; i < header.tensor_count; ++i) {
        if (!gguf_read_tensor_desc(&r, 0, 0)) {
            return 0;
        }
    }
    *out = (uint64_t)(uint32_t)ai_mapped_base + r.offset;
    return 1;
}

int ai_tensor_get(uint32_t index, ai_tensor_desc_t* out) {
    if (!out || !ai_mapped_base) {
        return 0;
    }
    gguf_header_t header;
    if (!gguf_read_header(ai_mapped_base, &header)) {
        return 0;
    }
    if (index >= header.tensor_count) {
        return 0;
    }
    gguf_reader_t r;
    if (!gguf_reader_init(&r, (const uint8_t*)ai_mapped_base, ai_mapped_size)) {
        return 0;
    }
    if (!gguf_reader_seek(&r, sizeof(gguf_header_t))) {
        return 0;
    }
    if (!gguf_skip_kv_pairs(&r, header.kv_count)) {
        return 0;
    }
    uint64_t elem_count = 0;
    ai_tensor_desc_t temp;
    int found = 0;
    for (uint64_t i = 0; i < header.tensor_count; ++i) {
        ai_tensor_desc_t desc;
        uint64_t count = 0;
        if (!gguf_read_tensor_desc(&r, &desc, &count)) {
            return 0;
        }
        if (i == index) {
            temp = desc;
            elem_count = count;
            found = 1;
        }
    }
    if (!found) {
        return 0;
    }
    uint64_t data_start = r.offset;
    uint32_t type_size = ggml_type_size(temp.type);
    if (type_size) {
        temp.data_bytes = elem_count * (uint64_t)type_size;
    } else {
        ai_quant_info_t info;
        if (ai_quant_info(temp.type, &info) && info.block_elems) {
            uint64_t blocks = (elem_count + info.block_elems - 1) / info.block_elems;
            temp.data_bytes = blocks * info.block_bytes;
        } else {
            temp.data_bytes = 0;
        }
    }
    uint64_t offset_bytes = data_start + temp.offset;
    if (offset_bytes + temp.data_bytes > ai_mapped_size) {
        diag_log(DIAG_ERROR, "tensor bounds invalid");
        return 0;
    }
    temp.data_offset = (uint64_t)(uint32_t)ai_mapped_base + offset_bytes;
    *out = temp;
    return 1;
}

uint32_t ai_quant_read_q16(uint32_t type, const uint8_t* src, q16_16_t* dst, uint32_t max) {
    if (!src || !dst || max == 0) {
        return 0;
    }
    if (type == GGML_TYPE_F16) {
        uint16_t h = *(const uint16_t*)src;
        dst[0] = q16_from_f16_bits(h);
        return 1;
    }
    if (type == GGML_TYPE_F32) {
        uint32_t f = *(const uint32_t*)src;
        dst[0] = q16_from_f32_bits(f);
        return 1;
    }
    if (type == GGML_TYPE_Q8_0) {
        q16_16_t scale = q16_from_f32_bits(*(const uint32_t*)src);
        const int8_t* qs = (const int8_t*)(src + 4);
        uint32_t count = max < 32 ? max : 32;
        for (uint32_t i = 0; i < count; ++i) {
            dst[i] = q16_mul(scale, q16_from_int(qs[i]));
        }
        return count;
    }
    if (type == GGML_TYPE_Q4_0) {
        q16_16_t scale = q16_from_f32_bits(*(const uint32_t*)src);
        const uint8_t* qs = src + 4;
        uint32_t count = max < 32 ? max : 32;
        for (uint32_t i = 0; i < count / 2; ++i) {
            uint8_t byte = qs[i];
            int32_t lo = (int32_t)(byte & 0x0F) - 8;
            int32_t hi = (int32_t)(byte >> 4) - 8;
            uint32_t idx = i * 2;
            if (idx < count) {
                dst[idx] = q16_mul(scale, q16_from_int(lo));
            }
            if (idx + 1 < count) {
                dst[idx + 1] = q16_mul(scale, q16_from_int(hi));
            }
        }
        return count;
    }
    if (type == GGML_TYPE_Q4_1) {
        q16_16_t scale = q16_from_f16_bits(*(const uint16_t*)src);
        q16_16_t min = q16_from_f16_bits(*(const uint16_t*)(src + 2));
        const uint8_t* qs = src + 4;
        uint32_t count = max < 32 ? max : 32;
        for (uint32_t i = 0; i < count / 2; ++i) {
            uint8_t byte = qs[i];
            int32_t lo = (int32_t)(byte & 0x0F);
            int32_t hi = (int32_t)(byte >> 4);
            uint32_t idx = i * 2;
            if (idx < count) {
                dst[idx] = q16_add(q16_mul(scale, q16_from_int(lo)), min);
            }
            if (idx + 1 < count) {
                dst[idx + 1] = q16_add(q16_mul(scale, q16_from_int(hi)), min);
            }
        }
        return count;
    }
    if (type == GGML_TYPE_Q5_0) {
        q16_16_t scale = q16_from_f16_bits(*(const uint16_t*)src);
        const uint8_t* qh = src + 2;
        const uint8_t* qs = src + 6;
        uint32_t count = max < 32 ? max : 32;
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t low = (i & 1u) ? (qs[i >> 1] >> 4) : (qs[i >> 1] & 0x0F);
            uint8_t high = (qh[i >> 3] >> (i & 7u)) & 1u;
            int32_t q = (int32_t)(low | (high << 4)) - 16;
            dst[i] = q16_mul(scale, q16_from_int(q));
        }
        return count;
    }
    if (type == GGML_TYPE_Q5_1) {
        q16_16_t scale = q16_from_f16_bits(*(const uint16_t*)src);
        q16_16_t min = q16_from_f16_bits(*(const uint16_t*)(src + 2));
        const uint8_t* qh = src + 4;
        const uint8_t* qs = src + 8;
        uint32_t count = max < 32 ? max : 32;
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t low = (i & 1u) ? (qs[i >> 1] >> 4) : (qs[i >> 1] & 0x0F);
            uint8_t high = (qh[i >> 3] >> (i & 7u)) & 1u;
            uint32_t q = (uint32_t)(low | (high << 4));
            dst[i] = q16_add(q16_mul(scale, q16_from_int((int32_t)q)), min);
        }
        return count;
    }
    if (type == GGML_TYPE_Q4_K) {
        uint16_t d_bits = *(const uint16_t*)src;
        uint16_t m_bits = *(const uint16_t*)(src + 2);
        q16_16_t d = q16_from_f16_bits(d_bits);
        q16_16_t m = q16_from_f16_bits(m_bits);
        const uint8_t* scales = src + 4;
        const uint8_t* qs = src + 16;
        uint32_t count = max < 256 ? max : 256;
        uint32_t out_idx = 0;
        uint8_t sub_scale[8];
        uint8_t sub_min[8];
        for (uint32_t i = 0; i < 8; ++i) {
            sub_scale[i] = (uint8_t)read_bits_6(scales, i);
        }
        for (uint32_t i = 0; i < 8; ++i) {
            sub_min[i] = (uint8_t)read_bits_6(scales, i + 8);
        }
        uint32_t q_idx = 0;
        for (uint32_t sb = 0; sb < 8 && out_idx < count; ++sb) {
            q16_16_t sb_scale = q16_mul(d, q16_from_int((int32_t)sub_scale[sb]));
            q16_16_t sb_min = q16_mul(m, q16_from_int((int32_t)sub_min[sb]));
            for (uint32_t i = 0; i < 16 && out_idx < count; ++i) {
                uint8_t byte = qs[q_idx++];
                int32_t lo = (int32_t)(byte & 0x0F);
                int32_t hi = (int32_t)(byte >> 4);
                dst[out_idx++] = q16_add(q16_mul(sb_scale, q16_from_int(lo)), sb_min);
                if (out_idx < count) {
                    dst[out_idx++] = q16_add(q16_mul(sb_scale, q16_from_int(hi)), sb_min);
                }
            }
        }
        return count;
    }
    if (type == GGML_TYPE_Q8_1) {
        q16_16_t scale = q16_from_f16_bits(*(const uint16_t*)src);
        const int8_t* qs = (const int8_t*)(src + 4);
        uint32_t count = max < 32 ? max : 32;
        for (uint32_t i = 0; i < count; ++i) {
            dst[i] = q16_mul(scale, q16_from_int(qs[i]));
        }
        return count;
    }
    return 0;
}

void ai_set_simd_enabled(int enabled) {
    simd_enabled = enabled ? 1 : 0;
}

static q16_16_t ai_dot_q16_simd(const q16_16_t* a, const q16_16_t* b, uint32_t n, uint32_t k, uint32_t col) {
    q16_16_t sum = 0;
    uint32_t p = 0;
    int32_t temp[4];
    int32_t bpack[4];
    for (; p + 4 <= k; p += 4) {
        bpack[0] = b[(p + 0) * n + col];
        bpack[1] = b[(p + 1) * n + col];
        bpack[2] = b[(p + 2) * n + col];
        bpack[3] = b[(p + 3) * n + col];
        asm volatile(
            "movdqu (%1), %%xmm0\n"
            "movdqu (%2), %%xmm1\n"
            "pmulld %%xmm1, %%xmm0\n"
            "psrad $16, %%xmm0\n"
            "movdqu %%xmm0, (%0)\n"
            :
            : "r"(temp), "r"(a + p), "r"(bpack)
            : "xmm0", "xmm1", "memory"
        );
        sum = q16_add(sum, (q16_16_t)temp[0]);
        sum = q16_add(sum, (q16_16_t)temp[1]);
        sum = q16_add(sum, (q16_16_t)temp[2]);
        sum = q16_add(sum, (q16_16_t)temp[3]);
    }
    for (; p < k; ++p) {
        sum = q16_add(sum, q16_mul(a[p], b[p * n + col]));
    }
    return sum;
}

typedef struct {
    const q16_16_t* a;
    const q16_16_t* b;
    q16_16_t* c;
    uint32_t m_start;
    uint32_t m_end;
    uint32_t n;
    uint32_t k;
} matmul_task_args_t;

static void matmul_worker(void* arg) {
    matmul_task_args_t* args = (matmul_task_args_t*)arg;
    for (uint32_t i = args->m_start; i < args->m_end; ++i) {
        const q16_16_t* arow = args->a + i * args->k;
        for (uint32_t j = 0; j < args->n; ++j) {
            args->c[i * args->n + j] = ai_dot_q16_simd(arow, args->b, args->n, args->k, j);
        }
    }
}

int ai_matmul_q16_16(const q16_16_t* a, const q16_16_t* b, q16_16_t* c, uint32_t m, uint32_t n, uint32_t k) {
    if (!a || !b || !c || m == 0 || n == 0 || k == 0) {
        return 0;
    }

    // If SMP is enabled and matrix is large enough, use scheduler
    if (ai_scheduler_running && m >= 8 && ai_worker_count > 0) {
        // Split into chunks based on worker count
        // For simplicity, launch 4 jobs if m is large enough
        uint32_t chunks = 4;
        if (m < 16) chunks = 2;
        
        uint32_t rows_per_chunk = m / chunks;
        ai_job_t jobs[4];
        matmul_task_args_t args[4];
        
        for (uint32_t i = 0; i < chunks; ++i) {
            args[i].a = a;
            args[i].b = b;
            args[i].c = c;
            args[i].m_start = i * rows_per_chunk;
            args[i].m_end = (i == chunks - 1) ? m : (i + 1) * rows_per_chunk;
            args[i].n = n;
            args[i].k = k;
            
            jobs[i].func = matmul_worker;
            jobs[i].arg = &args[i];
            
            ai_scheduler_submit(&jobs[i]);
        }
        
        // Wait for all jobs
        for (uint32_t i = 0; i < chunks; ++i) {
            ai_scheduler_wait(&jobs[i]);
        }
        return 2; // SMP executed
    }

    if (simd_enabled && k >= 4) {
        for (uint32_t i = 0; i < m; ++i) {
            const q16_16_t* arow = a + i * k;
            for (uint32_t j = 0; j < n; ++j) {
                c[i * n + j] = ai_dot_q16_simd(arow, b, n, k, j);
            }
        }
        return 2;
    }
    uint32_t i = 0;
    for (; i + 3 < m; i += 4) {
        uint32_t j = 0;
        for (; j + 3 < n; j += 4) {
            q16_16_t acc00 = 0, acc01 = 0, acc02 = 0, acc03 = 0;
            q16_16_t acc10 = 0, acc11 = 0, acc12 = 0, acc13 = 0;
            q16_16_t acc20 = 0, acc21 = 0, acc22 = 0, acc23 = 0;
            q16_16_t acc30 = 0, acc31 = 0, acc32 = 0, acc33 = 0;
            for (uint32_t p = 0; p < k; ++p) {
                q16_16_t a0 = a[(i + 0) * k + p];
                q16_16_t a1 = a[(i + 1) * k + p];
                q16_16_t a2 = a[(i + 2) * k + p];
                q16_16_t a3 = a[(i + 3) * k + p];
                q16_16_t b0 = b[p * n + (j + 0)];
                q16_16_t b1 = b[p * n + (j + 1)];
                q16_16_t b2 = b[p * n + (j + 2)];
                q16_16_t b3 = b[p * n + (j + 3)];
                acc00 = q16_add(acc00, q16_mul(a0, b0));
                acc01 = q16_add(acc01, q16_mul(a0, b1));
                acc02 = q16_add(acc02, q16_mul(a0, b2));
                acc03 = q16_add(acc03, q16_mul(a0, b3));
                acc10 = q16_add(acc10, q16_mul(a1, b0));
                acc11 = q16_add(acc11, q16_mul(a1, b1));
                acc12 = q16_add(acc12, q16_mul(a1, b2));
                acc13 = q16_add(acc13, q16_mul(a1, b3));
                acc20 = q16_add(acc20, q16_mul(a2, b0));
                acc21 = q16_add(acc21, q16_mul(a2, b1));
                acc22 = q16_add(acc22, q16_mul(a2, b2));
                acc23 = q16_add(acc23, q16_mul(a2, b3));
                acc30 = q16_add(acc30, q16_mul(a3, b0));
                acc31 = q16_add(acc31, q16_mul(a3, b1));
                acc32 = q16_add(acc32, q16_mul(a3, b2));
                acc33 = q16_add(acc33, q16_mul(a3, b3));
            }
            c[(i + 0) * n + (j + 0)] = acc00;
            c[(i + 0) * n + (j + 1)] = acc01;
            c[(i + 0) * n + (j + 2)] = acc02;
            c[(i + 0) * n + (j + 3)] = acc03;
            c[(i + 1) * n + (j + 0)] = acc10;
            c[(i + 1) * n + (j + 1)] = acc11;
            c[(i + 1) * n + (j + 2)] = acc12;
            c[(i + 1) * n + (j + 3)] = acc13;
            c[(i + 2) * n + (j + 0)] = acc20;
            c[(i + 2) * n + (j + 1)] = acc21;
            c[(i + 2) * n + (j + 2)] = acc22;
            c[(i + 2) * n + (j + 3)] = acc23;
            c[(i + 3) * n + (j + 0)] = acc30;
            c[(i + 3) * n + (j + 1)] = acc31;
            c[(i + 3) * n + (j + 2)] = acc32;
            c[(i + 3) * n + (j + 3)] = acc33;
        }
        for (; j < n; ++j) {
            q16_16_t acc0 = 0;
            q16_16_t acc1 = 0;
            q16_16_t acc2 = 0;
            q16_16_t acc3 = 0;
            for (uint32_t p = 0; p < k; ++p) {
                q16_16_t bp = b[p * n + j];
                acc0 = q16_add(acc0, q16_mul(a[(i + 0) * k + p], bp));
                acc1 = q16_add(acc1, q16_mul(a[(i + 1) * k + p], bp));
                acc2 = q16_add(acc2, q16_mul(a[(i + 2) * k + p], bp));
                acc3 = q16_add(acc3, q16_mul(a[(i + 3) * k + p], bp));
            }
            c[(i + 0) * n + j] = acc0;
            c[(i + 1) * n + j] = acc1;
            c[(i + 2) * n + j] = acc2;
            c[(i + 3) * n + j] = acc3;
        }
    }
    for (; i < m; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            q16_16_t acc = 0;
            for (uint32_t p = 0; p < k; ++p) {
                acc = q16_add(acc, q16_mul(a[i * k + p], b[p * n + j]));
            }
            c[i * n + j] = acc;
        }
    }
    return 1;
}

void ai_model_init(void) {
    serial_write_string("DEBUG: ai_model_init starting...\n");
    uint32_t phys_start = (uint32_t)&_ai_model_start;
    uint32_t phys_end = (uint32_t)&_ai_model_end;
    
    serial_write_string("DEBUG: AI Model range: ");
    serial_write_hex32(phys_start);
    serial_write_string(" - ");
    serial_write_hex32(phys_end);
    serial_write_string("\n");

    if (phys_end <= phys_start) {
        serial_write_string("DEBUG: AI Model not found (size 0)\n");
        ai_mapped_base = 0;
        ai_mapped_size = 0;
        return;
    }
    uint32_t size = phys_end - phys_start;
    uint32_t aligned_phys = phys_start & 0xFFC00000;
    uint32_t offset = phys_start - aligned_phys;
    uint32_t total = offset + size;
    uint32_t page_count = (total + 0x3FFFFF) >> 22;
    
    serial_write_string("DEBUG: Mapping AI Model pages: ");
    serial_write_hex32(page_count);
    serial_write_string("\n");

    for (uint32_t i = 0; i < page_count; ++i) {
        if ((i % 10) == 0) serial_write_string(".");
        map_page_4mb(AI_MODEL_VIRT_BASE + (i << 22), aligned_phys + (i << 22), 0x3);
    }
    serial_write_string("\nDEBUG: AI Model pages mapped\n");

    ai_mapped_base = (uint8_t*)(AI_MODEL_VIRT_BASE + offset);
    ai_mapped_size = size;

    serial_write_string("DEBUG: Reading GGUF header...\n");
    gguf_header_t header;
    if (gguf_read_header(ai_mapped_base, &header)) {
        serial_write_string("DEBUG: GGUF OK\n");
        diag_log(DIAG_INFO, "gguf header ok");
    } else {
        serial_write_string("DEBUG: GGUF BAD\n");
        diag_log(DIAG_ERROR, "gguf header invalid");
    }
    serial_write_string("DEBUG: ai_model_init finished\n");
}

static int contains_word(const char* text, const char* word) {
    uint32_t i = 0;
    uint32_t wlen = strlen(word);
    if (wlen == 0) {
        return 0;
    }
    while (text[i]) {
        uint32_t j = 0;
        while (text[i + j] && word[j] && text[i + j] == word[j]) {
            j++;
        }
        if (j == wlen) {
            return 1;
        }
        i++;
    }
    return 0;
}

static void wait_ticks(uint64_t ticks) {
    uint64_t start = timer_get_ticks();
    while ((timer_get_ticks() - start) < ticks) { }
}

static void ai_stream_write_slow(const char* text) {
    for (uint32_t i = 0; text[i] != 0; ++i) {
        ai_stream_putc(text[i]);
        wait_ticks(1);
    }
}

void ai_infer(const char* prompt, const ai_context_t* context) {
    ai_stream_write_slow("BasicallyLinux AI: ");
    if (contains_word(prompt, "memory") || contains_word(prompt, "mem")) {
        ai_stream_write_slow("Memory usage ");
        ai_stream_write_slow("total=");
        ai_stream_write_uint64(context ? context->mem_total : 0);
        ai_stream_write_slow(" used=");
        ai_stream_write_uint64(context ? context->mem_used : 0);
        ai_stream_write_slow("\n");
        return;
    }
    if (contains_word(prompt, "uptime")) {
        ai_stream_write_slow("Uptime ticks ");
        ai_stream_write_uint64(context ? context->uptime_ticks : 0);
        ai_stream_write_slow("\n");
        return;
    }
    ai_stream_write_slow("Model mapped at 0xE0000000. Ask about memory or uptime.\n");
}

void ai_ask(const char* prompt, const ai_context_t* context) {
    ai_infer(prompt, context);
}

void ai_explain_page_fault(uint32_t err_code) {
    ai_stream_write("AI Page Fault Analysis: ");
    if (err_code & 0x1) {
        ai_stream_write("present ");
    } else {
        ai_stream_write("not-present ");
    }
    if (err_code & 0x2) {
        ai_stream_write("write ");
    } else {
        ai_stream_write("read ");
    }
    if (err_code & 0x4) {
        ai_stream_write("user ");
    } else {
        ai_stream_write("kernel ");
    }
    if (err_code & 0x8) {
        ai_stream_write("reserved-bit ");
    }
    if (err_code & 0x10) {
        ai_stream_write("instruction-fetch ");
    }
    ai_stream_write("\n");
}

uint32_t ai_debug_last_tokens(char* out, uint32_t max) {
    if (!out || max == 0) {
        return 0;
    }
    uint32_t count = token_count;
    if (count > max - 1) {
        count = max - 1;
    }
    uint32_t start = (token_head + 10 - count) % 10;
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = token_history[(start + i) % 10];
    }
    out[count] = 0;
    return count;
}

static q16_16_t q16_div(q16_16_t a, q16_16_t b) {
    if (b == 0) {
        return 0;
    }
    int64_t num = (int64_t)a << 16;
    return (q16_16_t)(num / b);
}

static uint64_t isqrt64(uint64_t x) {
    uint64_t op = x;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

static q16_16_t q16_inv_sqrt(q16_16_t x) {
    if (x <= 0) {
        return 0;
    }
    uint64_t scaled = ((uint64_t)(uint32_t)x) << 16;
    uint64_t root = isqrt64(scaled);
    if (root == 0) {
        return 0;
    }
    uint64_t inv = ((uint64_t)1 << 32) / root;
    return (q16_16_t)inv;
}

static q16_16_t q16_exp_approx(q16_16_t x) {
    q16_16_t max = q16_from_int(8);
    q16_16_t min = -q16_from_int(8);
    if (x > max) {
        x = max;
    }
    if (x < min) {
        x = min;
    }
    q16_16_t one = q16_from_int(1);
    q16_16_t x2 = q16_mul(x, x);
    q16_16_t x3 = q16_mul(x2, x);
    q16_16_t term = one;
    term = q16_add(term, x);
    term = q16_add(term, (q16_16_t)((int64_t)x2 / 2));
    term = q16_add(term, (q16_16_t)((int64_t)x3 / 6));
    if (term < 0) {
        term = 0;
    }
    return term;
}

int gguf_parse(gguf_header_t* out) {
    return ai_model_read_header(out);
}

int gguf_get_tensor(uint32_t index, ai_tensor_desc_t* out) {
    return ai_tensor_get(index, out);
}

int matmul(const q16_16_t* a, const q16_16_t* b, q16_16_t* c, uint32_t m, uint32_t n, uint32_t k) {
    return ai_matmul_q16_16(a, b, c, m, n, k);
}

q16_16_t dot_product(const q16_16_t* a, const q16_16_t* b, uint32_t n) {
    if (!a || !b || n == 0) {
        return 0;
    }
    q16_16_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) {
        sum = q16_add(sum, q16_mul(a[i], b[i]));
    }
    return sum;
}

void rmsnorm(q16_16_t* data, uint32_t n) {
    if (!data || n == 0) {
        return;
    }
    int64_t acc = 0;
    for (uint32_t i = 0; i < n; ++i) {
        q16_16_t val = data[i];
        acc += (int64_t)q16_mul(val, val);
    }
    q16_16_t mean = (q16_16_t)(acc / (int64_t)n);
    q16_16_t inv = q16_inv_sqrt(mean);
    for (uint32_t i = 0; i < n; ++i) {
        data[i] = q16_mul(data[i], inv);
    }
}

void softmax(q16_16_t* data, uint32_t n) {
    if (!data || n == 0) {
        return;
    }
    q16_16_t max = data[0];
    for (uint32_t i = 1; i < n; ++i) {
        if (data[i] > max) {
            max = data[i];
        }
    }
    q16_16_t sum = 0;
    for (uint32_t i = 0; i < n; ++i) {
        q16_16_t val = q16_sub(data[i], max);
        q16_16_t expv = q16_exp_approx(val);
        data[i] = expv;
        sum = q16_add(sum, expv);
    }
    if (sum == 0) {
        return;
    }
    for (uint32_t i = 0; i < n; ++i) {
        int64_t num = (int64_t)data[i] << 16;
        data[i] = (q16_16_t)(num / sum);
    }
}

void rope_apply(q16_16_t* data, uint32_t n) {
    if (!data || n < 2) {
        return;
    }
    for (uint32_t i = 0; i + 1 < n; i += 2) {
        q16_16_t a = data[i];
        q16_16_t b = data[i + 1];
        data[i] = q16_sub(a, (q16_16_t)(b >> 1));
        data[i + 1] = q16_add(b, (q16_16_t)(a >> 1));
    }
}

uint32_t tokenizer_encode(const char* text, uint32_t* out, uint32_t max) {
    if (!text || !out || max == 0) {
        return 0;
    }
    uint32_t count = 0;
    while (text[count] && count < max) {
        out[count] = (uint32_t)(uint8_t)text[count];
        count++;
    }
    return count;
}

uint32_t tokenizer_decode(uint32_t token, char* out, uint32_t max) {
    if (!out || max == 0) {
        return 0;
    }
    if (token > 255 || max < 2) {
        out[0] = 0;
        return 0;
    }
    out[0] = (char)token;
    out[1] = 0;
    return 1;
}

void ai_context_init(ai_context_t* context) {
    if (!context) {
        return;
    }
    get_system_context(context);
}

void ai_stream_token(uint32_t token) {
    char buf[2];
    if (tokenizer_decode(token, buf, sizeof(buf)) == 1) {
        ai_stream_putc(buf[0]);
    }
}

int kv_cache_init(uint32_t tokens) {
    kv_cache_tokens = tokens;
    return tokens ? 1 : 0;
}

static uint32_t ai_argmax(const q16_16_t* data, uint32_t n) {
    if (!data || n == 0) {
        return 0;
    }
    q16_16_t max = data[0];
    uint32_t idx = 0;
    for (uint32_t i = 1; i < n; ++i) {
        if (data[i] > max) {
            max = data[i];
            idx = i;
        }
    }
    return idx;
}

uint32_t sample_top_p(const q16_16_t* probs, uint32_t count, q16_16_t p) {
    if (!probs || count == 0) {
        return 0;
    }
    if (count == 1) {
        return 0;
    }
    q16_16_t one = q16_from_int(1);
    if (p <= 0) {
        return ai_argmax(probs, count);
    }
    if (p > one) {
        p = one;
    }
    q16_16_t temp = ai_temp > 0 ? ai_temp : one;
    q16_16_t* expv = (q16_16_t*)kmalloc(count * sizeof(q16_16_t));
    if (!expv) {
        return ai_argmax(probs, count);
    }
    uint8_t* used = (uint8_t*)kmalloc(count);
    if (!used) {
        kfree(expv);
        return ai_argmax(probs, count);
    }
    q16_16_t max = probs[0];
    for (uint32_t i = 1; i < count; ++i) {
        if (probs[i] > max) {
            max = probs[i];
        }
    }
    q16_16_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        q16_16_t centered = q16_sub(probs[i], max);
        q16_16_t scaled = q16_div(centered, temp);
        expv[i] = q16_exp_approx(scaled);
        sum = q16_add(sum, expv[i]);
        used[i] = 0;
    }
    if (sum == 0) {
        kfree(used);
        kfree(expv);
        return ai_argmax(probs, count);
    }
    q16_16_t target = q16_mul(p, sum);
    q16_16_t cumulative = 0;
    uint32_t selected = ai_argmax(probs, count);
    for (uint32_t picked = 0; picked < count; ++picked) {
        q16_16_t best = 0;
        int found = 0;
        uint32_t best_idx = 0;
        for (uint32_t i = 0; i < count; ++i) {
            if (used[i]) {
                continue;
            }
            if (!found || expv[i] > best) {
                best = expv[i];
                best_idx = i;
                found = 1;
            }
        }
        if (!found) {
            break;
        }
        used[best_idx] = 1;
        cumulative = q16_add(cumulative, best);
        selected = best_idx;
        if (cumulative >= target) {
            break;
        }
    }
    kfree(used);
    kfree(expv);
    return selected;
}

int transformer_step(const uint32_t* input, uint32_t input_len, uint32_t* output_token) {
    if (!input || !output_token || input_len == 0) {
        return 0;
    }
    *output_token = input[input_len - 1];
    return 1;
}

int ai_load_vocab(void) {
    return ai_mapped_base && ai_mapped_size ? 1 : 0;
}

void ai_panic_handler(const char* msg) {
    if (msg) {
        panic(msg);
    } else {
        panic("ai panic");
    }
}

float fixed_to_float(q16_16_t value) {
    return (float)value / 65536.0f;
}

q16_16_t activation_silu(q16_16_t x) {
    q16_16_t neg = q16_sub(0, x);
    q16_16_t expv = q16_exp_approx(neg);
    q16_16_t denom = q16_add(q16_from_int(1), expv);
    return q16_div(x, denom);
}

void ai_set_temp(q16_16_t temp) {
    ai_temp = temp;
}

void get_system_context(ai_context_t* context) {
    if (!context) {
        return;
    }
    context->mem_total = (uint64_t)pmm_total_blocks() * (uint64_t)pmm_block_size();
    context->mem_used = (uint64_t)pmm_used_blocks() * (uint64_t)pmm_block_size();
    context->heap_total = heap_total_bytes();
    context->heap_free = heap_free_bytes();
    context->uptime_ticks = timer_get_ticks();
}

void mmap_ai_weights(void) {
    ai_model_init();
}
