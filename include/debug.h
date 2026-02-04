#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"

void debug_write(const char* msg);
void debug_write_hex32(uint32_t value);
void debug_write_hex64(uint64_t value);
void panic(const char* msg);
void debug_backtrace(uint32_t max_frames);
void debug_backtrace_from(uint32_t ebp, uint32_t max_frames);
void kprintf(const char* fmt, ...);

#endif
