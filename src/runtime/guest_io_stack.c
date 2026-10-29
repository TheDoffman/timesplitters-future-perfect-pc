#include "guest_io_stack.h"
static uint32_t get(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put(unsigned char *p,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));
}
int tsfp_io_stack_decode(const void *source,size_t bytes,tsfp_io_stack *out) {
    const unsigned char *p=(const unsigned char *)source;tsfp_io_stack value={0};
    if(!p || !out || bytes<TSFP_IO_STACK_BYTES)return 0;
    value.major=p[0];value.minor=p[1];value.flags=p[2];value.control=p[3];
    value.length=get(p+4);value.key=get(p+8);
    value.offset=(uint64_t)get(p+12)|((uint64_t)get(p+16)<<32);
    value.device=get(p+20);*out=value;return 1;
}
int tsfp_io_stack_encode(void *target,size_t bytes,const tsfp_io_stack *source) {
    unsigned char *p=(unsigned char *)target;tsfp_io_stack value;
    if(!p || !source || bytes<TSFP_IO_STACK_BYTES)return 0;
    value=*source;
    p[0]=value.major;p[1]=value.minor;p[2]=value.flags;p[3]=value.control;
    put(p+4,value.length);put(p+8,value.key);put(p+12,(uint32_t)value.offset);
    put(p+16,(uint32_t)(value.offset>>32));put(p+20,value.device);return 1;
}
