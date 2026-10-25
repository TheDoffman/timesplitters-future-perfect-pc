/* Complete original walking update, with a controlled flat collision floor.
 * Input displacements and world services are boundaries, not a live match. */
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
enum { PLAYER=0x10000,ENTITY=0x20000,BODY=0x22000,POSITION=0x40000,
       SIDE=0x40100,FORWARD=0x40110,STACK=0x50000 };
static unsigned buttons,collisions;
void sub_00042760(void) {g_eax=buttons;g_esp+=4;}
void sub_00059F90(void) {g_eax=0;g_esp+=4;}
void sub_000D9230(void) {g_esp+=4;}
void sub_000C5B50(void) {
    assert(MEM32(g_esp+8)==PLAYER+0xa8 && MEM32(g_esp+12)==POSITION);
    MEMF(POSITION+4)=0;MEMF(BODY+0xbfc)=0;collisions++;
    g_eax=1;g_esp+=4;
}
void sub_00258CE0(void) {assert(!"unexpected wall collision");}
void sub_000D2CE0(void) {assert(!"unexpected loss of ground contact");}
void sub_000D1E20(void) {
    assert(MEM32(g_esp+4)==PLAYER);
    assert(MEMF(g_esp+8)==MEMF(PLAYER+0xa8));
    assert(MEMF(g_esp+16)==MEMF(PLAYER+0xb0));
    g_esp+=4;
}
/* Mutation control leaves the other production numeric policies enabled. */
static int legacy_boost;
float pc_native_rocking_sub(float left,float right) {
    (void)left;(void)right;assert(!"unrelated rocking policy");return 0;
}
static float observed_sub(uint32_t site,float left,float right) {
    if(legacy_boost && site==0x259060u)return left-right;
    return RECOMP_F32_SUB_AT(site,left,right);
}
#undef RECOMP_F32_SUB_AT
#define RECOMP_F32_SUB_AT(site,left,right) observed_sub(site,left,right)
/* GENERATED_ROUTINES */
static void setup(int boosted) {
    memset((void *)((uintptr_t)PLAYER+g_xbox_mem_offset),0,0x30200);
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER+0x14)=ENTITY;MEM32(ENTITY+0x7c)=BODY;
    MEM32(PLAYER+4)=0xffffffff;MEM32(PLAYER+0x114)=2;
    MEMF(PLAYER+0x70)=12;MEM32(0x4e8cec)=60;MEM32(0x4d1940)=1;
    MEMF(SIDE)=1;MEMF(FORWARD+4)=1;MEMF(ENTITY+0x1d0)=100;
    MEM8(0x7de456)=boosted?0x11:0;MEM8(0x7de498)=0;
    buttons=boosted?0x800:0;collisions=0;
}
static void step(unsigned whole,float raw,float delta,float side,float forward) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;MEMF(0x4e7948)=raw;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=POSITION;MEMF(STACK+8)=0;
    MEMF(STACK+12)=side;MEM32(STACK+16)=SIDE;
    MEM32(STACK+20)=FORWARD;MEMF(STACK+24)=forward;
    g_ebx=0x123;g_ebp=0x456;g_esi=0x789;g_edi=0xabc;
    sub_00258F50();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0x123 && g_ebp==0x456 && g_esi==0x789 && g_edi==0xabc);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(int argc,char **argv) {
    (void)argv;legacy_boost=argc>1;
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    const unsigned rates[]={60,60,120,144,180,240};
    for(unsigned boosted=0;boosted<2;boosted++) for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        unsigned fps=rates[mode];g_tsfp_native_timing_active=mode!=0;setup(boosted);
        for(unsigned f=0;f<fps;f++)step((f+1)*60/fps-f*60/fps,60.f/fps,60.f/fps,0,1);
        printf("walk boosted=%u native=%u fps=%u x=%.7f z=%.7f charge=%.7f collisions=%u\n",
            boosted,mode!=0,fps,MEMF(PLAYER+0xa8),MEMF(PLAYER+0xb0),MEMF(ENTITY+0x1d0),collisions);
        fflush(stdout);
        if(boosted)assert(fabsf(MEMF(ENTITY+0x1d0)-(100-60*MEMF(0x49f2bc)))<.002f);
        assert(collisions==fps && fabsf(MEMF(PLAYER+0xac))<.0001f);
        if(!boosted)assert(fabsf(MEMF(PLAYER+0xb0)-12)<.0001f);
    }
    /* Equal active-original-update budgets at quarter and triple speed.
       The real parent dispatcher skips original zero-whole-tick updates;
       its gate has a separate actual-source fixture. */
    for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) for(unsigned speed=0;speed<2;speed++) {
        unsigned fps=rates[mode],previous=0;
        float scale=speed?3.f:.25f;
        g_tsfp_native_timing_active=mode!=0;setup(1);
        for(unsigned f=0;f<fps;f++) {
            unsigned total=(unsigned)((f+1)*60.0/fps*scale+1e-6);
            unsigned whole=total-previous;previous=total;
            if(mode || whole)step(whole,60.f/fps,scale*60.f/fps,0,1);
        }
        float charges=speed?60.f:15.f;
        printf("boost fps=%u native=%u scale=%.2f remaining=%.7f expected=%.7f\n",
            fps,mode!=0,scale,MEMF(ENTITY+0x1d0),100-charges*MEMF(0x49f2bc));
        assert(fabsf(MEMF(ENTITY+0x1d0)-(100-charges*MEMF(0x49f2bc)))<.002f);
    }
    /* Fractional pause and one catch-up update preserve charge semantics. */
    g_tsfp_native_timing_active=1;setup(1);
    step(1,1,0,0,0);assert(MEMF(ENTITY+0x1d0)==100);
    step(3,3,3,0,1);
    assert(fabsf(MEMF(ENTITY+0x1d0)-(100-MEMF(0x49f2bc)))<.00001f);
    /* Released input, another mode, another selected player, and an empty
       resource must all retain the original non-boost behavior. */
    for(unsigned disabled=0;disabled<4;disabled++) {
        setup(1);
        if(disabled==0)buttons=0;
        if(disabled==1)MEM8(0x7de456)=0;
        if(disabled==2)MEM8(0x7de498)=1;
        if(disabled==3)MEMF(ENTITY+0x1d0)=0;
        float before=MEMF(ENTITY+0x1d0);
        step(0,.5f,.5f,0,1);
        assert(MEMF(ENTITY+0x1d0)==before && fabsf(MEMF(PLAYER+0xb0)-.1f)<.00001f);
    }
    /* Combined sideways/forward input respects the same speed cap. Stop
       and reversal samples exercise the complete displacement/collision path. */
    for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        unsigned fps=rates[mode];g_tsfp_native_timing_active=mode!=0;setup(0);
        for(unsigned f=0;f<fps;f++) {
            float direction=f<fps/4?1.f:f<fps/2?0.f:-.5f;
            step((f+1)*60/fps-f*60/fps,60.f/fps,60.f/fps,direction,direction);
        }
        assert(fabsf(MEMF(PLAYER+0xa8)+3.f/sqrtf(2.f))<.0001f);
        assert(fabsf(MEMF(PLAYER+0xb0)+3.f/sqrtf(2.f))<.0001f);
    }
    /* Keep a full-radius guest-only reference across frame rates. The PC
       sampler applies an additional diagonal correction after this point;
       live player-position tests cover the complete input path. */
    for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        unsigned fps=rates[mode];float diagonal=23169.f/32767.f;
        float straight_distance,diagonal_distance;
        g_tsfp_native_timing_active=mode!=0;setup(0);
        for(unsigned f=0;f<fps;f++)
            step((f+1)*60/fps-f*60/fps,60.f/fps,60.f/fps,0,1);
        straight_distance=hypotf(MEMF(PLAYER+0xa8),MEMF(PLAYER+0xb0));
        setup(0);
        for(unsigned f=0;f<fps;f++)
            step((f+1)*60/fps-f*60/fps,60.f/fps,60.f/fps,diagonal,diagonal);
        diagonal_distance=hypotf(MEMF(PLAYER+0xa8),MEMF(PLAYER+0xb0));
        printf("keyboard travel fps=%u straight=%.7f diagonal=%.7f\n",
               fps,straight_distance,diagonal_distance);
        assert(fabsf(diagonal_distance-straight_distance)<.003f);
    }
    /* Depletion retains the original threshold/overshoot, within one
       original update of boost duration and travel distance. */
    for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        unsigned fps=rates[mode],f=0;g_tsfp_native_timing_active=mode!=0;setup(1);
        MEMF(ENTITY+0x1d0)=1;
        while(MEMF(ENTITY+0x1d0)>0 && f<fps*5) {
            step((f+1)*60/fps-f*60/fps,60.f/fps,60.f/fps,0,1);f++;
        }
        float original_seconds=ceilf(1.f/MEMF(0x49f2bc))/60.f;
        assert(fabsf((float)f/fps-original_seconds)<=1.f/60.f+.00001f);
        assert(MEMF(ENTITY+0x1d0)<=0 && MEMF(ENTITY+0x1d0)>=-MEMF(0x49f2bc));
        assert(fabsf(MEMF(PLAYER+0xb0)-original_seconds*19.2f)<.321f);
    }
    /* Unrelated sites and the disabled native path keep exact subtraction. */
    assert(RECOMP_F32_SUB_AT(0x259061u,4.f,.25f)==3.75f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_SUB_AT(0x259060u,4.f,.25f)==3.75f);
    puts("PASS: original walking update across native rates");
    free(memory);return 0;
}
