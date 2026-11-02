/* Original bit-defined extended integer product. */
#ifndef TSFP_GUEST_INTEGER_PRODUCT_H
#define TSFP_GUEST_INTEGER_PRODUCT_H
#include <stdint.h>
/* Multiplicand is a 64-bit two's-complement bit pattern; multiplier is a
 * 32-bit two's-complement bit pattern. Return the low 64 product bits.
 * No signed conversion, signed overflow or host long width participates. */
uint64_t tsfp_guest_integer_product(uint64_t multiplicand,uint32_t multiplier);
#endif
