#ifndef PC_NATIVE_SURFACE_GUEST_H
#define PC_NATIVE_SURFACE_GUEST_H
/* Fractional procedural-feedback candidate. The original one-tick path remains
 * exact; disturbances remain discrete while the feedback integrates live time.
 * With original 8-bit feedback targets, 120 loses variation and 144 becomes flat.
 * Keep opt-in for diagnosis; see the rejected-candidate regression record. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "pc_native_rate.h"
#include "pc_surface_bias.h"
extern void hle_d3d8_surface_bias_step(float delta);
extern void hle_d3d8_surface_coefficients(float acceleration,float velocity,float damping);
extern void hle_d3d8_surface_precision_rate(int high_rate);
#ifndef PC_SURFACE_WORD
#define PC_SURFACE_WORD(va) (*(uint32_t *)((uintptr_t)(va)+g_xbox_mem_offset))
#endif
#ifndef PC_SURFACE_VALUE
#define PC_SURFACE_VALUE(va) (*(float *)((uintptr_t)(va)+g_xbox_mem_offset))
#endif
#ifndef PC_SURFACE_NATIVE_PARTIAL
#define PC_SURFACE_NATIVE_PARTIAL() 0
#endif
static struct { int active;float delta;uint32_t whole; } pc_surface_step;
static int pc_surface_fractional_requested(void)
{
    static int checked,enabled;
    if(!checked) { const char *s=getenv("TSFP_SURFACE_FRACTIONAL_TEST");enabled=s && s[0]=='1';checked=1; }
    return enabled;
}
static void pc_surface_simulation_guest(void (*original)(void))
{
    if(pc_surface_live_requested())hle_d3d8_surface_precision_rate(pc_native_rate()>60);
    float delta=PC_SURFACE_VALUE(0x4e794cu);
    if(!pc_surface_fractional_requested() || !g_tsfp_native_timing_active ||
       pc_native_rate()<=60 || !(delta>0 && delta<1)) { original();return; }
    /* Retain the original full-tick/catch-up transaction. Subtick execution
     * requires a positive dispatcher gate, but nested events use saved ticks. */
    uint32_t whole=PC_SURFACE_WORD(0x4e7944u);
    pc_surface_step.active=1;pc_surface_step.delta=delta;pc_surface_step.whole=whole;
    PC_SURFACE_WORD(0x4e7944u)=1;
    original();
    PC_SURFACE_WORD(0x4e7944u)=whole;pc_surface_step.active=0;
}
static void pc_surface_disturbance_guest(void (*original)(void))
{
    if(PC_SURFACE_NATIVE_PARTIAL()){g_esp+=4;return;}
    if(pc_surface_step.active && !pc_surface_step.whole) {g_esp+=4;return;}
    original();
}
static void pc_surface_feedback_guest(void (*original)(void))
{
    if(PC_SURFACE_NATIVE_PARTIAL()){g_esp+=4;return;}
    if(!pc_surface_step.active) {original();return;}
    uint32_t params=PC_SURFACE_WORD(g_esi+0x1cu);
    float acceleration=PC_SURFACE_VALUE(params+0x14u);
    float velocity=PC_SURFACE_VALUE(params+0x18u);
    float damping=PC_SURFACE_VALUE(params+0x1cu);
    float delta=pc_surface_step.delta;
    PC_SURFACE_VALUE(params+0x14u)=acceleration*delta;
    PC_SURFACE_VALUE(params+0x18u)=velocity*delta;
    PC_SURFACE_VALUE(params+0x1cu)=powf(damping,delta);
    int bias=pc_surface_bias_requested();
    if(bias) {
        hle_d3d8_surface_coefficients(acceleration,velocity,damping);
        hle_d3d8_surface_bias_step(delta);
    }
    original();
    if(bias)hle_d3d8_surface_bias_step(-1);
    PC_SURFACE_VALUE(params+0x14u)=acceleration;
    PC_SURFACE_VALUE(params+0x18u)=velocity;
    PC_SURFACE_VALUE(params+0x1cu)=damping;
}
static void pc_surface_smoothing_guest(void (*original)(void))
{
    if(PC_SURFACE_NATIVE_PARTIAL()){g_esp+=4;return;}
    if(!pc_surface_step.active || pc_surface_step.whole) {original();return;}
    /* The caller has just swapped current (+24) and destination (+28), and
     * will swap them again after this call. Cancel that second swap so the
     * feedback reads the latest state directly and writes the other buffer.
     * The shader's former zero-radius "copy" changes floating-point state.
     * Do not dispatch it between authored smoothing events. */
    uint32_t current=PC_SURFACE_WORD(g_esi+0x24u);
    PC_SURFACE_WORD(g_esi+0x24u)=PC_SURFACE_WORD(g_esi+0x28u);
    PC_SURFACE_WORD(g_esi+0x28u)=current;
    g_esp+=4;
}
#endif
