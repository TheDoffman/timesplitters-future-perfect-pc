/* Execute the original US scripted-camera translation and direction easing.
 * Only the collision/sector service is stubbed; all camera math is original.
 * This tests fixed targets, not full camera modes or moving-target tracking. */
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
enum { CAMERA=0x1000, TARGET=0x4000, STACK=0x7000 };
static unsigned collisions;
void sub_000888A0(void)
{
    assert(MEM32(g_esp+4)==CAMERA+0x27c);
    assert(MEM32(g_esp+16)==0);
    ++collisions; g_eax=7; g_esp+=4;
}
/* GENERATED_ROUTINES */
static void time_step(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole; MEMF(0x4e794c)=delta;
    g_esp=STACK; MEM32(STACK)=0x12345678;
    g_ebx=0xdead; g_ebp=0xbeef;
}
static void returned(unsigned whole,float delta)
{
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(g_ebx==0xdead && g_ebp==0xbeef);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    assert(g_fp_top==0);
}
static void translate(unsigned whole,float delta)
{
    time_step(whole,delta); g_esi=CAMERA; g_edi=TARGET;
    sub_000C73C0(); returned(whole,delta);
    assert(g_esi==CAMERA && g_edi==TARGET);
}
static void orient(unsigned whole,float delta,int remaining)
{
    time_step(whole,delta); g_eax=TARGET; g_ecx=(uint32_t)remaining;
    sub_000C7610(); returned(whole,delta);
}
static void vector(uint32_t address,float yaw,float pitch)
{
    MEMF(address)=sinf(yaw)*cosf(pitch);
    MEMF(address+4)=sinf(pitch);
    MEMF(address+8)=cosf(yaw)*cosf(pitch);
}
static void direction(float yaw,float pitch)
{
    assert(fabsf(atan2f(MEMF(CAMERA+0x334),MEMF(CAMERA+0x33c))-yaw)<.00005f);
    assert(fabsf(asinf(MEMF(CAMERA+0x338))-pitch)<.00005f);
    float length=0;
    for(unsigned a=CAMERA+0x334;a<=CAMERA+0x33c;a+=4)
        length+=MEMF(a)*MEMF(a);
    assert(fabsf(length-1)<.00001f);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x7b0cc0)=CAMERA; MEM32(0x4e8cec)=60;
    MEMF(0x475c78)=1; MEMF(0x4788fc)=15; MEMF(0x47cff4)=30;
    MEMF(0x475c98)=-3.1415927410125732f;
    MEMF(0x475c94)=6.2831854820251465f;
    MEMF(0x475c90)=3.1415927410125732f;
    const unsigned rates[]={60,120,144};
    for(unsigned r=0;r<3;r++) for(int native=0;native<2;native++) {
        unsigned fps=rates[r]; g_tsfp_native_timing_active=native;
        memset(memory+CAMERA,0,0x1000); collisions=0;
        MEMF(TARGET)=100; MEMF(TARGET+4)=MEMF(TARGET+8)=0;
        for(unsigned f=0;f<fps;f++) {
            unsigned whole=(f+1)*60/fps-f*60/fps;
            translate(whole,60.0f/fps);
            /* The caller clears its transition flag when this returns zero.
             * Fractional time must not masquerade as reaching a far target. */
            assert((g_eax>0)==(native || whole>0));
        }
        /* Original whole-tick gate loses time at fractional update rates. */
        assert(fabsf(MEMF(CAMERA+0x27c)-15*(native?1:60.0f/fps))<.0001f);
        assert(collisions==(native?fps:60));
        assert(fabsf(MEMF(CAMERA+0x2e8)-15)<.0001f);
        float before=MEMF(CAMERA+0x27c);
        translate(0,0); assert(MEMF(CAMERA+0x27c)==before);
        vector(CAMERA+0x334,0,0); vector(TARGET,1,.3f);
        for(unsigned f=0;f<fps;f++) {
            float old=MEMF(CAMERA+0x334);
            orient((f+1)*60/fps-f*60/fps,60.0f/fps,60);
            if(native) assert(MEMF(CAMERA+0x334)>old);
        }
        float fraction=1-powf(59.0f/60,60);
        direction(fraction,.3f*fraction);
        orient(0,0,0); direction(fraction,.3f*fraction);
    }
    g_tsfp_native_timing_active=1;
    /* Quarter-speed time and catch-up use game time, not the host rate. */
    for(unsigned r=0;r<3;r++) {
        unsigned fps=rates[r];
        vector(CAMERA+0x334,0,0); vector(TARGET,1,.3f);
        for(unsigned f=0;f<fps;f++)
            orient((f+1)*15/fps-f*15/fps,15.0f/fps,60);
        float fraction=1-powf(59.0f/60,15);
        direction(fraction,.3f*fraction);
    }
    vector(CAMERA+0x334,0,0); vector(TARGET,1,.3f);
    orient(3,3,60);
    float fraction=1-powf(59.0f/60,3);
    direction(fraction,.3f*fraction);
    /* Shortest angular path across the wrap, with vertical easing. */
    vector(CAMERA+0x334,-3.1f,-.2f); vector(TARGET,3.1f,.25f);
    orient(0,.25f,10);
    fraction=1-powf(.9f,.25f);
    direction(-3.1f+(6.2f-6.283185482f)*fraction,-.2f+.45f*fraction);
    /* Snap modes also consume a fractional interval; pause preserves them. */
    for(int remaining=-1;remaining<=1;remaining++) {
        vector(CAMERA+0x334,0,0);
        orient(0,0,remaining); direction(0,0);
        orient(0,.25f,remaining); direction(3.1f,.25f);
    }
    assert(RECOMP_LOAD_GLOBAL_U32(0xc73c1,0x4e7944,7)==7);
    assert(RECOMP_F32_DIV_AT(0xc7652,1,60)==1.0f/60);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0xc7610,0x4e7944,3)==3);
    assert(RECOMP_F32_DIV_AT(0xc7651,1,60)==1.0f/60);
    free(memory);
    puts("PASS: original camera translation and angular easing at 60/120/144");
    return 0;
}
