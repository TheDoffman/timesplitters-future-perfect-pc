#include "guest_io_build.h"
#include <string.h>
static void put_word(unsigned char *p,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));
}
int tsfp_io_build_packet(void *target,size_t span,uint32_t address,uint32_t bytes,
    uint32_t stacks,const tsfp_io_build_plan *source) {
    unsigned char slot[TSFP_IO_STACK_BYTES],*p=(unsigned char *)target;
    tsfp_io_build_plan value;uint32_t buffer_address;size_t offset;
    if(!p || !source || !stacks || stacks>TSFP_IO_MAX_STACKS)return 0;
    value=*source;
    if(!value.transfer.device || value.transfer.device>UINT32_MAX-71 ||
       (value.user_iosb && value.user_iosb>UINT32_MAX-7) ||
       (value.user_event && value.user_event>UINT32_MAX-15))return 0;
    if(!tsfp_io_transfer_address(&value.transfer,value.user_buffer,&buffer_address) ||
       !tsfp_io_transfer_encode(slot,sizeof(slot),&value.transfer))return 0;
    /* The transport's address/range validation is needed even though the guest
     * packet retains UserBuffer plus its separate absolute/relative word. */
    (void)buffer_address;
    if(!tsfp_io_request_initialize(p,span,address,bytes,stacks,6))return 0;
    offset=TSFP_IO_REQUEST_BYTES+(size_t)(stacks-1)*TSFP_IO_STACK_BYTES;
    memcpy(p+offset,slot,sizeof(slot));
    put_word(p+4,value.flags);put_word(p+0x1c,value.user_iosb);put_word(p+0x20,value.user_event);
    put_word(p+0x30,value.user_buffer);put_word(p+0x50,value.thread);return 1;
}
