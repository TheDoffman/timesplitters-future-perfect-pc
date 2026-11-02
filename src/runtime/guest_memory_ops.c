/* Original checked guest services; standard libc supplies bulk byte movement. */
#include "guest_memory_ops.h"
#include <string.h>
static unsigned char *memory_span(const tsfp_guest_vm *vm,uint32_t address,uint32_t bytes,int write) {
    if(!vm || !vm->resolve || !address || !bytes || address>UINT32_MAX-(bytes-1))return NULL;
    return vm->resolve(vm->host,address,bytes,write);
}
static uint32_t memory_word(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
int tsfp_memory_fill(const tsfp_guest_vm *vm,uint32_t destination,uint32_t bytes,uint32_t pattern,unsigned width) {
    unsigned char *out;
    if(width!=1 && width!=4)return 0;
    if(width==4)bytes-=bytes%4;
    if(!bytes)return 1;
    out=memory_span(vm,destination,bytes,1);if(!out)return 0;
    if(width==1)memset(out,(unsigned char)pattern,bytes);
    else for(uint32_t i=0;i<bytes;++i)out[i]=(unsigned char)(pattern>>((i%4)*8));
    return 1;
}
int tsfp_memory_move(const tsfp_guest_vm *vm,uint32_t destination,uint32_t source,uint32_t bytes) {
    unsigned char *out,*in;
    if(!bytes)return 1;
    in=memory_span(vm,source,bytes,0);if(!in)return 0;
    out=memory_span(vm,destination,bytes,1);if(!out)return 0;
    memmove(out,in,bytes);return 1;
}
int tsfp_memory_map_mask(const tsfp_guest_vm *vm,uint32_t mask,uint32_t mapping) {
    static const uint32_t flags[4]={UINT32_C(0x80000000),UINT32_C(0x40000000),UINT32_C(0x20000000),UINT32_C(0x10000000)};
    unsigned char *out=memory_span(vm,mask,4,1),*in;uint32_t value,table[4];
    if(!out)return 0;
    in=memory_span(vm,mapping,16,0);if(!in)return 0;
    value=memory_word(out);
    for(unsigned i=0;i<4;++i)table[i]=memory_word(in+i*4);
    for(unsigned i=0;i<4;++i)if(value&flags[i])value|=table[i];
    value&=UINT32_C(0x0fffffff);
    for(unsigned i=0;i<4;++i)out[i]=(unsigned char)(value>>(i*8));
    return 1;
}
uint32_t tsfp_memory_byte_swap(uint32_t value,unsigned width) {
    uint32_t result=0;
    if(width!=2 && width!=4)return 0;
    for(unsigned i=0;i<width;++i)result=(result<<8)|((value>>(i*8))&255);
    return result;
}
