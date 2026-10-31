#include "guest_io_transfer.h"
static uint32_t read_word(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void write_word(unsigned char *p,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));
}
static int transfer_major(unsigned major) {return major==2 || major==3;}
int tsfp_io_transfer_decode(const void *source,size_t bytes,tsfp_io_transfer *out) {
    const unsigned char *p=(const unsigned char *)source;tsfp_io_transfer value={0};
    if(!p || !out || bytes<TSFP_IO_STACK_BYTES || !transfer_major(p[0]))return 0;
    value.major=p[0];value.minor=p[1];value.flags=p[2];value.control=p[3];
    value.length=read_word(p+4);value.buffer_word=read_word(p+8);
    value.byte_offset=(uint64_t)read_word(p+12)|((uint64_t)read_word(p+16)<<32);
    value.device=read_word(p+20);*out=value;return 1;
}
int tsfp_io_transfer_encode(void *target,size_t bytes,const tsfp_io_transfer *source) {
    unsigned char *p=(unsigned char *)target;tsfp_io_transfer value;
    if(!p || !source || bytes<TSFP_IO_STACK_BYTES || !transfer_major(source->major))return 0;
    value=*source;p[0]=value.major;p[1]=value.minor;p[2]=value.flags;p[3]=value.control;
    write_word(p+4,value.length);write_word(p+8,value.buffer_word);
    write_word(p+12,(uint32_t)value.byte_offset);write_word(p+16,(uint32_t)(value.byte_offset>>32));
    write_word(p+20,value.device);return 1;
}
int tsfp_io_transfer_address(const tsfp_io_transfer *value,uint32_t user_buffer,uint32_t *out) {
    uint64_t address;
    if(!value || !out || !transfer_major(value->major))return 0;
    address=value->buffer_word;
    if(!(value->flags&0x80))address+=user_buffer;
    if(address>UINT32_MAX || (!address && value->length) || address+value->length>UINT64_C(0x100000000))return 0;
    *out=(uint32_t)address;return 1;
}
