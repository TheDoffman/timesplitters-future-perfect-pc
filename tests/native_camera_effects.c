/* Original US camera-effect/collision routine and its real matrix helpers.
 * The caller is its original gate/call slice, terminated at the join by RET.
 * World collision services return an unobstructed path. This does not test
 * real geometry, the complete caller, input delivery or every camera mode.
 * The harness supplies a fresh base pose before each effect evaluation. */
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
RECOMP_TLS int g_fp_top,g_fp_cmp;
enum { CAMERA=0x1000, STACK=0x7000 };
static unsigned moves;
void sub_00088C60(void) { g_eax=0; g_esp+=4; }
void sub_00088430(void)
{
    assert(MEM32(g_esp+4)==CAMERA+0x27c);
    ++moves; g_eax=7; g_esp+=4;
}
/* GENERATED_ROUTINES */
static void dispatch(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole; MEMF(0x4e794c)=delta;
    MEMF(CAMERA+0x27c)=MEMF(CAMERA+0x280)=MEMF(CAMERA+0x284)=0;
    g_esp=STACK; MEM32(STACK)=0x12345678;
    MEMF(STACK+0x14)=0; MEMF(STACK+0x18)=0; MEMF(STACK+0x1c)=1;
    g_ebx=0xdead; g_ebp=0xbeef; g_esi=0xdef; g_edi=0xface;
    sub_000CEA8D();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    /* g_ebp is the recompiler's last-published frame cache, not a global
     * architectural EBP. The real matrix helpers own local saved frames. */
    int active=g_tsfp_native_timing_active?delta>0:whole>0;
    assert(g_ebx==0xdead && g_edi==0xface && g_fp_top==0);
    assert(g_esi==(active?STACK+0x14:0xdef));
    /* A live effect must be applied to each fresh base pose, not pop out
     * on zero-whole-tick frames. The constant supplied offset is 0.1. */
    if(!(MEM32(CAMERA+0x21c)&0x80))
        assert(fabsf(MEMF(CAMERA+0x27c)-(active?.1f:0))<.000001f);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void setup(void)
{
    memset((void *)((uintptr_t)CAMERA+g_xbox_mem_offset),0,0x1000);
    moves=0;
    MEMF(CAMERA+0x33c)=1; MEMF(CAMERA+0x398)=2;
    /* These are the original sub_000C7910 caller's kick/factor arguments. */
    MEMF(CAMERA+0x3a4)=.03f; MEMF(CAMERA+0x3a8)=.8f;
    MEMF(CAMERA+0x3ac)=.2f; MEMF(CAMERA+0x3b0)=.8f;
    MEMF(CAMERA+0x3b8)=.1f; MEMF(0x7330a0)=1;
    MEMF(0x4dd350)=MEMF(0x4dd354)=MEMF(0x4dd358)=0;
}
static float decayed(float initial,float ticks)
{
    float value=initial*powf(.8f,ticks);
    return value<.01f?0:value;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x7b0cc0)=CAMERA; MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<4;i++) MEMF(0x4e7900+i*20)=1;
    MEMF(0x475c78)=1; MEMF(0x475cd4)=.5f;
    MEMF(0x475d0c)=.01745329238474369f; MEMF(0x475ce0)=325.9493103027344f;
    MEMF(0x475d10)=2; MEMF(0x475d1c)=.01f;
    MEMF(0x4765f4)=1.0000000116860974e-7f; MEMF(0x4788e8)=.55f;
    MEMF(0x47d618)=2280; MEMF(0x47d5a4)=1800; MEMF(0x47d614)=1940;
    const unsigned rates[]={60,120,144};
    for(unsigned r=0;r<3;r++) for(int native=0;native<2;native++) {
        unsigned fps=rates[r]; g_tsfp_native_timing_active=native; setup();
        for(unsigned f=0;f<fps;f++) {
            unsigned whole=(f+1)*60/fps-f*60/fps;
            dispatch(whole,60.0f/fps);
            unsigned ticks=(f+1)*60/fps;
            float decay_ticks=native?(f+1)*60.0f/fps:(float)ticks;
            float seconds=native?(f+1)/(float)fps:ticks/(float)fps;
            assert(fabsf(MEMF(CAMERA+0x398)-(2-seconds))<.00002f);
            assert(fabsf(MEMF(CAMERA+0x3a4)-decayed(.03f,decay_ticks))<.000001f);
            assert(fabsf(MEMF(CAMERA+0x3ac)-decayed(.2f,decay_ticks))<.000001f);
            assert(moves==(native?f+1:ticks));
        }
        float before=MEMF(CAMERA+0x398),phase=MEMF(0x4dd350);
        unsigned old_moves=moves;
        dispatch(0,0);
        assert(MEMF(CAMERA+0x398)==before && MEMF(0x4dd350)==phase && moves==old_moves);
    }
    g_tsfp_native_timing_active=1;
    for(unsigned r=0;r<3;r++) {
        unsigned fps=rates[r]; setup();
        for(unsigned f=0;f<fps;f++)
            dispatch((f+1)*15/fps-f*15/fps,15.0f/fps);
        assert(fabsf(MEMF(CAMERA+0x398)-1.75f)<.00002f && moves==fps);
        assert(fabsf(MEMF(0x4dd350)-570)<.01f);
    }
    setup(); dispatch(0,.25f);
    assert(fabsf(MEMF(CAMERA+0x3a4)-decayed(.03f,.25f))<.000001f);
    setup(); dispatch(3,3);
    assert(fabsf(MEMF(CAMERA+0x3a4)-decayed(.03f,3))<.000001f);
    assert(fabsf(MEMF(CAMERA+0x3ac)-decayed(.2f,3))<.000001f);
    assert(fabsf(MEMF(CAMERA+0x398)-1.95f)<.000001f && moves==1);
    /* The original flag-driven reset runs on live time, but not pause. */
    setup(); MEM32(CAMERA+0x21c)=0x80; dispatch(0,0);
    assert(MEMF(CAMERA+0x3a4)==.03f);
    dispatch(0,.25f);
    assert(MEMF(CAMERA+0x3a4)==0 && MEMF(CAMERA+0x3ac)==0 && MEMF(CAMERA+0x398)==0);
    assert(RECOMP_LOAD_GLOBAL_U32(0xc7e0d,0x4e7944,3)==3);
    assert(RECOMP_F32_MUL_AT(0xc7da9,.8f,.03f)==.8f*.03f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0xcea8d,0x4e7944,3)==3);
    assert(RECOMP_F32_MUL_AT(0xc7da8,.8f,.03f)==.8f*.03f);
    free(memory);
    puts("PASS: original camera-effect timer, collision cadence and kick decay");
    return 0;
}
