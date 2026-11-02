/* Scalar compiler-emitted MOVs at protected guest MMIO. Unknown forms fail
 * closed. Register numbering is the x86 encoding order, independent of OS ABI. */
#ifndef TSFP_X64_ACCESS_H
#define TSFP_X64_ACCESS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {
    uintptr_t address;
    uint64_t immediate;
    unsigned length,bytes,result_bytes,reg,shift,load,sign_extend,has_immediate;
} TsfpX64Access;
static inline int tsfp_x64_access(const unsigned char *code,size_t available,
                                 uintptr_t pc,const uint64_t regs[16],TsfpX64Access *out) {
    TsfpX64Access a={0};size_t n=0;unsigned word=0,rex=0,op,modrm,mod,rm,reg;
    uint64_t address=0;int rip_relative=0;int32_t displacement=0;
    if(!code||!regs||!out)return 0;
#define BYTE(v) do {if(n>=available||n>=15)return 0;(v)=code[n++];} while(0)
    for(;;) {
        if(n>=available||n>=15)return 0;
        unsigned b=code[n];
        if(b==0x66 && !rex){word=1;n++;}
        else if(b>=0x40&&b<=0x4f){rex=b;n++;}
        else break;
    }
    BYTE(op);a.bytes=(rex&8)?8:word?2:4;a.result_bytes=a.bytes;
    if(op==0x8a||op==0x88||op==0xc6)a.bytes=a.result_bytes=1;
    if(op==0x8b||op==0x8a)a.load=1;
    else if(op==0x89||op==0x88){}
    else if(op==0xc6||op==0xc7)a.has_immediate=1;
    else if(op==0x63 && (rex&8)){a.load=a.sign_extend=1;a.bytes=4;a.result_bytes=8;}
    else if(op==0x0f) {
        unsigned second;BYTE(second);
        if(second!=0xb6&&second!=0xb7&&second!=0xbe&&second!=0xbf)return 0;
        a.load=1;a.sign_extend=second>=0xbe;a.bytes=(second&1)?2:1;
    } else return 0;
    BYTE(modrm);mod=modrm>>6;rm=modrm&7;reg=((modrm>>3)&7)|((rex&4)?8:0);
    if(mod==3 || (a.has_immediate && reg))return 0;
    a.reg=reg;
    if(a.result_bytes==1&&!rex&&reg>=4&&reg<8){a.reg=reg-4;a.shift=8;}
    if(rm==4) {
        unsigned sib;BYTE(sib);unsigned index=((sib>>3)&7)|((rex&2)?8:0),base=(sib&7)|((rex&1)?8:0);
        if(index!=4)address=regs[index]<<(sib>>6);
        if(mod==0&&(sib&7)==5) {
            if(n+4>available||n+4>15)return 0;
            memcpy(&displacement,code+n,4);n+=4;
        } else address+=regs[base];
    } else if(mod==0&&rm==5) {
        rip_relative=1;
        if(n+4>available||n+4>15)return 0;
        memcpy(&displacement,code+n,4);n+=4;
    } else address=regs[rm|((rex&1)?8:0)];
    if(mod==1){unsigned d;BYTE(d);displacement=(int8_t)d;}
    if(mod==2){if(n+4>available||n+4>15)return 0;memcpy(&displacement,code+n,4);n+=4;}
    if(a.has_immediate) {
        unsigned bytes=a.bytes==8?4:a.bytes;
        if(n+bytes>available||n+bytes>15)return 0;
        memcpy(&a.immediate,code+n,bytes);n+=bytes;
        if(a.bytes==8)a.immediate=(uint64_t)(int64_t)(int32_t)a.immediate;
    }
    a.address=(uintptr_t)(address+(uint64_t)(int64_t)displacement+(rip_relative?pc+n:0));
    a.length=(unsigned)n;*out=a;return 1;
#undef BYTE
}
static inline uint64_t tsfp_x64_load_result(uint64_t old,uint64_t value,const TsfpX64Access *a) {
    if(a->bytes<8){unsigned bits=a->bytes*8;value&=(UINT64_C(1)<<bits)-1;
        if(a->sign_extend)value=(uint64_t)((int64_t)(value<<(64-bits))>>(64-bits));}
    if(a->result_bytes==8)return value;
    if(a->result_bytes==4)return (uint32_t)value;
    uint64_t mask=((UINT64_C(1)<<(a->result_bytes*8))-1)<<a->shift;
    return (old&~mask)|((value<<a->shift)&mask);
}
#endif
