/* Exact guest-visible state, plus generated entry/callback ordering. */
#include <assert.h>
#include <fenv.h>
#include <stdio.h>
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
#include "pc_model_transform_guest.inc"
static unsigned char memory[4096],initial[4096],expected[4096];
static unsigned seed=17, trace_calls, fast_calls;
static int change_input;
#ifndef DISABLE_FAST
#define DISABLE_FAST 0
#endif
static unsigned random_word(void) { seed=seed*1664525u+1013904223u;return seed; }
static void trace_entry(void) {
    trace_calls++;
    if(change_input) MEMF(MEM32(g_esp+8))=7.f;
}
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,site) trace_entry()
static int actual_policy(uint32_t site) { return RECOMP_FAST_ENTRY(site); }
#undef RECOMP_FAST_ENTRY
#define RECOMP_FAST_ENTRY(site) 0
/* ORIGINAL */
#undef RECOMP_FAST_ENTRY
#define RECOMP_FAST_ENTRY(site) counted_entry(site)
static int counted_entry(uint32_t site) { int done=DISABLE_FAST?0:actual_policy(site);fast_calls+=done;return done; }
/* CANDIDATE */
static void init_regs(void) {
    g_eax=11;g_ecx=12;g_edx=13;g_esp=256;g_ebx=14;g_esi=15;g_edi=16;g_ebp=17;g_seh_ebp=18;
    RecompXmm value;
    for(int i=0;i<4;i++)value.u[i]=0x7f812345u+i;
    g_xmm0=g_xmm1=g_xmm2=g_xmm3=g_xmm4=g_xmm5=g_xmm6=g_xmm7=value;
    trace_calls=0;
}
static void registers(unsigned *regs,RecompXmm *xs) {
    unsigned words[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp};
    RecompXmm values[]={g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7};
    memcpy(regs,words,sizeof words);memcpy(xs,values,sizeof values);
}
static void unchanged_fallback(uint32_t matrix,uint32_t point) {
    MEM32(260)=matrix;MEM32(264)=point;init_regs();
    unsigned before[9],after[9];RecompXmm a[8],b[8];
    registers(before,a);memcpy(initial,memory,sizeof memory);
    assert(feclearexcept(FE_ALL_EXCEPT)==0);
    assert(!pc_model_transform_entry());registers(after,b);
    assert(fetestexcept(FE_ALL_EXCEPT)==0);
    assert(!memcmp(before,after,sizeof before));assert(!memcmp(a,b,sizeof a));
    assert(!memcmp(initial,memory,sizeof memory));
}
/* Even two copies of the original generated body can choose different NaN
 * payloads when a preceding branch changes host register allocation. Only
 * fallback arithmetic destinations permit quiet-NaN equivalence; finite
 * results, all other bytes, and every fast-path result remain bit-exact. */
static int same_result(uint32_t a,uint32_t b,int fallback) {
    return a==b || (fallback &&
        (a&0x7fc00000u)==0x7fc00000u && (b&0x7fc00000u)==0x7fc00000u);
}
int main(void) {
    g_xbox_mem_offset=(ptrdiff_t)memory;
    const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    unsigned tested=0;
    for(unsigned mode=0;mode<4;mode++) {
        assert(fesetround(modes[mode])==0);
        for(unsigned trial=0;trial<10000;trial++) {
            for(unsigned i=0;i<sizeof memory;i+=4) {
                unsigned value=random_word();
                if(trial%4==1)value=(value&0x807fffffu)|((100u+value%35u)<<23);
                memcpy(memory+i,&value,4);
            }
            MEM32(260)=1024;MEM32(264)=trial%2?2048:1008+(trial%25)*4;
            g_tsfp_native_timing_active=trial%2;
            memcpy(initial,memory,sizeof memory);change_input=trial%11==0;
            init_regs();original_transform();assert(trace_calls==1);
            memcpy(expected,memory,sizeof memory);
            unsigned before[9],after[9];RecompXmm a[8],b[8];registers(before,a);
            memcpy(memory,initial,sizeof memory);init_regs();unsigned prior_fast=fast_calls;candidate_transform();
            assert(trace_calls==1);registers(after,b);
            unsigned point=MEM32(264);int fallback=fast_calls==prior_fast;
            for(unsigned i=0;i<sizeof memory;i+=4) {
                uint32_t left,right;memcpy(&left,expected+i,4);memcpy(&right,memory+i,4);
                assert(same_result(left,right,fallback && i>=point && i<point+12));
            }
            assert(!memcmp(before,after,sizeof before));
            for(unsigned reg=0;reg<8;reg++)for(unsigned lane=0;lane<4;lane++)
                assert(same_result(a[reg].u[lane],b[reg].u[lane],fallback && reg<5 && lane==0));
            tested++;
        }
    }
    assert(fesetround(FE_TONEAREST)==0);change_input=0;
    assert(DISABLE_FAST?fast_calls==0:(fast_calls>=10000 && fast_calls<20000));
    memset(memory,0,sizeof memory);
    assert(!actual_policy(0) && !actual_policy(0x151F20u));
    unchanged_fallback(UINT32_MAX-58u,2048);unchanged_fallback(1024,UINT32_MAX-10u);
    for(unsigned point=1016;point<1084;point+=4)unchanged_fallback(1024,point);
    const unsigned rejected[]={0x50000001u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u};
    for(unsigned i=0;i<sizeof rejected/sizeof rejected[0];i++) {
        MEM32(1024)=rejected[i];unchanged_fallback(1024,2048);MEM32(1024)=0;
        MEM32(2048)=rejected[i];unchanged_fallback(1024,2048);MEM32(2048)=0;
    }
    printf("PASS: %u transform comparisons, four rounding modes, %u fast calls, callbacks and unchanged fallback guards\n",tested,fast_calls);
    return 0;
}
