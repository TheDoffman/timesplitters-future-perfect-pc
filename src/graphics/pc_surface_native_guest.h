#ifndef PC_SURFACE_NATIVE_GUEST_H
#define PC_SURFACE_NATIVE_GUEST_H
#include "pc_surface_native.h"
#include "pc_native_timing.h"
#include "pc_guest_memory.h"
#include "pc_native_rate.h"
#ifndef PC_SURFACE_WORD
#define PC_SURFACE_WORD(va) (*(uint32_t *)((uintptr_t)(va)+g_xbox_mem_offset))
#endif
#ifndef PC_SURFACE_VALUE
#define PC_SURFACE_VALUE(va) (*(float *)((uintptr_t)(va)+g_xbox_mem_offset))
#endif
#define PC_SURFACE_NATIVE_PARTIAL() pc_surface_native_partial()
extern void hle_d3d8_surface_native_prepare(const PcSurfaceNativeSample *sample);
extern void hle_d3d8_surface_native_finish(void);
extern void hle_d3d8_surface_native_reset(void);
extern void hle_d3d8_surface_native_suspend(void);
static struct { unsigned active,used;uint32_t whole; } pc_surface_native_scope;
static int pc_surface_native_requested(void) {
    static int checked,enabled;
    if(!checked) {
#ifndef PC_SURFACE_NATIVE_SUPPORTED
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
#define PC_SURFACE_NATIVE_SUPPORTED 1
#else
#define PC_SURFACE_NATIVE_SUPPORTED 0
#endif
#endif
        enabled=pc_surface_native_policy(PC_SURFACE_NATIVE_SUPPORTED,getenv("TSFP_SURFACE_NATIVE_TEST"),
            getenv("TSFP_SURFACE_FRACTIONAL_TEST"),getenv("TSFP_SURFACE_FLOAT_TEST"),getenv("TSFP_SURFACE_LIVE_TEST"));
        checked=1;
    }
    return enabled;
}
static int pc_surface_native_partial(void) {
    return pc_surface_native_scope.active && !pc_surface_native_scope.whole;
}
static int pc_surface_native_read(uint32_t address,void *out,unsigned bytes) {
    if(address<0x10000u || address>0x84000000u || bytes>0x84000000u-address)return 0;
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    if(!pc_guest_memory(host,bytes,0))return 0;
    memcpy(out,(void*)host,bytes);return 1;
}
static void pc_surface_native_simulation(void (*original)(void)) {
    if(!pc_surface_native_requested()){original();return;}
    PcNativeStep *step=&g_tsfp_native_step;
    uint32_t whole=PC_SURFACE_WORD(0x4e7944u);
    float delta=PC_SURFACE_VALUE(0x4e794cu);
    if(!g_tsfp_native_timing_active || pc_native_rate()<=60) {
        if(pc_surface_native_scope.used)hle_d3d8_surface_native_suspend();
        pc_surface_native_scope.used=0;original();return;
    }
    if(!(delta>0) || delta!=step->game_delta || whole!=step->game_ticks ||
       step->units_per_tick<=0 || step->game_units<=0){original();return;}
    pc_surface_native_scope.active=pc_surface_native_scope.used=1;
    pc_surface_native_scope.whole=whole;
    PC_SURFACE_WORD(0x4e7944u)=whole?whole:1;
    original();
    PC_SURFACE_WORD(0x4e7944u)=whole;pc_surface_native_scope.active=0;
}
static void pc_surface_native_final(void (*original)(void)) {
    PcSurfaceNativeSample sample={0};int ready=0;
    if(pc_surface_native_scope.active && pc_surface_capture_collect(pc_surface_native_read,g_esi,&sample.feedback)) {
        uint32_t ids,index,table,base,id;
        if(pc_surface_native_read(g_esi+0x20,&ids,4) && pc_surface_native_read(g_esi+0x30,&index,4) &&
           index<4 && ids<=0x83fffff0u && pc_surface_native_read(ids+index*4,&id,4) &&
           pc_surface_native_read(0x4b85c8u,&table,4) && pc_surface_native_read(table,&base,4)) {
            uint64_t va=(uint64_t)base+(uint64_t)id*0x34u;
            if(base>=0x10000u && va<=0x83ffffccu) {
                sample.final_texture[0]=(uint32_t)va;
                ready=pc_surface_native_read((uint32_t)va+4,&sample.final_texture[1],4) &&
                      pc_surface_native_read((uint32_t)va+12,&sample.final_texture[2],8);
            }
        }
        sample.feedback.whole=pc_surface_native_scope.whole;
        sample.frequency=g_tsfp_native_step.units_per_tick;
        sample.start_phase=g_tsfp_native_step.game_phase;sample.units=g_tsfp_native_step.game_units;
    }
    if(ready)hle_d3d8_surface_native_prepare(&sample);
    original();
    if(ready)hle_d3d8_surface_native_finish();
}
static void pc_surface_native_constructor(void (*original)(void)) {
    if(pc_surface_native_requested() && PC_SURFACE_WORD(g_esp+4)==0)hle_d3d8_surface_native_reset();
    original();
}
#endif
