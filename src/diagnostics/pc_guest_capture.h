#ifndef TSFP_PC_GUEST_CAPTURE_H
#define TSFP_PC_GUEST_CAPTURE_H
/* Shared bounded guest-memory report policy. Readers must return exactly the
 * requested bytes or fail; platform adapters never dereference remote pointers. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define PC_GUEST_CAPTURE_LIMIT (1024u*1024u)
typedef int (*PcGuestRead)(void *,uint32_t,void *,size_t);
static inline int pc_guest_capture_span(uint32_t address,size_t bytes) {
    return bytes && bytes<=65536u &&
        ((address>=0x10000u && (uint64_t)address+bytes<=0x04000000u) ||
         (address>=0x80000000u && (uint64_t)address+bytes<=0x84000000u));
}
/* Numeric, versioned little-endian records; no native struct packing on disk. */
static inline int pc_guest_capture_u32(FILE *f,uint32_t v) {
    unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};
    return fwrite(b,1,4,f)==4;
}
static inline int pc_guest_capture_region(FILE *f,PcGuestRead read,void *context,
                                         uint32_t kind,uint32_t address,uint32_t bytes) {
    unsigned char data[65536];
    int valid=pc_guest_capture_span(address,bytes)&&read(context,address,data,bytes);
    if(!pc_guest_capture_u32(f,kind)||!pc_guest_capture_u32(f,address)||
       !pc_guest_capture_u32(f,bytes)||!pc_guest_capture_u32(f,valid?bytes:0))return 0;
    return !valid || fwrite(data,1,bytes,f)==bytes;
}
/* Registers: eax,ebx,ecx,edx,esi,edi,esp,ebp. Bound to eight pages plus
 * 64 KiB of guest stack and a candidate model's 128 signed-index nodes.
 * Candidate model records are evidence, not a claim that EDI is always a model. */
static inline int pc_guest_capture_write_mode(FILE *f,PcGuestRead read,void *context,const uint32_t r[8],int extended) {
    if(fwrite("TSFPGST1",1,8,f)!=8)return 0;
    for(unsigned i=0;i<8;i++)if(!pc_guest_capture_region(f,read,context,1+i,r[i]&~4095u,extended?65536:4096))return 0;
    if(!pc_guest_capture_region(f,read,context,9,r[6],65536))return 0;
    uint32_t head=0,index=0;unsigned char word[4];
    if(r[5]<=UINT32_MAX-4 && pc_guest_capture_span(r[5]+4,4)&&read(context,r[5]+4,word,4)) {
        head=(uint32_t)word[0]|(uint32_t)word[1]<<8|(uint32_t)word[2]<<16|(uint32_t)word[3]<<24;
        if(pc_guest_capture_span(head,4)&&read(context,head,word,4)) {
            index=(uint32_t)word[0]|(uint32_t)word[1]<<8|(uint32_t)word[2]<<16|(uint32_t)word[3]<<24;
            if(index<128 && head>=index*200u)
                if(!pc_guest_capture_region(f,read,context,10,head-index*200u,128u*200u))return 0;
        }
    }
    return !ferror(f);
}
static inline int pc_guest_capture_write(FILE *f,PcGuestRead read,void *context,const uint32_t r[8]) { return pc_guest_capture_write_mode(f,read,context,r,0); }
#endif
