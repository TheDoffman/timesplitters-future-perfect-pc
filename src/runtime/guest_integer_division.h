/* Original extended division over guest bit patterns. */
#ifndef TSFP_GUEST_INTEGER_DIVISION_H
#define TSFP_GUEST_INTEGER_DIVISION_H
#include <stdint.h>
typedef struct tsfp_integer_quotient {uint64_t quotient;uint32_t remainder;} tsfp_integer_quotient;
/* Unsigned 64/32 quotient/remainder. Invalid result or zero divisor leaves the
 * result unchanged and returns0. The import owns divide-error delivery. */
int tsfp_guest_integer_divide(uint64_t dividend,uint32_t divisor,tsfp_integer_quotient *result);
/* Signed dividend magnitude times an unsigned 64-bit fractional factor. Take
 * the high64 product bits, shift right, then restore the dividend's sign.
 * Shift is the guest CCHAR's byte pattern; values>=64 yield zero. */
uint64_t tsfp_guest_magic_divide(uint64_t dividend,uint64_t factor,uint8_t shift);
#endif
