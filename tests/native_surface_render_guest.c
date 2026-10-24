/* Real guest adapters, with a synthetic mapped address space and call spies. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned char memory[0x800000];
static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp,g_esi;
static int g_tsfp_native_timing_active=1,rate=144;
#define TSFP_PC_NATIVE_RATE_H
static int pc_native_rate(void){return rate;}
#define TSFP_PC_GUEST_MEMORY_H
static int pc_guest_memory(uintptr_t p,size_t size,int write) {
    (void)write;return p>=(uintptr_t)memory && size<=sizeof memory && p-(uintptr_t)memory<=sizeof memory-size;
}
static char *test_getenv(const char *name){return !strcmp(name,"TSFP_SURFACE_NATIVE_TEST")?"1":NULL;}
#define getenv test_getenv
#include "pc_surface_native_guest.h"
#include "pc_native_surface_guest.h"
#include "pc_surface_capture_guest.h"
PcNativeStep g_tsfp_native_step;
static unsigned events,feedback,smoothing,finals,prepares,finishes,resets,captures;
static PcSurfaceNativeSample submitted;
void hle_d3d8_surface_native_prepare(const PcSurfaceNativeSample *s){submitted=*s;prepares++;}
void hle_d3d8_surface_native_finish(void){finishes++;}
void hle_d3d8_surface_native_reset(void){resets++;}
void hle_d3d8_surface_capture(const PcSurfaceCapture *s){(void)s;captures++;}
void hle_d3d8_surface_bias_step(float d){(void)d;assert(0);}
void hle_d3d8_surface_coefficients(float a,float b,float d){(void)a;(void)b;(void)d;assert(0);}
void hle_d3d8_surface_precision_rate(int r){(void)r;assert(0);}
static void event(void){events++;g_esp+=4;}
static void step(void){feedback++;g_esp+=4;}
static void smooth(void){smoothing++;g_esp+=4;}
static void final(void){finals++;g_esp+=4;}
static void dispatch(void) {
    if(!PC_SURFACE_WORD(0x4e7944u)){g_esp+=4;return;}
    uint32_t stack=g_esp;
    g_esp-=4;pc_surface_disturbance_guest(event);assert(g_esp==stack);
    g_esp-=4;pc_surface_disturbance_guest(event);assert(g_esp==stack);
    g_esp-=4;pc_surface_smoothing_guest(smooth);assert(g_esp==stack);
    g_esp-=4;pc_surface_capture_feedback_guest(step);assert(g_esp==stack);
    g_esp-=4;pc_surface_native_final(final);assert(g_esp==stack);
    g_esp+=4;
}
int main(void) {
    g_xbox_mem_offset=(uintptr_t)memory;g_esi=0x10000;
    PC_SURFACE_WORD(g_esi+0x1c)=0x11000;PC_SURFACE_WORD(g_esi+0x20)=0x12000;
    PC_SURFACE_WORD(g_esi+0x24)=0;PC_SURFACE_WORD(g_esi+0x28)=1;
    PC_SURFACE_WORD(g_esi+0x2c)=2;PC_SURFACE_WORD(g_esi+0x30)=3;
    PC_SURFACE_WORD(0x4b85c8u)=0x13000;PC_SURFACE_WORD(0x13000)=0x14000;
    for(unsigned i=0;i<4;i++)PC_SURFACE_WORD(0x12000+i*4)=i;
    PcNativeTiming clock={0};pc_native_timing_step(&clock,0,72000000,144,0);
    unsigned ticks=0;
    for(unsigned frame=1;frame<=144;frame++) {
        g_tsfp_native_step=pc_native_timing_step(&clock,(int64_t)frame*500000,72000000,144,0);
        ticks+=g_tsfp_native_step.game_ticks;
        PC_SURFACE_WORD(0x4e7944u)=g_tsfp_native_step.game_ticks;
        PC_SURFACE_VALUE(0x4e794cu)=g_tsfp_native_step.game_delta;
        g_esp=0x18000;pc_surface_native_simulation(dispatch);
        assert(g_esp==0x18004 && PC_SURFACE_WORD(0x4e7944u)==g_tsfp_native_step.game_ticks);
        assert(submitted.feedback.whole==g_tsfp_native_step.game_ticks);
    }
    assert(ticks==60 && feedback==60 && smoothing==60 && events==120);
    assert(finals==144 && prepares==144 && finishes==144 && !captures);
    unsigned before=prepares;PC_SURFACE_VALUE(0x4e794cu)=0;PC_SURFACE_WORD(0x4e7944u)=0;
    g_esp=0x18000;pc_surface_native_simulation(dispatch);assert(prepares==before);
    rate=60;pc_surface_native_simulation(dispatch);assert(resets==1);
    pc_surface_native_simulation(dispatch);assert(resets==1);
    puts("PASS: native water keeps authored events, ABI, pause and Original60 recovery");
    return 0;
}
