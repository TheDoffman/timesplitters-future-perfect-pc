/* Original US NPC dispatcher: verify its real elapsed-time arithmetic and
 * movement callback cadence. Asset/world/state helpers are isolated stubs. */
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
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4;
static unsigned movements;
#define STUB(name) void name(void) { g_eax=0; g_esp+=4; }
STUB(sub_001260C0)
STUB(sub_00158CB0)
STUB(sub_001CB260)
STUB(sub_0015C6C0)
STUB(sub_000D84C0)
STUB(sub_0009A420)
STUB(sub_0009BCF0)
STUB(sub_00017510)
STUB(sub_000174E0)
STUB(sub_00238510)
STUB(sub_0027EFB0)
STUB(sub_001213D0)
STUB(sub_0015C700)
STUB(sub_00275620)
void sub_00121BA0(void) { g_eax=1; g_esp+=4; }
void sub_0027FE30(void)
{
    assert(MEM32(g_esp+4)==0x2000);
    movements++; g_esp+=4;
}
#define UNUSED(name) void name(void) { assert(!"unexpected NPC path"); g_esp+=4; }
UNUSED(sub_003C8960)
UNUSED(sub_000DAAE0)
UNUSED(sub_0027E980)
UNUSED(sub_00058C70)
UNUSED(sub_00012F00)
UNUSED(sub_0009A590)
UNUSED(sub_0009A570)
UNUSED(sub_00017570)
UNUSED(sub_000D6C90)
UNUSED(sub_00275CC0)
/* GENERATED_ROUTINES */
static void dispatch(unsigned whole, float delta)
{
    MEM32(0x4e7944)=whole; MEMF(0x4e794c)=delta;
    g_esp=0x7000; MEM32(g_esp)=0x12345678; MEM32(g_esp+4)=0x2000;
    g_ebx=0xabc; g_esi=0xdef; g_edi=0x123; g_ebp=0x456;
    sub_0015EAE0();
    assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678);
    assert(g_ebx==0xabc && g_esi==0xdef && g_edi==0x123 && g_ebp==0x456);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144};
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x7a2958)=1; MEM32(0x207c)=0x3000;
    MEM32(0x4e8cec)=60; MEMF(0x475c78)=1;
    for(unsigned r=0;r<3;r++) for(int native=0;native<2;native++) {
        unsigned fps=rates[r];
        memset(memory+0x3000,0,0x1000);
        MEM32(0x3018)=1;
        movements=0; g_tsfp_native_timing_active=native;
        for(unsigned frame=0;frame<fps;frame++)
            dispatch((frame+1)*60/fps-frame*60/fps,60.0f/fps);
        assert(movements==(native ? fps : 60));
        /* The original routine increments these real AI timers by delta/60.
         * Its old gate loses fractional intervals at high refresh. */
        float expected=native ? 1.0f : 60.0f/fps;
        assert(fabsf(MEMF(0x39cc)-expected)<.00001f);
        assert(fabsf(MEMF(0x3b30)-expected)<.00001f);
        float before=MEMF(0x39cc); unsigned calls=movements;
        dispatch(0,0); assert(MEMF(0x39cc)==before && movements==calls);
        if(native) {
            dispatch(0,.25f); assert(movements==calls+1);
            assert(fabsf(MEMF(0x39cc)-before-.25f/60)<.00001f);
            dispatch(3,3); /* bounded catch-up is one callback with full dt */
            assert(movements==calls+2);
            assert(fabsf(MEMF(0x39cc)-before-3.25f/60)<.00001f);
        }
    }
    assert(RECOMP_LOAD_GLOBAL_U32(0x15eb0d,0x4e7944,7)==7);
    assert(RECOMP_LOAD_GLOBAL_U32(0x15eb0c,0x1234,7)==7);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0x15eb0c,0x4e7944,3)==3);
    free(memory);
    puts("PASS: original NPC timers and movement cadence preserve elapsed time at 60/120/144");
    return 0;
}
