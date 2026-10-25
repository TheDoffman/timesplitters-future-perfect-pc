/* Complete original player statistics, clock getter, eligibility and float
 * accumulator. No replacement time-counter or statistics model. */
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
enum { PLAYER=0x1000, MODEL=0x3000, BODY=0x4000, STACK=0x10000, STATS=0x20000, SIZE=0x273*4 };
static int legacy;
void sub_003533F0(void) { assert(!"unexpected multiplayer stats branch"); }
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    (legacy && (site)==0xd2079 ? (value) : pc_native_global_load(site,address,value))
/* GENERATED_ROUTINES */
static void reset(unsigned hour) {
    memset((void *)(g_xbox_mem_offset+PLAYER),0,0x4000);
    memset((void *)(g_xbox_mem_offset+STATS),0,SIZE);
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER+0x14)=MODEL;MEM32(MODEL+0x7c)=BODY;
    MEM32(PLAYER+0x114)=8;MEM8(BODY+0xc6c)=4;MEM32(BODY+0xc04)=1;
    MEM32(0x75ccc0)=STATS;MEM32(0x790950)=1;MEM32(0x7a2958)=0;
    MEM32(0x79094c)=1;MEM32(0x7de30c)=7;MEM32(0x7de455)=4;
    MEM32(0x4e8cec)=60;MEM32(0x7de328)=0;MEM32(0x749160)=hour;
}
static void step(unsigned whole,float delta,float x,float y) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEMF(STACK+4)=x;MEMF(STACK+8)=y;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    if(legacy)sub_000D1FD0();
    else RECOMP_DIRECT_CALLEE_AT(0xD2CB7,sub_000D1FD0)();
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static float stat(unsigned id) { return MEMF(STATS+id*4); }
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);g_tsfp_native_timing_active=1;legacy=1;reset(0);
    for(unsigned f=1;f<=240;f++)step(f/4-(f-1)/4,.25f,0,0);
    printf("legacy_stats time=%f early=%f ticks=%u\n",stat(0x29),stat(0x4b),MEM32(0x7de328));
    assert(fabsf(stat(0x29)-.25f)<.00001f && fabsf(stat(0x4b)-.25f)<.00001f);
    assert(MEM32(0x7de328)==60 && fabsf(stat(0x45)-1)<.00001f);
    legacy=0;const unsigned rates[]={60,120,144,180,240},hours[]={0,5,6,23};
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned phase=0;phase<4;phase++)for(unsigned h=0;h<4;h++) {
        reset(hours[h]);unsigned count=rates[r]*slow,previous=0;
        for(unsigned f=1;f<=count;f++) {
            unsigned tick=(f*240+phase*count)/(count*4);
            step(tick-previous,60.f/count,4.f/count,-2.f/count);previous=tick;
        }
        printf("stats fps=%u slow=%u phase=%u hour=%u time=%f early=%f other=%f/%f/%f\n",
               rates[r],slow,phase,hours[h],stat(0x29),stat(0x4b),stat(0x45),stat(0x49),stat(0x4e));
        assert(fabsf(stat(0x29)-1)<.00003f);
        assert(fabsf(stat(0x4b)-(hours[h]<6?1.f:0.f))<.00003f);
        assert(MEM32(0x7de328)==60);
        assert(fabsf(stat(0x45)-1)<.00003f && fabsf(stat(0x49)-1)<.00003f && fabsf(stat(0x4e)-1)<.00003f);
        assert(fabsf(stat(0x31)-4)<.0001f && stat(0x32)==0 && fabsf(stat(0x34)-2)<.0001f);
        unsigned char saved[SIZE];memcpy(saved,(void *)(g_xbox_mem_offset+STATS),SIZE);
        step(0,0,0,0);
        assert(!memcmp(saved,(void *)(g_xbox_mem_offset+STATS),SIZE) && MEM32(0x7de328)==60);
    }
    reset(0);step(1,0,0,0);assert(stat(0x29)==0 && MEM32(0x7de328)==0);
    /* Uneven intervals with catch-up retain all elapsed time. */
    reset(0);unsigned units=0,old=0;const unsigned pieces[]={1,3,2,7,5,1,4,9};
    for(unsigned i=0;units<240;i++) {
        unsigned n=pieces[i%8];if(n>240-units)n=240-units;
        units+=n;unsigned tick=units/4;step(tick-old,n/4.f,0,0);old=tick;
    }
    assert(fabsf(stat(0x29)-1)<.00001f && MEM32(0x7de328)==60);
    /* The existing wall-clock condition remains owned by the original getter. */
    reset(0);
    for(unsigned f=1;f<=240;f++) {
        MEM32(0x749160)=f<=120?5:6;step(f/4-(f-1)/4,.25f,0,0);
    }
    assert(fabsf(stat(0x29)-1)<.00001f && fabsf(stat(0x4b)-.5f)<.00001f);
    for(unsigned disabled=0;disabled<2;disabled++) {
        reset(0);
        if(disabled) { MEM32(PLAYER)=1;MEM32(BODY+8)=1; }
        else MEM32(0x7de30c)=8;
        step(1,1,2,-1);
        for(unsigned i=0;i<SIZE;i++)assert(!MEM8(STATS+i));
    }
    /* Preserve Original fallback and exact whole/catch-up updates. */
    unsigned char reference[SIZE];unsigned reference_ticks=0;
    for(unsigned native=0;native<2;native++) {
        reset(5);g_tsfp_native_timing_active=native;
        for(unsigned n=1;n<=4;n++)step(n,(float)n,(float)n,-(float)n);
        if(!native) { memcpy(reference,(void *)(g_xbox_mem_offset+STATS),SIZE);reference_ticks=MEM32(0x7de328); }
        else assert(!memcmp(reference,(void *)(g_xbox_mem_offset+STATS),SIZE) && reference_ticks==MEM32(0x7de328));
    }
    reset(0);g_tsfp_native_timing_active=0;step(0,.25f,0,0);
    assert(stat(0x29)==0 && stat(0x4b)==0);
    /* Bookkeeping precision must retain the original addition schedule too. */
    float time_reference[5];const unsigned ids[]={0x29,0x4b,0x45,0x49,0x4e};
    for(unsigned native=0;native<2;native++) {
        reset(0);g_tsfp_native_timing_active=native;
        unsigned rate=native?240:60,previous=0;
        for(unsigned f=1;f<=rate*600;f++) {
            unsigned tick=f*60/rate;
            step(tick-previous,60.f/rate,0,0);previous=tick;
        }
        for(unsigned i=0;i<5;i++) {
            printf("long_stats native=%u id=%x seconds=%f\n",native,ids[i],stat(ids[i]));
            if(!native)time_reference[i]=stat(ids[i]);
            else assert(stat(ids[i])==time_reference[i]);
        }
    }
    puts("PASS: original elapsed statistics, clock conditions, integer count, distance, pause and fallback");
    free(memory);return 0;
}
