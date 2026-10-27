/* Original level dispatcher -> Venice timed ambient event. Asset lookup and
 * sound consumers are spies; scheduling, countdown and reset are original. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1;
static unsigned events,callbacks;
static int legacy_gate;
static uint32_t callback_gate(uint32_t site,uint32_t address,uint32_t value)
{
    if(legacy_gate && site==0x222b60u)return value;
    return pc_native_global_load(site,address,value);
}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) callback_gate(site,address,value)
void sub_00222B60(void);
static void level_callback(uint32_t target)
{ assert(target==0x222b60);++callbacks;sub_00222B60(); }
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) level_callback(target)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) level_callback(target)
void sub_0006A850(void) {g_eax=0x5000;g_esp+=4;}
void sub_00045390(void) {g_esp+=4;}
void sub_00045820(void)
{ assert(MEM32(MEM32(g_esp+4))==0x156f);++events;g_esp+=4; }
#define UNUSED(name) void name(void) {assert(!"unexpected dispatcher path");g_esp+=4;}
UNUSED(sub_0003D7B0) UNUSED(sub_00067450) UNUSED(sub_0012D350)
UNUSED(sub_001CDAE0) UNUSED(sub_00227E40) UNUSED(sub_002282B0)
UNUSED(sub_002284D0) UNUSED(sub_00230510) UNUSED(sub_00232270)
UNUSED(sub_0033ED70)
/* ROUTINES */
static void dispatch(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=0x7000;MEM32(g_esp)=0x12345678;
    g_ebx=0x1111;g_esi=0x2222;g_edi=0x3333;
    sub_00228870();
    assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678);
    assert(g_ebx==0x1111 && g_esi==0x2222 && g_edi==0x3333);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(int argc,char **argv)
{
    (void)argv;legacy_gate=argc>1;setbuf(stdout,NULL);
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    MEM32(0x5110)=0x222b60;MEM32(0x75c118)=0x6000;
    MEM32(0x4e8cec)=60;MEM32(0x7de45c)=0x8000;
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned fps=rates[r],denominator=fps*slow;
        g_tsfp_native_timing_active=r!=0;events=callbacks=0;
        g_esp=0x7000;sub_00222740();assert(g_esp==0x7004);
        assert(MEMF(0x6004)==6.f);
        float delta=60.f/denominator;
        /* One game second: the timer must consume all fractional intervals. */
        for(unsigned frame=0;frame<denominator;frame++) {
            unsigned whole=(frame+1)*60/denominator-frame*60/denominator;
            dispatch(whole,g_tsfp_native_timing_active?delta:(whole?1.f:0.f));
        }
        printf("venice_callback fps=%u slow=%u remaining=%.8f callbacks=%u\n",
               fps,slow,MEMF(0x6004),callbacks);
        assert(fabsf(MEMF(0x6004)-5.f)<.0003f && !events);
        assert(callbacks==(g_tsfp_native_timing_active?denominator:60));
        /* Freeze even if a local pause retained an old integer count. */
        float before=MEMF(0x6004);dispatch(0,0);dispatch(1,0);
        if(g_tsfp_native_timing_active)assert(MEMF(0x6004)==before && !events);
        /* Trigger the original event and preserve its authored 900s period. */
        MEMF(0x6004)=delta/120.f;events=0;
        dispatch(1,g_tsfp_native_timing_active?delta:1.f);
        float expected=delta/120.f-(g_tsfp_native_timing_active?delta:1.f)/60.f+900.f;
        assert(events==1 && fabsf(MEMF(0x6004)-expected)<.0001f);
    }
    g_tsfp_native_timing_active=1;events=callbacks=0;MEMF(0x6004)=6.f;
    const float intervals[]={.25f,.5f,1.f/3.f,0.f,1.75f,.125f};
    double ticks=0;unsigned carried=0;
    for(unsigned i=0;i<600;i++) {
        float delta=intervals[i%6];ticks+=delta;
        unsigned total=(unsigned)ticks,whole=total-carried;carried=total;
        dispatch(delta>0?whole:1,delta); /* stale integer pause included */
    }
    assert(!events && fabs(MEMF(0x6004)-(6.-ticks/60.))<.0003);
    assert(callbacks==500);
    assert(RECOMP_LOAD_GLOBAL_U32(0x222b61,0x4e7944,0)==0);
    assert(RECOMP_LOAD_GLOBAL_U32(0x222b60,0x1234,9)==9);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0x222b60,0x4e7944,3)==3);
    free(memory);puts("PASS: Venice callback countdown, dispatch, event, pause, mixed intervals, ABI and Original60");
    return 0;
}
