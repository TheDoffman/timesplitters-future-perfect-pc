/* Original US light-color preview; model-color propagation is a spy, RNG is
 * deterministic. The only direct caller is the scene-100 type-2 entity path. */
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
enum { ENTITY=0x1000,PARAMS=0x2000,PALETTE=0x3000,STACK=0x10000 };
static unsigned random_calls,propagations;
static int unpatched;
/* LIGHT_POLICY */
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected light preview tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected light preview callback")
void sub_00153B40(void) { random_calls++;g_eax=0x80000000u;g_esp+=4; }
void sub_00115F50(void) {
    uint32_t color=MEM32(PARAMS+40+0x1c);
    assert(MEM32(g_esp+4)==1 && MEM32(g_esp+8)==(color&255u));
    assert(MEM32(g_esp+12)==((color>>8)&255u));
    propagations++;g_esp+=4;
}
/* GENERATED_ROUTINES */
static uint32_t step(unsigned whole,float delta,unsigned tick) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;MEM32(0x7de318)=tick;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    if(unpatched)sub_00116130_gen();
    else pc_native_light_preview_guest(sub_00116130_gen,ENTITY);
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta && MEM32(0x7de318)==tick);
    return MEM32(PARAMS+40+0x1c);
}
static void setup(unsigned kind) {
    memset((void *)(g_xbox_mem_offset+PARAMS),0,80);
    MEM32(ENTITY+0x34)=1;MEM32(0x73d96c)=PARAMS;MEM32(0x7ac87c)=PALETTE;
    MEM32(0x4e8cec)=60;MEM32(PARAMS+40)=kind;
    MEM32(PARAMS+44)=100000;MEMF(PARAMS+48)=.25f;MEMF(PARAMS+52)=.75f;
    for(unsigned i=0x15;i<0x18;i++)MEM8(PALETTE+i)=255;
    for(unsigned i=0x19;i<0x1c;i++)MEM8(PALETTE+i)=0;
    random_calls=propagations=0;
    memset((void *)(g_xbox_mem_offset+0x7ac88c),0,12);
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);
    unpatched=1;
    for(unsigned kind=0;kind<4;kind++) {
        setup(kind);g_tsfp_native_timing_active=1;
        uint32_t first=step(1,1,10),fractional=step(0,60.f/144,10);
        printf("light kind=%u whole=%08x fractional=%08x rng=%u propagation=%u\n",
            kind,first,fractional,random_calls,propagations);
        if(kind==3)assert(first!=fractional);else assert(first==fractional);
        assert(random_calls==0 && propagations==2);
    }
    unpatched=0;
    for(unsigned kind=0;kind<4;kind++) {
        setup(kind);g_tsfp_native_timing_active=1;
        uint32_t first=step(1,1,10),fractional=step(0,60.f/144,10);
        assert(first==fractional && random_calls==0);
        assert(propagations==(kind==3?1u:2u));
        if(kind==3) {
            assert(step(0,0,10)==first && step(1,0,10)==first);
            assert(propagations==1 && random_calls==0);
        }
    }
    /* Compare each discrete decision to the original normal-speed 60-Hz
     * sequence; intermediate native/slow-motion frames must retain it. */
    uint32_t colors[481],deadlines[481],rgb[481][3];unsigned rng[481];
    setup(3);MEM32(PARAMS+44)=0;g_tsfp_native_timing_active=0;
    colors[0]=MEM32(PARAMS+40+0x1c);deadlines[0]=0;rng[0]=0;
    memset(rgb[0],0,sizeof rgb[0]);
    for(unsigned t=1;t<=480;t++) {
        colors[t]=step(1,1,t);deadlines[t]=MEM32(PARAMS+44);rng[t]=random_calls;
        memcpy(rgb[t],(void *)(g_xbox_mem_offset+0x7ac88c),sizeof rgb[t]);
    }
    const unsigned rates[]={60,120,144};
    for(unsigned r=0;r<3;r++)for(unsigned slow=1;slow<=4;slow*=4) {
        setup(3);MEM32(PARAMS+44)=0;g_tsfp_native_timing_active=1;
        unsigned n=rates[r]*slow*8,prev=0,retained=0;
        for(unsigned f=1;f<=n;f++) {
            unsigned tick=f*60/(rates[r]*slow),whole=tick-prev;
            uint32_t color=step(whole,60.f/(rates[r]*slow),tick);
            assert(color==colors[tick] && MEM32(PARAMS+44)==deadlines[tick]);
            assert(random_calls==rng[tick] && propagations==tick);
            assert(!memcmp(rgb[tick],(void *)(g_xbox_mem_offset+0x7ac88c),sizeof rgb[tick]));
            if(!whole)retained++;
            assert(step(0,0,tick)==color && step(1,0,tick)==color);
            prev=tick;
        }
        printf("light fixed fps=%u slow=%u retained=%u rng=%u decisions=%u\n",
            rates[r],slow,retained,random_calls,propagations);
    }
    /* Positive whole/catch-up intervals must remain the original function,
     * including its whole-count-dependent random deadline calculation. */
    for(unsigned whole=1;whole<=3;whole++) {
        uint32_t expected[10],actual[10],expected_rgb[3];
        setup(3);MEM32(PARAMS+44)=0;g_tsfp_native_timing_active=0;
        step(whole,whole+.125f,47);
        memcpy(expected,(void *)(g_xbox_mem_offset+PARAMS+40),sizeof expected);
        memcpy(expected_rgb,(void *)(g_xbox_mem_offset+0x7ac88c),sizeof expected_rgb);
        unsigned expected_rng=random_calls;
        setup(3);MEM32(PARAMS+44)=0;g_tsfp_native_timing_active=1;
        step(whole,whole+.125f,47);
        memcpy(actual,(void *)(g_xbox_mem_offset+PARAMS+40),sizeof actual);
        assert(!memcmp(expected,actual,sizeof expected) && random_calls==expected_rng);
        assert(!memcmp(expected_rgb,(void *)(g_xbox_mem_offset+0x7ac88c),sizeof expected_rgb));
    }
    puts("PASS: original light flicker decisions, RNG, colors and paused/fractional retention at 60/120/144");
    free(memory);return 0;
}
