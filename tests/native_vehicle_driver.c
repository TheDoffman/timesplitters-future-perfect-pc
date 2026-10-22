/* Original driver dispatcher and trigger/steering producer. Body wake, action
 * service and model lookup are controlled; this is not a live handling test. */
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
enum { PROP=0x1000, DATA=0x2000, PARAM=0x4000, BODY=0x5000,
       MATRIX=0x6000, INPUT=0x7000, DRIVER=0x8000, CHARACTER=0x9000,
       STACK=0x18000, HANDLE=1 };
static int legacy_driver;
static unsigned updates,wakes;
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((site)==0x17da03u && legacy_driver ? (value) : \
     pc_native_global_load((site),(address),(value)))
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) do { if((address)==0x172ee0u)++updates; } while(0)
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected callback")
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected switch target")
void sub_00165690(void) { g_eax=MATRIX;g_esp+=4; }
void sub_0017A440(void) { g_esp+=4; } /* weapon/action service, no buttons */
void sub_001679A0(void) { g_eax=0;g_esp+=4; } /* awake */
void sub_00167880(void) { assert(MEM32(g_esp+4)==BODY);++wakes;g_esp+=4; }
/* UNREACHED */
/* ROUTINES */
static void reset(void)
{
    memset((void *)(g_xbox_mem_offset+PROP),0,0xf000);
    updates=wakes=0;
    MEM32(PROP+0x7c)=DATA;MEM32(PROP+0x80)=BODY;
    MEM32(DATA+0x48)=BODY;MEM32(DATA+0x50)=PARAM;
    MEM32(DATA+0x140)=1;MEM32(DATA+0x70)=HANDLE;
    MEM32(DATA+0x60)=2;MEM32(DATA+0x5c)=1;
    MEMF(PARAM+4)=100;MEMF(PARAM+8)=40;MEMF(PARAM+0x54)=2;
    MEMF(PARAM+0x34)=1;
    MEM32(0x72f5e8)=DRIVER;MEM32(DRIVER+0x2c)=HANDLE;
    MEM32(DRIVER+0x7c)=CHARACTER;MEM32(INPUT+0x1c)=HANDLE;
}
static void step(unsigned whole,float delta,float forward,float reverse)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    MEMF(INPUT+0x10)=forward;MEMF(INPUT+0x14)=reverse;
    /* Vehicle simulation consumes and clears these each advancing update. */
    MEMF(DATA+0x530)=MEMF(DATA+0x534)=0;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=PROP;MEM32(STACK+8)=INPUT;
    g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=0xdef0;
    sub_0017DA00();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef && g_ebp==0xdef0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);
    legacy_driver=argc>1 && !strcmp(argv[1],"legacy-driver");
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    for(unsigned rate=0;rate<3;rate++)for(unsigned mode=0;mode<3;mode++) {
        unsigned fps=rate==0?60:rate==1?120:144;
        unsigned duration=mode==1?4:1,multiplier=mode==2?3:1;
        unsigned frames=fps*duration;
        g_tsfp_native_timing_active=rate!=0;reset();
        double impulse=0;unsigned expected_updates=0;
        for(unsigned frame=0;frame<frames;frame++) {
            unsigned whole=(frame+1)*60*multiplier/frames-frame*60*multiplier/frames;
            /* Original mode intentionally updates only on authored ticks. */
            float delta=g_tsfp_native_timing_active?60.f*multiplier/frames:(float)whole;
            step(whole,delta,1,0);
            if(delta>0)++expected_updates;
            impulse+=MEMF(DATA+0x530)*delta/60.;
        }
        printf("driver fps=%u mode=%u updates=%u impulse=%.9f\n",fps,mode,updates,impulse);
        assert(updates==expected_updates && wakes==expected_updates);
        assert(fabs(impulse-200.*multiplier)<.001);
        /* Release, analog throttle, braking and reverse are fresh, too. */
        unsigned changed_whole=g_tsfp_native_timing_active?0:1;
        step(changed_whole,.5f,.25f,0);assert(MEMF(DATA+0x530)==50 && MEMF(DATA+0x534)==0);
        step(changed_whole,.5f,0,1);assert(MEMF(DATA+0x530)==0 && MEMF(DATA+0x534)==40);
        MEM32(DATA+0x60)=1;
        step(changed_whole,.5f,0,.5f);assert(MEMF(DATA+0x530)==100 && MEMF(DATA+0x534)==0);
        step(changed_whole,.5f,1,0);assert(MEMF(DATA+0x530)==0 && MEMF(DATA+0x534)==40);
        step(changed_whole,.5f,0,0);assert(MEMF(DATA+0x530)==0 && MEMF(DATA+0x534)==0);
        unsigned previous=updates;
        step(0,0,1,0);if(g_tsfp_native_timing_active)step(1,0,1,0);
        assert(updates==previous && MEMF(DATA+0x530)==0 && MEMF(DATA+0x534)==0);
    }
    g_tsfp_native_timing_active=0;reset();step(0,.5f,1,0);
    assert(updates==0); /* Original gate remains original even with fractional input. */
    g_tsfp_native_timing_active=1;reset();
    const float intervals[]={.25f,0,.75f,1.5f,.125f,3.f,.375f};
    float elapsed=0,impulse=0;
    for(unsigned i=0;i<7;i++) {
        float delta=intervals[i];
        unsigned whole=(unsigned)(elapsed+delta)-(unsigned)elapsed;
        step(whole,delta,1,0);elapsed+=delta;
        impulse+=MEMF(DATA+0x530)*delta;
    }
    assert(updates==6 && fabsf(impulse-200.f*elapsed)<.001f);
    free(memory);puts("PASS: original driver dispatch, forward/reverse/braking, release, pause, clock speeds and ABI");
    return 0;
}
