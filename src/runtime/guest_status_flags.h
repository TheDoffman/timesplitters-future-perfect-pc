/* Original arithmetic status flags. Unknown bits are never advertised as valid. */
#ifndef TSFP_GUEST_STATUS_FLAGS_H
#define TSFP_GUEST_STATUS_FLAGS_H
#include <stdint.h>
typedef struct tsfp_status_flags {uint32_t value,known;} tsfp_status_flags;
enum {TSFP_FLAG_CF=1,TSFP_FLAG_PF=4,TSFP_FLAG_AF=16,TSFP_FLAG_ZF=64,TSFP_FLAG_SF=128,TSFP_FLAG_OF=2048,TSFP_STATUS_FLAGS=0x8d5};
static inline void tsfp_status_forget(tsfp_status_flags *s,uint32_t bits) {
    if(s){s->known&=~(bits&TSFP_STATUS_FLAGS);s->known|=2;s->value|=2;}
}
static inline void tsfp_status_write(tsfp_status_flags *s,uint32_t defined,uint32_t value,uint32_t undefined) {
    if(!s)return;defined&=TSFP_STATUS_FLAGS;undefined&=TSFP_STATUS_FLAGS;
    s->value=(s->value&~(defined|undefined))|(value&defined)|2;
    s->known=(s->known&~undefined)|defined|2;
}
static inline uint32_t tsfp_status_carry(const tsfp_status_flags *s,uint32_t fallback) {
    return s && (s->known&TSFP_FLAG_CF)?s->value&TSFP_FLAG_CF:!!fallback;
}
static inline uint32_t tsfp_status_szp(uint32_t result,unsigned bits) {
    return (result?0:TSFP_FLAG_ZF)|((result&(UINT32_C(1)<<(bits-1)))?TSFP_FLAG_SF:0)|
        (((UINT32_C(0x9669)>>((result^(result>>4))&15))&1)?TSFP_FLAG_PF:0);
}
static inline void tsfp_status_alu(tsfp_status_flags *s,unsigned bits,uint32_t left,uint32_t right,uint32_t carry,uint32_t result,int subtract,uint32_t defined) {
    uint32_t mask,sign,value;
    if(!s || (bits!=8 && bits!=16 && bits!=32))return;
    mask=bits==32?UINT32_MAX:(UINT32_C(1)<<bits)-1;sign=UINT32_C(1)<<(bits-1);
    left&=mask;right&=mask;result&=mask;carry&=1;
    value=tsfp_status_szp(result,bits)|((left^right^result)&TSFP_FLAG_AF);
    if(subtract) {
        if((uint64_t)left<(uint64_t)right+carry)value|=TSFP_FLAG_CF;
        if((left^right)&(left^result)&sign)value|=TSFP_FLAG_OF;
    } else {
        if((uint64_t)left+right+carry>mask)value|=TSFP_FLAG_CF;
        if(~(left^right)&(left^result)&sign)value|=TSFP_FLAG_OF;
    }
    tsfp_status_write(s,defined,value,0);
}
static inline void tsfp_status_logic(tsfp_status_flags *s,unsigned bits,uint32_t result) {
    uint32_t mask;if(!s || (bits!=8 && bits!=16 && bits!=32))return;
    mask=bits==32?UINT32_MAX:(UINT32_C(1)<<bits)-1;
    tsfp_status_write(s,TSFP_STATUS_FLAGS&~TSFP_FLAG_AF,tsfp_status_szp(result&mask,bits),TSFP_FLAG_AF);
}
static inline void tsfp_status_complement_carry(tsfp_status_flags *s) {
    if(s){s->value^=TSFP_FLAG_CF;s->value|=2;s->known|=2;}
}
static inline uint32_t tsfp_status_lahf(const tsfp_status_flags *s,uint32_t fallback) {
    uint32_t known=s?s->known&(TSFP_STATUS_FLAGS&255):0;
    return ((fallback&~known)|(s?s->value&known:0)|2)&0xd7;
}
static inline uint32_t tsfp_status_condition(const tsfp_status_flags *s,unsigned code,uint32_t fallback) {
    static const uint16_t required[16]={2048,2048,1,1,64,64,65,65,128,128,4,4,2176,2176,2240,2240};
    uint32_t f,v=0;if(!s || code>=16 || (s->known&required[code])!=required[code])return !!fallback;
    f=s->value;
    switch(code>>1) {
        case 0:v=!!(f&TSFP_FLAG_OF);break;
        case 1:v=!!(f&TSFP_FLAG_CF);break;
        case 2:v=!!(f&TSFP_FLAG_ZF);break;
        case 3:v=!!(f&(TSFP_FLAG_CF|TSFP_FLAG_ZF));break;
        case 4:v=!!(f&TSFP_FLAG_SF);break;
        case 5:v=!!(f&TSFP_FLAG_PF);break;
        case 6:v=!!(f&TSFP_FLAG_SF)!=!!(f&TSFP_FLAG_OF);break;
        case 7:v=!!(f&TSFP_FLAG_ZF) || (!!(f&TSFP_FLAG_SF)!=!!(f&TSFP_FLAG_OF));break;
    }
    return v^(code&1);
}
enum {TSFP_SHIFT_LEFT,TSFP_SHIFT_RIGHT,TSFP_SHIFT_ARITHMETIC,TSFP_SHIFT_DOUBLE_LEFT,TSFP_SHIFT_DOUBLE_RIGHT};
static inline void tsfp_status_shift(tsfp_status_flags *s,unsigned bits,uint32_t input,unsigned count,uint32_t result,unsigned operation) {
    uint32_t mask,value,defined=TSFP_FLAG_SF|TSFP_FLAG_ZF|TSFP_FLAG_PF;
    uint32_t undefined=TSFP_FLAG_CF|TSFP_FLAG_OF|TSFP_FLAG_AF,carry=0;
    if(!s || (bits!=8 && bits!=16 && bits!=32) || operation>TSFP_SHIFT_DOUBLE_RIGHT)return;
    if(operation>=TSFP_SHIFT_DOUBLE_LEFT && bits==8)return;
    count&=31u;if(!count)return;
    if(operation>=TSFP_SHIFT_DOUBLE_LEFT && count>bits){tsfp_status_forget(s,TSFP_STATUS_FLAGS);return;}
    mask=bits==32?UINT32_MAX:(UINT32_C(1)<<bits)-1;input&=mask;result&=mask;
    value=tsfp_status_szp(result,bits);
    if(operation==TSFP_SHIFT_ARITHMETIC || operation>=TSFP_SHIFT_DOUBLE_LEFT || count<bits) {
        if(operation==TSFP_SHIFT_LEFT || operation==TSFP_SHIFT_DOUBLE_LEFT)carry=(input>>(bits-count))&1u;
        else if(operation==TSFP_SHIFT_ARITHMETIC && count>=bits)carry=(input>>(bits-1))&1u;
        else carry=(input>>(count-1))&1u;
        defined|=TSFP_FLAG_CF;undefined&=~TSFP_FLAG_CF;value|=carry;
    }
    if(count==1) {
        uint32_t overflow=operation==TSFP_SHIFT_ARITHMETIC?0:
            operation==TSFP_SHIFT_RIGHT?(input>>(bits-1))&1u:((input^result)>>(bits-1))&1u;
        defined|=TSFP_FLAG_OF;undefined&=~TSFP_FLAG_OF;value|=overflow*TSFP_FLAG_OF;
    }
    tsfp_status_write(s,defined,value,undefined);
}
enum {TSFP_ROTATE_LEFT,TSFP_ROTATE_RIGHT,TSFP_ROTATE_CARRY_LEFT,TSFP_ROTATE_CARRY_RIGHT};
static inline void tsfp_status_rotate(tsfp_status_flags *s,unsigned bits,uint32_t input,unsigned count,uint32_t result,unsigned operation) {
    uint32_t value,defined=TSFP_FLAG_CF,undefined=TSFP_FLAG_OF,carry,sign,prior_carry_known;
    unsigned distance;
    if(!s || (bits!=8 && bits!=16 && bits!=32) || operation>TSFP_ROTATE_CARRY_RIGHT)return;
    count&=31u;if(!count)return;
    distance=count%(bits+(operation>=TSFP_ROTATE_CARRY_LEFT));
    if(!distance && operation>=TSFP_ROTATE_CARRY_LEFT){tsfp_status_write(s,0,0,TSFP_FLAG_OF);return;}
    prior_carry_known=s->known&TSFP_FLAG_CF;sign=UINT32_C(1)<<(bits-1);
    if(operation==TSFP_ROTATE_LEFT)carry=result&1u;
    else if(operation==TSFP_ROTATE_RIGHT)carry=!!(result&sign);
    else if(operation==TSFP_ROTATE_CARRY_LEFT)carry=(input>>(bits-distance))&1u;
    else carry=(input>>(distance-1))&1u;
    value=carry;
    if(count==1 && (operation!=TSFP_ROTATE_CARRY_RIGHT || prior_carry_known)) {
        uint32_t overflow=operation==TSFP_ROTATE_CARRY_LEFT?((input^(input<<1))&sign):
            operation==TSFP_ROTATE_LEFT?((!!(result&sign)^carry)*sign):((result^(result<<1))&sign);
        defined|=TSFP_FLAG_OF;undefined=0;value|=overflow?TSFP_FLAG_OF:0;
    }
    tsfp_status_write(s,defined,value,undefined);
}
#endif
