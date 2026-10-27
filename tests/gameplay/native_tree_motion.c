/* Execute the original TREE callback; effect services are boundary spies. */
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
RECOMP_TLS uint16_t g_fp_control_word=0x037f;
enum { ENTITY=0x1000, STACK=0x10000 };
static unsigned crossings,finishes;
static int legacy;
static double elapsed,crossed_at,finished_at;
void sub_001D7130(void) {
    assert(MEM32(g_esp+4)==ENTITY+0x64);crossings++;crossed_at=elapsed;g_esp+=4;
}
void sub_00111AE0(void) {
    assert(MEM32(g_esp+4)==0 && MEM32(g_esp+8)==0x78f2e0);
    assert(MEM32(g_esp+16)==ENTITY+0x64);finishes++;finished_at=elapsed;g_esp+=4;
}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    (legacy && (site)==0x1d88b0 ? (value) : pc_native_global_load(site,address,value))
/* GENERATED_ROUTINES */
static void reset(void) {
    memset((void *)(g_xbox_mem_offset+ENTITY),0,0x300);
    MEM32(ENTITY+0x1a0)=1;MEMF(ENTITY+0xe0)=2;
    crossings=finishes=0;elapsed=crossed_at=finished_at=0;
}
static void step(unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    elapsed+=(double)delta/60;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    sub_001D88B0();
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;setvbuf(stdout,NULL,_IONBF,0);
    const unsigned rates[]={60,120,144,180,240};
    /* Before repair, 240 FPS carries only one quarter of the animation time. */
    g_tsfp_native_timing_active=1;legacy=1;reset();
    for(unsigned f=1;f<=240;f++)step(f/4-(f-1)/4,.25f);
    printf("legacy_tree age=%f crossings=%u\n",MEMF(ENTITY+0x1a4),crossings);
    assert(fabsf(MEMF(ENTITY+0x1a4)-.25f)<.00001f && !crossings);
    legacy=0;
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned phase=0;phase<4;phase++) {
        reset();unsigned rate=rates[r]*slow,previous=phase/4;
        for(unsigned f=1;f<=rate;f++) {
            unsigned tick=(f*240+phase*rate)/(rate*4);
            step(tick-previous,60.f/rate);previous=tick;
        }
        printf("tree fps=%u slow=%u phase=%u age=%f\n",rates[r],slow,phase,MEMF(ENTITY+0x1a4));
        assert(fabsf(MEMF(ENTITY+0x1a4)-1.f)<.00003f);
        /* Pose is evaluated directly from age, with no new easing policy. */
        double expected=(exp((double)MEMF(ENTITY+0x1a4)*MEMF(0x49ded8))-1)*MEMD(0x49ded0)*MEMD(0x49dec8);
        assert(fabs(MEMF(ENTITY+0xc8)-expected)<.00001);
        float before=MEMF(ENTITY+0x1a4);unsigned events=crossings;
        step(0,0);step(1,0);assert(MEMF(ENTITY+0x1a4)==before && crossings==events);
        for(unsigned f=rate+1;f<=rate*6;f++) {
            unsigned tick=(f*240+phase*rate)/(rate*4);
            step(tick-previous,60.f/rate);previous=tick;
        }
        assert(crossings==1 && finishes==1 && !MEM32(ENTITY+0x1a0));
        assert(crossed_at>=1-.00003 && crossed_at<=1+1./rate+.00003);
        double terminal=log1p(80/(MEMD(0x49ded0)*MEMD(0x49dec8)))/MEMF(0x49ded8);
        assert(finished_at>=terminal-.00003 && finished_at<=terminal+1./rate+.00003);
        assert(MEMF(ENTITY+0xc8)==80 && fabsf(MEMF(ENTITY+0xe0)+.2f)<.000001f);
    }
    /* Original mode and whole/catch-up intervals retain the original body. */
    for(unsigned native=0;native<2;native++) {
        reset();g_tsfp_native_timing_active=native;step(0,.25f);
        assert(fabsf(MEMF(ENTITY+0x1a4)-(native?1.f/240:0))<.000001f);
        reset();step(1,1);assert(fabsf(MEMF(ENTITY+0x1a4)-1.f/60)<.000001f);
        reset();step(3,3);assert(fabsf(MEMF(ENTITY+0x1a4)-.05f)<.000001f);
    }
    unsigned char reference[0x300];unsigned reference_crossings,reference_finishes;
    const unsigned spans[]={1,3,30,45,60,180};
    for(unsigned native=0;native<2;native++) {
        reset();g_tsfp_native_timing_active=native;
        for(unsigned i=0;i<sizeof(spans)/sizeof(spans[0]);i++)step(spans[i],(float)spans[i]);
        if(!native) {
            memcpy(reference,(void *)(g_xbox_mem_offset+ENTITY),sizeof reference);
            reference_crossings=crossings;reference_finishes=finishes;
        } else {
            assert(!memcmp(reference,(void *)(g_xbox_mem_offset+ENTITY),sizeof reference));
            assert(crossings==reference_crossings && finishes==reference_finishes);
        }
    }
    puts("PASS: original TREE phase/pose, single events, pause, slow time, phase and original fallback");
    free(memory);return 0;
}
