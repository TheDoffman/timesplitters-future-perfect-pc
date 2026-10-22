/* Original US camera-track branch, isolated at its enclosing switch exit.
 * Synthetic keyframe decoding supplies a linear path; orientation, sector,
 * projection and player-input services are stubs. This checks branch timing
 * and sample selection, not actual movie assets or full camera rendering. */
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
enum { CAMERA=0x1000, TRACK=0x3000, TABLE=0x4000, PLAYER=0x5000, STACK=0x7000 };
static unsigned reads, completed, samples[2];
static float spacing;
void sub_00059110(void)
{
    unsigned index=MEM32(g_esp+12),out=MEM32(g_esp+16);
    assert(MEM32(g_esp+4)==TRACK && index<=120);
    samples[reads++%2]=index;
    memset((void *)((uintptr_t)out+g_xbox_mem_offset),0,32);
    MEMF(out)=index*spacing; MEMF(out+28)=1;
    g_esp+=4;
}
void sub_00150960(void) { g_esp+=4; }
void sub_001506B0(void)
{
    unsigned out=MEM32(g_esp+4);
    memset((void *)((uintptr_t)out+g_xbox_mem_offset),0,24);
    MEMF(out+8)=1; MEMF(out+20)=1; g_esp+=4;
}
void sub_000C3C50(void) { g_eax=7; g_esp+=4; }
void sub_0003C420(void) { g_eax=0; g_esp+=4; }
void sub_000C77C0(void) { g_esp+=4; }
void sub_000427B0(void) { g_eax=0; g_esp+=4; }
void sub_000CB3B0(void)
{ g_fp_top=(g_fp_top+7)&7; g_fp_stack[g_fp_top]=60; g_esp+=4; }
void sub_000CBD10(void) { ++completed; g_esp+=4; }
/* GENERATED_ROUTINES */
static void setup(void)
{
    memset((void *)((uintptr_t)CAMERA+g_xbox_mem_offset),0,0x1000);
    MEM32(0x7b0cc0)=CAMERA; MEM32(0x7b0c7c)=PLAYER;
    MEM32(0x7e3954)=TABLE; MEM16(0x7e03c0)=1;
    MEM32(TABLE+4)=TRACK; MEM32(TRACK+16)=121; MEM8(TRACK+8)=0;
    MEM32(0x7de668)=0; MEM32(CAMERA+0x21c)=0x1000000;
    MEM32(0x4e8cec)=60;
    MEM32(0x4c045c)=480; MEM32(0x4e8cf4)=480;
    MEM32(0x4c0458)=640; MEM32(0x4e8cf0)=640;
    MEMF(0x475c78)=1; MEMF(0x4d1994)=60;
    MEMF(0x475d24)=.1f; MEMF(0x475cd4)=.5f;
    reads=completed=0; spacing=.001f;
}
static void step(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole; MEMF(0x4e794c)=delta;
    g_esp=STACK; MEM32(STACK)=0x12345678; g_esi=CAMERA;
    g_fp_top=0;
    sub_000CD691();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(void)
{
    void *memory=calloc(1,0x800000);
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    const unsigned rates[]={60,120,144};
    for(unsigned r=0;r<3;r++) for(unsigned slow=1;slow<=4;slow+=3) {
        unsigned fps=rates[r]; setup(); g_tsfp_native_timing_active=1;
        for(unsigned f=0;f<fps;f++) {
            float time=(f+1)/(float)(fps*slow);
            unsigned whole=(f+1)*60/(fps*slow)-f*60/(fps*slow);
            step(whole,60.f/(fps*slow));
            assert(fabsf(MEMF(CAMERA+0xfd4)-time)<.00001f);
            assert(fabsf(MEMF(CAMERA+0x27c)-time*.06f)<.000001f);
            assert(reads==2*(f+1));
            assert(samples[1]==samples[0]+1);
        }
        float position=MEMF(CAMERA+0x27c),time=MEMF(CAMERA+0xfd4);
        step(0,0);
        assert(reads==2*fps && MEMF(CAMERA+0x27c)==position && MEMF(CAMERA+0xfd4)==time);
    }
    setup(); g_tsfp_native_timing_active=1;
    MEMF(CAMERA+0xfd4)=.005f; step(3,3);
    assert(samples[0]==3 && samples[1]==6);
    assert(fabsf(MEMF(CAMERA+0x27c)-.0033f)<.000001f);
    setup(); MEMF(CAMERA+0xfd4)=.005f; step(2,2.5f);
    assert(samples[0]==2 && samples[1]==4);
    assert(fabsf(MEMF(CAMERA+0x27c)-.0028f)<.000001f);
    setup(); MEMF(CAMERA+0xfd4)=.005f; step(2,1.8f);
    assert(samples[0]==2 && samples[1]==4);
    assert(fabsf(MEMF(CAMERA+0x27c)-.0021f)<.000001f);
    /* A synchronized track changes pose only when its external time changes. */
    setup(); MEM8(TRACK+8)=2; MEM32(0x70e164)=0x6000; MEMF(0x6008)=.3f;
    step(0,.25f); assert(reads==2 && fabsf(MEMF(CAMERA+0x27c)-.018f)<.000001f);
    step(0,.25f); assert(reads==2);
    MEMF(0x6008)=.31f; step(0,.25f); assert(reads==4);
    /* Retain original end-of-track and loop behavior. */
    setup(); MEMF(CAMERA+0xfd4)=1.999f; step(0,.25f);
    assert(completed==1 && MEMF(CAMERA+0xfd4)==2 && (MEM32(CAMERA+0x21c)&0x200000));
    setup(); MEM32(0x7de668)=1; MEMF(CAMERA+0xfd4)=1.999f; step(0,.25f);
    assert(completed==0 && reads==2 && MEMF(CAMERA+0xfd4)==0);
    /* Large keyframe discontinuities keep the original nearest-side cut. */
    setup(); spacing=1; step(0,.25f);
    assert(reads==2 && MEMF(CAMERA+0x27c)==0);
    step(1,.5f);
    assert(reads==4 && MEMF(CAMERA+0x27c)==1);
    /* Original mode still uses its whole-tick gate and sample span. */
    setup(); g_tsfp_native_timing_active=0; step(0,.25f); assert(reads==0);
    step(1,.75f); assert(reads==2 && samples[1]==samples[0]+1);
    assert(RECOMP_LOAD_GLOBAL_U32(0xcd851,0x4e7944,0)==0);
    assert(RECOMP_F32_DIV_AT(0xcd88e,.5f,2.5f)==.5f/2.5f);
    g_tsfp_native_timing_active=1;
    assert(RECOMP_LOAD_GLOBAL_U32(0xcd852,0x4e7944,0)==0);
    assert(RECOMP_F32_DIV_AT(0xcd88f,.5f,2.5f)==.5f/2.5f);
    free(memory);
    puts("PASS: camera-track fractional poses, pause, external clock, catch-up and endpoints");
    return 0;
}
