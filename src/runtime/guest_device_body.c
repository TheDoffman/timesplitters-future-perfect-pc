#include "guest_device_body.h"
#include <string.h>
static void put(unsigned char *p,uint32_t value) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));}
int tsfp_device_body_initialize(void *target,size_t span,uint32_t address,
    uint32_t driver,uint32_t extension,uint32_t type,int named,int exclusive) {
    unsigned char *p=(unsigned char *)target;uint32_t bytes;
    if(!p || !address || !driver || type>UINT8_MAX || extension>UINT16_MAX-TSFP_DEVICE_BODY_BYTES)return 0;
    bytes=TSFP_DEVICE_BODY_BYTES+extension;
    if(span<bytes || (uint64_t)address+bytes-1>UINT32_MAX)return 0;
    memset(p,0,bytes);p[0]=3;p[2]=(unsigned char)bytes;p[3]=(unsigned char)(bytes>>8);
    put(p+8,driver);
    put(p+0x14,UINT32_C(0x10)|(named?UINT32_C(8):0)|(exclusive?UINT32_C(2):0));
    put(p+0x18,extension?address+TSFP_DEVICE_BODY_BYTES:0);p[0x1c]=(unsigned char)type;p[0x1e]=1;
    /* KDEVICE_QUEUE.Type=DeviceQueueObject0x14, Size is bytes. */
    p[0x28]=0x14;p[0x2a]=12;put(p+0x2c,address+0x2c);put(p+0x30,address+0x2c);
    /* Dispatcher header Size is words: sixteen-byte synchronization event. */
    p[0x34]=1;p[0x36]=4;put(p+0x38,1);put(p+0x3c,address+0x3c);put(p+0x40,address+0x3c);
    return 1;
}
int tsfp_device_body_mark_deleted(void *target,size_t span) {
    unsigned char *p=(unsigned char *)target;uint32_t bytes;
    if(!p || span<TSFP_DEVICE_BODY_BYTES || p[0]!=3 || p[1])return 0;
    bytes=(uint32_t)p[2]|((uint32_t)p[3]<<8);
    if(bytes<TSFP_DEVICE_BODY_BYTES || span<bytes)return 0;
    p[0x1f]=1;return 1;
}
