#ifndef GGUF_H
#define GGUF_H

#include "types.h"

#define GGUF_MAGIC 0x46554747u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t kv_count;
} gguf_header_t;

typedef struct {
    const uint8_t* base;
    uint64_t size;
    uint64_t offset;
} gguf_reader_t;

enum {
    GGUF_TYPE_UINT8 = 0,
    GGUF_TYPE_INT8 = 1,
    GGUF_TYPE_UINT16 = 2,
    GGUF_TYPE_INT16 = 3,
    GGUF_TYPE_UINT32 = 4,
    GGUF_TYPE_INT32 = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL = 7,
    GGUF_TYPE_STRING = 8,
    GGUF_TYPE_ARRAY = 9,
    GGUF_TYPE_UINT64 = 10,
    GGUF_TYPE_INT64 = 11,
    GGUF_TYPE_FLOAT64 = 12
};

static inline int gguf_read_header(const uint8_t* base, gguf_header_t* out) {
    if (!base || !out) {
        return 0;
    }
    const uint32_t* u32 = (const uint32_t*)base;
    const uint64_t* u64 = (const uint64_t*)(base + 8);
    out->magic = u32[0];
    out->version = u32[1];
    out->tensor_count = u64[0];
    out->kv_count = u64[1];
    return out->magic == GGUF_MAGIC;
}

static inline int gguf_reader_init(gguf_reader_t* r, const uint8_t* base, uint64_t size) {
    if (!r || !base) {
        return 0;
    }
    r->base = base;
    r->size = size;
    r->offset = 0;
    return 1;
}

static inline int gguf_reader_seek(gguf_reader_t* r, uint64_t offset) {
    if (!r || offset > r->size) {
        return 0;
    }
    r->offset = offset;
    return 1;
}

static inline int gguf_read_u32(gguf_reader_t* r, uint32_t* out) {
    if (!r || !out || r->offset + 4 > r->size) {
        return 0;
    }
    const uint32_t* ptr = (const uint32_t*)(r->base + r->offset);
    *out = *ptr;
    r->offset += 4;
    return 1;
}

static inline int gguf_read_u64(gguf_reader_t* r, uint64_t* out) {
    if (!r || !out || r->offset + 8 > r->size) {
        return 0;
    }
    const uint64_t* ptr = (const uint64_t*)(r->base + r->offset);
    *out = *ptr;
    r->offset += 8;
    return 1;
}

static inline int gguf_read_string(gguf_reader_t* r, const uint8_t** out, uint64_t* len) {
    if (!r || !out || !len) {
        return 0;
    }
    if (!gguf_read_u64(r, len)) {
        return 0;
    }
    if (r->offset + *len > r->size) {
        return 0;
    }
    *out = r->base + r->offset;
    r->offset += *len;
    return 1;
}

static inline uint32_t gguf_type_size(uint32_t type) {
    switch (type) {
        case GGUF_TYPE_UINT8:
        case GGUF_TYPE_INT8:
        case GGUF_TYPE_BOOL:
            return 1;
        case GGUF_TYPE_UINT16:
        case GGUF_TYPE_INT16:
            return 2;
        case GGUF_TYPE_UINT32:
        case GGUF_TYPE_INT32:
        case GGUF_TYPE_FLOAT32:
            return 4;
        case GGUF_TYPE_UINT64:
        case GGUF_TYPE_INT64:
        case GGUF_TYPE_FLOAT64:
            return 8;
        default:
            return 0;
    }
}

static inline int gguf_skip_value(gguf_reader_t* r, uint32_t type) {
    if (!r) {
        return 0;
    }
    if (type == GGUF_TYPE_STRING) {
        const uint8_t* str;
        uint64_t len;
        return gguf_read_string(r, &str, &len);
    }
    if (type == GGUF_TYPE_ARRAY) {
        uint32_t elem_type;
        uint64_t count;
        if (!gguf_read_u32(r, &elem_type)) {
            return 0;
        }
        if (!gguf_read_u64(r, &count)) {
            return 0;
        }
        if (elem_type == GGUF_TYPE_STRING) {
            for (uint64_t i = 0; i < count; ++i) {
                const uint8_t* str;
                uint64_t len;
                if (!gguf_read_string(r, &str, &len)) {
                    return 0;
                }
            }
            return 1;
        }
        uint32_t size = gguf_type_size(elem_type);
        if (size == 0) {
            return 0;
        }
        uint64_t bytes = (uint64_t)size * count;
        if (r->offset + bytes > r->size) {
            return 0;
        }
        r->offset += bytes;
        return 1;
    }
    uint32_t size = gguf_type_size(type);
    if (size == 0) {
        return 0;
    }
    if (r->offset + size > r->size) {
        return 0;
    }
    r->offset += size;
    return 1;
}

static inline int gguf_skip_kv_pairs(gguf_reader_t* r, uint64_t kv_count) {
    for (uint64_t i = 0; i < kv_count; ++i) {
        const uint8_t* key;
        uint64_t key_len;
        uint32_t type;
        if (!gguf_read_string(r, &key, &key_len)) {
            return 0;
        }
        if (!gguf_read_u32(r, &type)) {
            return 0;
        }
        if (!gguf_skip_value(r, type)) {
            return 0;
        }
    }
    return 1;
}

#endif
