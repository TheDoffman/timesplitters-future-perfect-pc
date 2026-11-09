/* One native device policy for ARM64 and x64 host-fault adapters. */
#ifndef TSFP_MMIO_DEVICE_H
#define TSFP_MMIO_DEVICE_H
#include "platform/win32_compat.h"
#include "apu/apu.h"
#include "guest_arena_service.h"
#include <string.h>
extern ptrdiff_t g_xbox_mem_offset;
static inline int tsfp_mmio_device(uintptr_t host,unsigned bytes,int load,uint64_t *value) {
    if(!value||!bytes||bytes>8||host<(uintptr_t)g_xbox_mem_offset||host-(uintptr_t)g_xbox_mem_offset>UINT32_MAX)return 0;
    uint32_t va=(uint32_t)(host-(uintptr_t)g_xbox_mem_offset);
    int apu=va>=0xfe800000u&&va<0xfe830000u;
    int crtc=va>=0xfd600000u&&va<0xfd604000u;
    int ac97=va>=0xfec00000u&&va<0xfec04000u;
    if(!apu&&!crtc&&!ac97)return 0;
    void *alias=tsfp_guest_mmio_alias((void *)host,bytes);if(!alias)return 0;
    if(apu) {
        if(!g_apu_state||bytes>4||va+bytes>0xfe830000u)return 0;
        if(load)*value=mcpx_apu_mmio_read(g_apu_state,va-0xfe800000u,bytes);
        else mcpx_apu_mmio_write(g_apu_state,va-0xfe800000u,*value,bytes);
    } else if(load){*value=0;memcpy(value,alias,bytes);}
    else if(crtc&&va==0xfd600100u) {
        if(bytes!=4)return 0;
        uint32_t *pmc=tsfp_guest_mmio_alias((void *)((uintptr_t)g_xbox_mem_offset+0xfd000100u),4);
        if(!pmc)return 0;
        uint32_t remaining=__atomic_fetch_and((uint32_t *)alias,~(uint32_t)*value,__ATOMIC_SEQ_CST)&~(uint32_t)*value;
        if(remaining)__atomic_fetch_or(pmc,0x01000000u,__ATOMIC_SEQ_CST);
        else __atomic_fetch_and(pmc,~0x01000000u,__ATOMIC_SEQ_CST);
    } else {
        if(ac97&&(va==0xfec0011bu||va==0xfec0017bu))*value&=~UINT64_C(2);
        memcpy(alias,value,bytes);
    }
    return 1;
}
#endif
