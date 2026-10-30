/* Execute the original US dispatcher, handoff script and animation-event
 * readers. Only asset lookup and inventory side effects are mocked. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0;
static unsigned grants, ammunition, dispatches;
void sub_00218910(void);
void sub_00061570(void);
void sub_000585E0(void);
void sub_00059CF0(void);
static void script_call(uint32_t target)
{
    assert(target==0x218910);
    dispatches++;
    sub_00218910();
}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target, stack, site) script_call(target)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) script_call(target)
void sub_0006A850(void) { g_eax=0x5000; g_esp+=4; }
void sub_00163730(void)
{ g_eax=MEM32(g_esp+4)==0x52425331 ? 0x2000 : 0; g_esp+=4; }
void sub_00056FC0(void)
{ assert(MEM32(g_esp+4)==0xa1c); g_eax=0x4000; g_esp+=4; }
void sub_001BA2B0(void)
{
    assert(MEM32(g_esp+4)==0x2300 && MEM32(g_esp+8)==25);
    ammunition+=MEM32(g_esp+12); g_esp+=4;
}
void sub_001BFCA0(void)
{
    assert(MEM32(g_esp+4)==0x2300 && MEM32(g_esp+8)==25);
    grants++; MEM32(0x2394)=25; g_esp+=4;
}
void sub_001BA130(void) { g_esp+=4; }
void sub_001BFC00(void) { g_esp+=4; }
#define UNUSED(name) void name(void) { assert(!"unexpected dispatcher path"); g_esp+=4; }
UNUSED(sub_0003D7B0)
UNUSED(sub_00067450)
UNUSED(sub_0012D350)
UNUSED(sub_001CDAE0)
UNUSED(sub_00227E40)
UNUSED(sub_002282B0)
UNUSED(sub_002284D0)
UNUSED(sub_00230510)
UNUSED(sub_00232270)
UNUSED(sub_0033ED70)
/* GENERATED_ROUTINES */
static void dispatch(unsigned whole, float delta)
{
    MEM32(0x4e7944)=whole; MEMF(0x4e794c)=delta;
    g_esp=0x7000; MEM32(g_esp)=0x12345678;
    sub_00228870();
    assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678);
    assert(MEM32(0x4e7944)==whole); /* no timer inflation */
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144,180,240};
    unsigned missed=0;
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x2004)=0x3000; MEM32(0x207c)=0x2200;
    MEM32(0x7b0c7c)=0x6000; MEM32(0x6014)=0x2100; MEM32(0x217c)=0x2300;
    MEM32(0x30d4)=0xa1c; MEM32(0x5110)=0x218910;
    MEM16(0x4006)=1; MEM16(0x4008)=7; MEM16(0x400a)=0x2000;
    MEMF(0x4d1994)=60; MEM32(0x7de45c)=0x8000;
    for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++)
    for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned fps=rates[rate],a=60,b=fps*slow;
        while(b) { unsigned remainder=a%b;a=b;b=remainder; }
        unsigned phases=fps*slow/a,frames=3*phases;
        float delta=60.f/(fps*slow);
        unsigned missed_before=missed;
        for(unsigned phase=0;phase<phases;phase++)
        for(int native=0;native<2;native++) {
            grants=ammunition=dispatches=0;
            g_tsfp_native_timing_active=native;
            /* Event frame 7 falls in every phase of this raw-tick cadence.
             * This reproduces overwritten event bounds on zero-tick frames. */
            for(unsigned frame=0;frame<frames;frame++) {
                float before=(6.0f+((int)frame-(int)phase)*delta)/60.0f;
                MEMF(0x30e0)=before; MEMF(0x30dc)=before+delta/60.0f;
                unsigned whole=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
                dispatch(whole,delta);
            }
            if(native) {
                assert(grants==1 && ammunition==8 && dispatches==frames);
            } else {
                assert(grants<=1 && dispatches==frames*60/(fps*slow));
                if(!grants) missed++;
                if(fps*slow==60)assert(grants==1 && ammunition==8);
            }
        }
        if(phases>1)assert(missed>missed_before);
        printf("script_events fps=%u slow=%u phases=%u legacy_misses=%u\n",
               fps,slow,phases,missed-missed_before);
    }
    assert(missed>0); /* unmodified whole-tick gate reproduces the failure */
    grants=ammunition=dispatches=0;
    MEMF(0x30e0)=6.5f/60; MEMF(0x30dc)=7.1f/60;
    dispatch(0,0); assert(!grants && !dispatches); /* paused */
    dispatch(0,.4f); assert(grants==1 && ammunition==8); /* resumed */
    assert(RECOMP_LOAD_GLOBAL_U32(0x228881,0x4e7944,0)==0);
    assert(RECOMP_LOAD_GLOBAL_U32(0x228880,0x1234,9)==9);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0x228880,0x4e7944,3)==3);
    grants=ammunition=dispatches=0;
    dispatch(1,1); assert(grants==1 && ammunition==8); /* original 60 Hz */
    free(memory);
    puts("PASS: original handoff catches all fractional phases; pause, ABI and timers preserved");
    return 0;
}
