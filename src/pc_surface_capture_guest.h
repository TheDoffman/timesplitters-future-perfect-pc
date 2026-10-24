#ifndef PC_SURFACE_CAPTURE_GUEST_H
#define PC_SURFACE_CAPTURE_GUEST_H
#include "pc_surface_capture.h"
#include "pc_guest_memory.h"
extern void hle_d3d8_surface_capture(const PcSurfaceCapture *sample);
static int pc_surface_capture_read(uint32_t address,void *out,unsigned bytes) {
    if(address<0x10000u || address>0x84000000u || bytes>0x84000000u-address)return 0;
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    if(!pc_guest_memory(host,bytes,0))return 0;
    memcpy(out,(const void*)host,bytes);return 1;
}
static void pc_surface_capture_feedback_guest(void (*original)(void)) {
    static int checked,enabled;
    if(!checked) {
        const char *path=getenv("TSFP_SURFACE_CAPTURE");
        enabled=path && *path;
        /* Capture the original feedback, including native mode's whole-tick
         * path. Older rejected fractional/float experiments are incompatible. */
        if(pc_surface_fractional_requested() || pc_surface_live_requested())enabled=0;
        const char *old=getenv("TSFP_SURFACE_FLOAT_TEST");
        if(old && old[0]=='1')enabled=0;
        checked=1;
    }
    PcSurfaceCapture sample;int ready=enabled && pc_surface_capture_collect(pc_surface_capture_read,g_esi,&sample);
    pc_surface_feedback_guest(original);
    if(ready && sample.scene==4 && sample.level==7 && sample.level_ticks>=300)
        hle_d3d8_surface_capture(&sample);
}
#endif
