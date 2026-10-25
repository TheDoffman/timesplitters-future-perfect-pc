/* Original status-3 model growth, including its rotation/scale/multiply helpers.
 * Entity teardown is observed; graphics allocation and draw submission are absent. */
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
enum { RECORD=0x10000, EFFECT=0x11000, MATRIX=0x12000, STACK=0x18000 };
static unsigned removed;
static int legacy;
void sub_00153600(void);
void sub_00120AB0(void);
static void production_scale(void) { RECOMP_DIRECT_CALLEE_AT(0x120BBFu,sub_00153600)(); }
static void production_update(void) { RECOMP_DIRECT_CALLEE_AT(0x126029u,sub_00120AB0)(); }
static void scope(void) {
    assert(RECOMP_DIRECT_CALLEE_AT(0x120BBEu,sub_00153600)==sub_00153600);
    assert(RECOMP_DIRECT_CALLEE_AT(0x126028u,sub_00120AB0)==sub_00120AB0);
    assert(RECOMP_DIRECT_CALLEE_AT(0x126029u,sub_00120AB0)==
           (pc_native_character_trace_requested()?pc_native_character_update:sub_00120AB0));
}
#undef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site,function) \
    ((site)==0x120BBFu && !legacy ? production_scale : \
     (site)==0x126029u ? production_update : (function))
void sub_000FF200(void) { assert(g_eax==EFFECT);removed++;g_esp+=4; }
#define UNUSED(name) void name(void) { assert(!"unexpected " #name); }
UNUSED(sub_00120660) UNUSED(sub_00120C10) UNUSED(sub_0011D4D0) UNUSED(sub_00347D70)
void sub_000A9070(void) { assert(!"unexpected attached entity"); }
/* CHARACTER_POLICY */
/* CHARACTER_TRACE_POLICY */
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"unexpected status switch target")
/* GENERATED_ROUTINES */
static void step(float raw,float game) {
    MEMF(0x4e7948)=raw;MEMF(0x4e794c)=game;
    g_esp=STACK;MEM32(STACK)=0x12345678;g_esi=RECORD;g_eax=RECORD;
    g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    sub_00125FE0();
    assert(g_esp==STACK+4 && !g_fp_top && g_esi==RECORD);
    assert(g_edi==0xbeef && g_ebx==0xfeed); /* EBP is published by generated frame helpers. */
    assert(MEMF(0x4e7948)==raw && MEMF(0x4e794c)==game);
}
static void reset(int native) {
    memset((void *)(g_xbox_mem_offset+RECORD),0,0x4000);removed=0;
    g_tsfp_native_timing_active=native;MEM32(0x4e8cec)=60;
    MEM32(RECORD+4)=1;MEM32(RECORD+0x54)=3;MEM32(RECORD+0x60)=EFFECT;MEM32(EFFECT+0x18)=MATRIX;
    for(unsigned i=0;i<16;i++)MEMF(MATRIX+4*i)=MEMF(0x4e7900+4*i)=i%5==0?1.f:0.f;
}
static float length(unsigned row) {
    float sum=0;for(unsigned i=0;i<3;i++){float a=MEMF(MATRIX+row*16+i*4);sum+=a*a;}
    return sqrtf(sum);
}
static void matrix(float out[16]) { for(unsigned i=0;i<16;i++)out[i]=MEMF(MATRIX+4*i); }
static void matches(const float expected[16]) {
    for(unsigned i=0;i<16;i++)assert(fabsf(MEMF(MATRIX+4*i)-expected[i])<.0001f*fmaxf(1,fabsf(expected[i])));
}
static float run(unsigned rate,float speed,int native,int check_pause) {
    reset(native);
    unsigned limit=rate/2;
    for(unsigned frame=0;frame<limit;frame++)step(60.f/rate,speed*60.f/rate);
    if(check_pause) {
        float snapshot[16];matrix(snapshot);float time=MEMF(RECORD+0x5c);
        step(1,0);matches(snapshot);assert(MEMF(RECORD+0x5c)==time && !removed);
    }
    return length(0);
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);
    scope();
    const unsigned rates[]={60,120,144,180,240};
    const float speeds[]={.25f,1.f,2.f};
    for(unsigned speed=0;speed<3;speed++) {
        float reference[16];run(60,speeds[speed],0,1);matrix(reference);
        for(unsigned r=0;r<5;r++) {
            run(rates[r],speeds[speed],1,1);matches(reference);
            printf("scale fps=%u speed=%g x=%f y=%f z=%f\n",rates[r],speeds[speed],length(0),length(1),length(2));
        }
    }
    /* Uneven frames and live preset changes preserve the same half-second transform. */
    float reference[16];run(60,1,0,0);matrix(reference);
    for(unsigned r=1;r<5;r++) {
        reset(1);
        for(unsigned f=0;f<rates[r]/2;f++) {
            float delta=60.f/rates[r]*(f%2?1.25f:.75f);step(delta,delta);
        }
        matches(reference);
    }
    reset(1);
    for(unsigned rate=120;rate<=240;rate+=60)
        for(unsigned f=0;f<rate/6;f++)step(60.f/rate,60.f/rate);
    matches(reference);
    /* A control without the call-site repair must retain the measured mismatch. */
    float original=run(60,1,0,0);legacy=1;
    float old120=run(120,1,1,0);legacy=0;
    printf("counterexample original=%f old120=%f\n",original,old120);
    assert(old120/original>1.005f);
    /* Full original updates, including catch-up, stay bit-identical. */
    for(unsigned ticks=1;ticks<=3;ticks++) {
        float reference[16];reset(0);step((float)ticks,(float)ticks);matrix(reference);
        reset(1);step((float)ticks,(float)ticks);
        assert(!memcmp(reference,(void *)(g_xbox_mem_offset+MATRIX),sizeof(reference)));
    }
    /* Check authored cleanup with the actual elapsed timer, not a stubbed expiry. */
    for(unsigned r=0;r<5;r++) {
        reset(1);unsigned frames=0;
        while(MEM32(RECORD+4)&1) { step(60.f/rates[r],60.f/rates[r]);assert(++frames<rates[r]*20); }
        float elapsed=(float)frames/rates[r],threshold=MEMF(0x475db4);
        assert(elapsed>=threshold-.00001f && elapsed<=threshold+1.01f/rates[r]);
        assert(removed==1 && !MEM32(RECORD+0x60));
    }
    /* The wrapper must not leave rewritten caller arguments on the guest stack. */
    reset(1);MEMF(0x4e7948)=.5f;MEMF(0x4e794c)=.5f;
    g_esp=STACK;MEM32(STACK+4)=MATRIX;
    const float factors[]={1.1f,1.2f,1.3f};
    for(unsigned axis=0;axis<3;axis++)MEMF(STACK+8+axis*4)=factors[axis];
    production_scale();assert(g_esp==STACK+4);
    for(unsigned axis=0;axis<3;axis++) {
        assert(MEMF(STACK+8+axis*4)==factors[axis]);
        assert(fabsf(MEMF(MATRIX+axis*20)-sqrtf(1.f+2.f*(factors[axis]-1.f)))<.000001f);
    }
    puts("PASS: original character scale/rotation, slow time, pause, catch-up, cleanup and old-path counterexample");
    free(memory);return 0;
}
