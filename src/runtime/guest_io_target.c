#include "guest_io_target.h"
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int tsfp_io_target_prepare(const void *packet,size_t span,uint32_t request,
    const void *device,size_t device_span,uint32_t body,
    const void *driver,size_t driver_span,uint32_t address,tsfp_io_target *out) {
    const unsigned char *p=(const unsigned char *)packet,*d=(const unsigned char *)device,*table=(const unsigned char *)driver;
    tsfp_io_target candidate;uint32_t stack;size_t offset;
    if(!out || !d || !table || device_span<72 || driver_span<68 || !body || body>UINT32_MAX-71 || !address || address>UINT32_MAX-67)return 0;
    if(word(d+8)!=address || !tsfp_io_request_peek(p,span,request,&stack))return 0;
    offset=(size_t)(stack-request);
    if(offset>span || span-offset<TSFP_IO_STACK_BYTES)return 0;
    candidate.major=p[offset];if(candidate.major>=14)return 0;
    candidate.routine=word(table+12+4*candidate.major);if(!candidate.routine)return 0;
    candidate.device=body;candidate.request=request;candidate.stack=stack;*out=candidate;return 1;
}
