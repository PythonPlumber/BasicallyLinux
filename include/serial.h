#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

void serial_init(void);
void serial_write(const char* text);
void serial_write_len(const char* text, uint32_t len);
void serial_write_hex32(uint32_t value);
void serial_write_hex64(uint64_t value);

#endif
