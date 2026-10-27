/* Run the original shared impact-spark updater. Its eight segment positions
 * must advance on native fractional frames, including newly spawned sparks. */
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
enum { FX=0x1000, PAYLOAD=0x2000, STACK=0x30000 };
static unsigned transformed;
void sub_00151650(void) {
    uint32_t out=MEM32(g_esp+4);
    assert(out>=PAYLOAD && out<PAYLOAD+0x200);
    MEMF(out)=MEMF(g_esp+8);
    MEMF(out+4)=MEMF(g_esp+12);
    MEMF(out+8)=MEMF(g_esp+16);
    transformed++;
    g_esp+=4;
}
/* GENERATED_ROUTINES */
static float run(int native,float delta,unsigned whole) {
    memset((void *)((uintptr_t)FX+g_xbox_mem_offset),0,0x100);
    memset((void *)((uintptr_t)PAYLOAD+g_xbox_mem_offset),0,0x900);
    MEM32(FX+4)=0;
    MEM32(FX+0x18)=PAYLOAD;
    MEM32(FX+0x1c)=0;
    MEMF(FX+0x20)=10.f;
    for(unsigned i=0;i<8;i++)MEMF(PAYLOAD+0x500+i*12+0x60)=1.f;
    MEM32(0x4e8cec)=60;
    MEM32(0x4e7944)=whole;
    MEMF(0x4e794c)=delta;
    MEMF(0x73bba8)=delta;
    g_tsfp_native_timing_active=native;
    g_esi=FX;g_esp=STACK;g_seh_ebp=0;
    MEM32(STACK)=0x12345678;
    transformed=0;
    sub_00108780();
    assert(g_esp==STACK+4);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    assert(transformed==(delta>0.f && (whole||native) ? 8u:0u));
    return MEMF(PAYLOAD+0x500);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    assert(run(0,1.f/3.f,0)==0.f);
    float reference=run(0,1.f,1);
    assert(reference>0.f && isfinite(reference));
    for(int fps=120;fps<=240;fps+=60) {
        float step=60.f/fps;
        float advanced=run(1,step,0);
        assert(advanced>0.f && isfinite(advanced));
        assert(run(1,0.f,0)==0.f);
        printf("impact_sparks fps=%d fractional=%.6f whole=%.6f\n",fps,advanced,reference);
    }
    puts("PASS: original shared impact sparks advance on native fractional frames and freeze when paused");
    free(memory);return 0;
}
