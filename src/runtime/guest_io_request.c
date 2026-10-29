#include "guest_io_request.h"
#include <string.h>
static uint32_t get(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put(unsigned char *p,uint32_t v) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));
}
static int bounds(uint32_t address,uint32_t bytes,uint32_t stacks) {
    uint64_t end=(uint64_t)address+TSFP_IO_REQUEST_BYTES+(uint64_t)stacks*TSFP_IO_STACK_BYTES;
    return address && stacks<=TSFP_IO_MAX_STACKS && bytes<=UINT16_MAX &&
        bytes>=TSFP_IO_REQUEST_BYTES+stacks*TSFP_IO_STACK_BYTES &&
        (uint64_t)address+bytes-1<=UINT32_MAX && end<=UINT32_MAX;
}
int tsfp_io_request_initialize(void *target,size_t span,uint32_t address,
    uint32_t bytes,uint32_t stacks,uint16_t type) {
    unsigned char *p=(unsigned char *)target;
    if(!p || !bounds(address,bytes,stacks) || span<bytes)return 0;
    memset(p,0,bytes);
    p[0]=(unsigned char)type;p[1]=(unsigned char)(type>>8);
    p[2]=(unsigned char)bytes;p[3]=(unsigned char)(bytes>>8);
    p[0x18]=(unsigned char)stacks;p[0x19]=(unsigned char)(stacks+1);
    put(p+0x5c,address+TSFP_IO_REQUEST_BYTES+stacks*TSFP_IO_STACK_BYTES);
    return 1;
}
int tsfp_io_request_peek(const void *target,size_t span,uint32_t address,uint32_t *out) {
    const unsigned char *p=(const unsigned char *)target;uint32_t bytes,stacks,location,current,expected;
    if(!p || !out || span<TSFP_IO_REQUEST_BYTES)return 0;
    bytes=(uint32_t)p[2]|((uint32_t)p[3]<<8);stacks=p[0x18];location=p[0x19];
    if(!bounds(address,bytes,stacks) || span<bytes || location<=1 || location>stacks+1)return 0;
    current=get(p+0x5c);expected=address+TSFP_IO_REQUEST_BYTES+(location-1)*TSFP_IO_STACK_BYTES;
    if(current!=expected)return 0;
    current-=TSFP_IO_STACK_BYTES;
    *out=current;return 1;
}

int tsfp_io_request_advance(void *target,size_t span,uint32_t address,uint32_t *out) {
    unsigned char *p=(unsigned char *)target;uint32_t current;
    if(!out || !tsfp_io_request_peek(target,span,address,&current))return 0;
    --p[0x19];put(p+0x5c,current);*out=current;return 1;
}
