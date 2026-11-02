/* Original portable arithmetic; no native long, int128 or signed overflow. */
#include "guest_integer_division.h"
int tsfp_guest_integer_divide(uint64_t dividend,uint32_t divisor,tsfp_integer_quotient *result) {
    tsfp_integer_quotient next;
    if(!divisor || !result)return 0;
    next.quotient=dividend/divisor;next.remainder=(uint32_t)(dividend%divisor);
    *result=next;return 1;
}
uint64_t tsfp_guest_magic_divide(uint64_t dividend,uint64_t factor,uint8_t shift) {
    uint64_t magnitude,low,across,down,carry,high,quotient;
    unsigned negative=(unsigned)(dividend>>63);
    if(shift>=64)return 0;
    magnitude=negative?UINT64_C(0)-dividend:dividend;
    low=(uint64_t)(uint32_t)magnitude*(uint32_t)factor;
    across=(uint64_t)(uint32_t)magnitude*(factor>>32);
    down=(magnitude>>32)*(uint32_t)factor;
    carry=(low>>32)+(uint32_t)across+(uint32_t)down;
    high=(magnitude>>32)*(factor>>32)+(across>>32)+(down>>32)+(carry>>32);
    quotient=high>>shift;
    return negative?UINT64_C(0)-quotient:quotient;
}
