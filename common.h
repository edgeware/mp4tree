#pragma once

#include <stdlib.h>
#include <stdint.h>

const char *
indent(int depth, int header);

uint16_t
get_u16(const uint8_t * p);

uint32_t
get_u24(const uint8_t * p);

uint32_t
get_u32(const uint8_t * p);

uint64_t
get_u64(const uint8_t * p);

const char *
get_pascal_string(const uint8_t * p);

uint32_t
get_exp_golomb(const uint8_t *p, uint32_t * bit);

void
print_hex(const uint8_t * buf, uint32_t num);

uint8_t
get_bit(const uint8_t * p, int n);
