#include "guest_io_stack.h"
#include <string.h>
int tsfp_io_stack_decode(const void *source,size_t bytes,tsfp_io_stack *out) {
    const unsigned char *p=(const unsigned char *)source;tsfp_io_stack value={0};
    if(!p || !out || bytes<TSFP_IO_STACK_BYTES)return 0;
    value.major=p[0];value.minor=p[1];value.flags=p[2];value.control=p[3];
    memcpy(value.opaque,p+4,sizeof(value.opaque));*out=value;return 1;
}
int tsfp_io_stack_encode(void *target,size_t bytes,const tsfp_io_stack *source) {
    unsigned char *p=(unsigned char *)target;tsfp_io_stack value;
    if(!p || !source || bytes<TSFP_IO_STACK_BYTES)return 0;
    value=*source;
    p[0]=value.major;p[1]=value.minor;p[2]=value.flags;p[3]=value.control;
    memcpy(p+4,value.opaque,sizeof(value.opaque));return 1;
}
