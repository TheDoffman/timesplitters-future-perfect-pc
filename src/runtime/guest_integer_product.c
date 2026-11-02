/* Original modulo-2^64 arithmetic, independent of toolkit implementations. */
#include "guest_integer_product.h"
uint64_t tsfp_guest_integer_product(uint64_t multiplicand,uint32_t multiplier) {
    uint64_t factor=multiplier;
    if(multiplier&UINT32_C(0x80000000))factor|=UINT64_C(0xffffffff00000000);
    return multiplicand*factor;
}
