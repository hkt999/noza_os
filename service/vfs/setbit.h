#pragma once

static inline void set_bit(uint32_t *bits, uint32_t i)
{
    bits[i / 32] |= (1 << (i % 32));
}

static inline void clear_bit(uint32_t *bits, uint32_t i)
{
    bits[i / 32] &= ~(1 << (i % 32));
}

static inline int test_bit(uint32_t *bits, uint32_t i)
{
    return (bits[i / 32] & (1 << (i % 32))) != 0;
}

// TODO: extend this to a variable length bitstream
static inline uint32_t find_first_set_bit(uint32_t x)
{
    if (x==0) {
        return 32;
    }
    return __builtin_ctz(x);
}

static inline uint32_t find_first_zero_bit(uint32_t x)
{
    return find_first_set_bit(~x);
}
