#include <stdio.h>
#include <string.h>

#include "common.h"

const char *
indent(int depth, int header)
{
    static char buf[64];
    int i = 0;

    memset(buf, 0 , sizeof buf);

    for (i = 0; i < depth; i++)
    {
        strcat(buf, "|  ");
    }

    if (header)
        strcat(buf, "+");
    else
        strcat(buf, "");

    return buf;
}

inline uint16_t
get_u16(const uint8_t * p)
{
    return (p[0] << 8) | p[1];
}

inline uint32_t
get_u24(const uint8_t * p)
{
    return (p[0] << 16) | (p[1] << 8) | p [2];
}


inline uint32_t
get_u32(const uint8_t * p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

inline uint64_t
get_u64(const uint8_t * p)
{
    return  ((uint64_t)p[0] << 56) |
            ((uint64_t)p[1] << 48) |
            ((uint64_t)p[2] << 40) |
            ((uint64_t)p[3] << 32) |
            ((uint64_t)p[4] << 24) |
            ((uint64_t)p[5] << 16) |
            ((uint64_t)p[6] << 8)  |
            (uint64_t) p[7];
}

const char *
get_pascal_string(const uint8_t * p)
{
    uint16_t length = p[0];
    (void) length;
    return (char *)p + 1;
}

uint8_t
get_bit(const uint8_t * p, int n)
{

    return (p[n/8] & (1 << (7-(n % 8)))) ? 1 : 0;
}

uint32_t
get_exp_golomb(const uint8_t *p, uint32_t * bit)
{
    int      leading_zero_bits = 0;
    int      i;
    int      j;
    uint32_t v = 0;

    for (i = *bit; 1; i++)
    {

        if (get_bit(p, i) == 0)
            leading_zero_bits++;
        else
            break;
    }

    for (j = 0; j < leading_zero_bits; j++)
    {
        v |= get_bit(p, i + j + 1) << (leading_zero_bits -j -1);
    }

    *bit += 2 * leading_zero_bits + 1;
    return (1 << leading_zero_bits) - 1 + v;
}

void
print_hex(const uint8_t * buf, uint32_t num)
{
    uint32_t pos = 0;
    while (pos < num)
    {
        printf("%.2x", buf[pos++]);
    }
}

