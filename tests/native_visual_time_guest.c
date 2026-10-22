/* Actual pinned US direct clock stores, isolated with appended RETs.
 * The enclosing state-machine/replay/network paths are not simulated here. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
/* VISUAL_TIME_POLICY */
/* GENERATED_ROUTINES */
static void store(void (*function)(void),uint32_t previous,uint32_t value,
                  uint32_t whole,float delta) {
    MEM32(0x7de318)=previous;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_eax=g_ebx=g_edx=g_esi=value;g_ecx=123;g_edi=456;g_ebp=789;
    g_esp=0x10000;MEM32(g_esp)=0;
    function();
    assert(g_esp==0x10004 && !g_fp_top);
    assert(g_eax==value && g_ebx==value && g_edx==value && g_esi==value);
    assert(g_ecx==123 && g_edi==456 && g_ebp==789);
    assert(MEM32(0x7de318)==value && MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static float sample(uint32_t value) {
    return RECOMP_I32_TO_F32_AT(0xf9726,0x7de318,value);
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    void (*increments[])(void)={sub_000688BB,sub_00068F8E,sub_000691F3,
                                sub_00069239,sub_00069265,sub_00069325};
    void (*rebases[])(void)={sub_000670E8,sub_00043595,sub_00357939};
    for(unsigned native=0;native<2;native++) {
        g_tsfp_native_timing_active=native;
        for(unsigned i=0;i<6;i++)for(unsigned j=0;j<3;j++) {
            store(rebases[j],0,100,0,0);
            store(increments[i],100,100,0,.25f);
            assert(sample(100)==(native?100.25f:100.f));
            assert(RECOMP_I32_TO_F32_AT(0xf9727,0x7de318,100)==100);
            assert(RECOMP_I32_TO_F64(0x7de318,100)==100);
            store(increments[i],100,100,0,0);
            assert(sample(100)==(native?100.25f:100.f));
            store(increments[i],100,103,3,2.75f);
            assert(sample(103)==103);
            store(increments[i],103,103,0,.5f);
            assert(sample(103)==(native?103.5f:103.f));
            /* Reset the fractional phase even for a same-value override. */
            store(rebases[j],103,103,0,0);assert(sample(103)==103);
            store(rebases[j],103,0,0,0);assert(sample(0)==0);
        }
    }
    /* Unknown direct stores rebase; unrelated stores never call the policy. */
    MEM32(0x7de318)=pc_native_visual_store(0x12345,10);assert(sample(10)==10);
    assert(RECOMP_STORE_GLOBAL_U32(0x688bb,0x7de31c,27)==27);
    assert(sample(10)==10);
    puts("PASS: original direct clock-store ABI, fractional increments and same-value rebases");
    free(memory);return 0;
}
