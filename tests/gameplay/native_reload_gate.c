/* Asset-free branch contract. Full original weapon/input integration is in
 * native_reload_input.c and requires the user's supported local executable. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
/* TIMING_HEADER */
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp;
/* GENERATED_GATE */
/* PHASE_SWEEP */
static unsigned held;
static void reset_poll(void) { held=0; }
static unsigned poll_edge(PcNativeStep step,unsigned down)
{
    if(g_tsfp_native_timing_active && !step.raw_ticks) return 0;
    unsigned edge=down && !held; held=down; return edge;
}
static unsigned consume_edge(PcNativeStep step,unsigned edge)
{
    MEM32(0x4e7944)=step.game_ticks; MEMF(0x4e794c)=step.game_delta;
    g_esp=0x1000; reload_gate();
    assert(g_esp==0x1004);
    return edge && g_eax;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    reload_phase_sweep(reset_poll,poll_edge,consume_edge);
    const float deltas[]={0,.25f,60.f/180,60.f/144,.5f,1,3};
    for(int native=0;native<=1;native++) for(unsigned whole=0;whole<=3;whole++)
    for(unsigned i=0;i<sizeof deltas/sizeof deltas[0];i++) {
        g_tsfp_native_timing_active=native;
        MEM32(0x4e7944)=whole;MEMF(0x4e794c)=deltas[i];
        g_esp=0x1000;g_eax=99;reload_gate();
        unsigned accepted=g_eax,expected=native ? deltas[i]>0 : whole>0;
        assert(accepted==expected);
        assert(g_esp==0x1004 && MEM32(0x4e7944)==whole);
        assert(pc_native_global_load(0xa0de8,0x4e7944,whole)==(native ? deltas[i]>0 : whole));
        assert(pc_native_global_load(0xa0de7,0x4e7944,whole)==whole);
        assert(pc_native_global_load(0xa0de8,0x4e7940,whole)==whole);
        /* Neither unrelated instructions nor a different address opt in. */
        assert(pc_native_global_load(0x1c2f08,0x4e7944,whole)==whole);
        assert(pc_native_global_load(0x1c2f09,0x4e7940,whole)==whole);
    }
    free(memory);puts("PASS: generated reload gate, default/native timing, pause and unrelated globals");
    return 0;
}
