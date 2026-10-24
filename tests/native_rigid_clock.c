#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
/* PHYSICS POLICY */
static int legacy;
static unsigned calls,allocs,active_frames;
static double seconds;
static float last_step;
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    (legacy && ((site)==0x171fb9 || (site)==0x17208e || (site)==0x172119) ? \
     (value) : pc_native_global_load(site,address,value))
#undef RECOMP_F32_MUL_AT
#define RECOMP_F32_MUL_AT(site,left,right) \
    ((site)==0x172099 && legacy ? (left)*(right) : pc_native_multiply(site,left,right))
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected callback")
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected tail")
void sub_0003DFA0(void) { g_eax=allocs++%2?0x3000:0x2000;g_esp+=4; }
void sub_0003D6E0(void) {g_eax=0x4000;g_esp+=4;}
void sub_0003D6B0(void) {g_esp+=4;}
void sub_00170500(void) {
    last_step=MEMF(g_esp+4);assert(isfinite(last_step) && last_step>0);
    MEM32(g_edi)=0;g_esp+=4;
}
void sub_0016CCD0(void) {g_esp+=4;}
void sub_00170690(void) {g_eax=0;g_esp+=4;}
void sub_0016B6B0(void) {g_esp+=4;}
void sub_0016B050(void) {g_esp+=4;}
void sub_0016C0D0(void) {g_esp+=4;}
void sub_001719E0(void) {g_esp+=4;}
void sub_00171560(void) {
    float secondary=MEM16(0x74c454)?last_step*MEMF(0x475d50):last_step;
    assert(MEMF(g_esp+12)==last_step && MEMF(g_esp+16)==secondary);
    seconds+=last_step;++calls;g_esp+=4;
}
void pc_native_physics_solver(void) {pc_native_physics_solver_guest(sub_00171560);}
/* ROUTINES */
static void step(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=0x18000;MEM32(g_esp)=0x12345678;
    g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=0xdef0;
    unsigned before=calls;
    pc_native_physics_guest(sub_00171FB0);
    assert(g_esp==0x18004 && MEM32(0x18000)==0x12345678);
    /* This framed function restores its local EBP. g_ebp is the lifter's
     * published frame for callees, unlike the shared EBX/ESI/EDI registers. */
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef);
    assert(MEM32(0x17ffc)==0xdef0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    if(calls>before)++active_frames;
}
int main(int argc,char **argv)
{
    (void)argv;legacy=argc>1;setbuf(stdout,NULL);
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    MEM32(0x4e8cec)=60;
    for(unsigned r=0;r<3;r++) {
        unsigned fps=r==0?60:r==1?120:144;
        g_tsfp_native_timing_active=r!=0;
        MEM32(0x74c450)=0;
        for(unsigned i=0;i<15;i++)MEMF(0x74c414+4*i)=1;
        for(unsigned frame=0;frame<fps;frame++)step((frame+1)*60/fps-frame*60/fps,60.f/fps);
        calls=active_frames=0;seconds=0;
        for(unsigned frame=0;frame<fps;frame++)step((frame+1)*60/fps-frame*60/fps,60.f/fps);
        printf("rigid_clock fps=%u calls=%u active_frames=%u seconds=%.9f\n",fps,calls,active_frames,seconds);
        assert(calls==fps && active_frames==calls);
        assert(fabs(seconds-1.)<1e-5);
        unsigned before=calls;step(0,0);step(3,0);assert(calls==before || !g_tsfp_native_timing_active);
    }
    g_tsfp_native_timing_active=1;
    for(unsigned speed=0;speed<3;speed++) {
        MEM16(0x74c454)=speed==2;
        const float intervals[]={.25f,.5f,.75f,1.25f,2.5f,.05f,3.f};
        float scale=speed==0?.25f:speed==1?1.f:3.f;
        double expected=0;unsigned expected_calls=0;
        seconds=0;calls=active_frames=0;
        for(unsigned i=0;i<7;i++) {
            float delta=intervals[i]*scale;
            step((unsigned)delta,delta);
            expected+=delta/60.;expected_calls+=(unsigned)ceilf(delta);
            assert(last_step<=1.f/60.f+.000001f);
        }
        assert(fabs(seconds-expected)<1e-7 && calls==expected_calls && active_frames==7);
        printf("rigid_variable scale=%g calls=%u seconds=%.9f\n",scale,calls,seconds);
    }
    free(memory);puts("PASS: original physics clock; solver/world services controlled, no handling claim");return 0;
}
