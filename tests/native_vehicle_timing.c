static const unsigned test_rates[]={60,120,144,180,240};
/* Execute the full original vehicle dispatcher and suspension update.
 * World/driver/asset services are controlled. This is not a handling test. */
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
       MATRIX=0x6000, POSITION=0x7000, MODEL=0x8000, TABLE=0x9000,
       VERTEX=0xa000, STACK=0x18000 };
static int legacy_gate,legacy_linear,legacy_rest;
static unsigned updates;
static unsigned sleep_calls;
static double integrated_seconds;
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((site)==0x17e6c3u && legacy_gate ? (value) : \
     pc_native_global_load((site),(address),(value)))
#undef RECOMP_PUSH_IMM_U32_AT
#define RECOMP_PUSH_IMM_U32_AT(site,value) \
    ((site)==0x17e6ecu && legacy_linear ? (value) : pc_native_push_imm(site,value))
#undef RECOMP_LOAD_U8_AT
#define RECOMP_LOAD_U8_AT(site,value) \
    ((site)==0x176673u && legacy_rest ? (value) : pc_native_load_u8(site,value))
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) do { if((address)==0x175150u) { \
    ++updates; integrated_seconds+=(double)MEMF(0x4e794c)/MEM32(0x4e8cec); } } while(0)
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected callback")
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected switch target")
void sub_001679F0(void) { g_eax=POSITION;g_esp+=4; }
void sub_00165690(void) { g_eax=MATRIX;g_esp+=4; }
void sub_00167A30(void) { g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=1;g_esp+=4; }
void sub_001679A0(void) { g_eax=1;g_esp+=4; }
void sub_0017D1D0(void) { g_esp+=4; } /* damage/effects service */
void sub_00179F70(void) { g_eax=0;g_esp+=4; } /* no driver */
void sub_00179C30(void) { g_esp+=4; } /* model service */
void sub_0017CCD0(void) { g_eax=0;g_esp+=4; } /* rest-state eligibility */
void sub_00167A10(void) { g_eax=POSITION+0x20;g_esp+=4; }
void sub_001677E0(void) { ++sleep_calls;g_esp+=4; }
void sub_00167880(void) { fprintf(stderr,"Unexpected wake\n");abort(); }
/* UNREACHED */
/* ROUTINES */
static void reset(void)
{
    memset((void *)(g_xbox_mem_offset+PROP),0,0xf000);
    updates=0;integrated_seconds=0;
    MEM32(PROP+0x7c)=DATA;MEM32(PROP+0x80)=BODY;MEM32(PROP+4)=MODEL;
    MEM32(DATA+0x50)=PARAM;
    MEM32(MODEL+4)=TABLE;MEM32(TABLE+0x78)=VERTEX;
    MEMF(VERTEX+4)=1;
    MEMF(DATA+0x4dc)=1;MEMF(DATA+0x4ac)=0;
    MEMF(DATA+0x49c)=1;MEMF(DATA+0x4cc)=0;
    MEMF(PARAM+0x68)=-100;MEMF(PARAM+0x6c)=100;
    MEMF(PARAM+0x7c)=-100;MEMF(PARAM+0x80)=100;
    for(unsigned i=0;i<16;i++)MEMF(MATRIX+4*i)=i%5==0?1:0;
    for(unsigned i=0;i<4;i++) {
        MEMF(DATA+0x498+i*0x18)=0.25f; /* spring displacement */
        MEMF(DATA+0x4a0+i*0x18)=1.f; /* velocity */
        MEMF(DATA+0x4a4+i*0x18)=2.f; /* frequency */
        MEMF(DATA+0x4a8+i*0x18)=0.5f; /* damping */
    }
    MEMF(DATA+0x96c)=20;
    MEM32(0x4e8cec)=60;MEM32(0x790950)=0;
}
static void step_time(unsigned raw_whole,unsigned whole,float raw,float delta)
{
    MEM32(0x4e7940)=raw_whole;MEMF(0x4e7948)=raw;
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=PROP;
    g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=0xdef0;
    sub_0017E680();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef && g_ebp==0xdef0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void step(unsigned whole,float delta) {step_time(whole,whole,delta,delta);}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);
    legacy_gate=argc>1 && !strcmp(argv[1],"legacy-gate");
    legacy_linear=argc>1 && !strcmp(argv[1],"legacy-linear");
    legacy_rest=argc>1 && !strcmp(argv[1],"legacy-rest");
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    for(unsigned rate=0;rate<sizeof test_rates/sizeof test_rates[0];rate++) {
        unsigned fps=test_rates[rate];
        g_tsfp_native_timing_active=rate!=0;reset();
        for(unsigned frame=0;frame<fps;frame++)
            step((frame+1)*60/fps-frame*60/fps,60.f/fps);
        printf("vehicle fps=%u updates=%u seconds=%.9f displacement=%.9f velocity=%.9f linear_state=%.9f\n",
            fps,updates,integrated_seconds,MEMF(DATA+0x498),MEMF(DATA+0x4a0),MEMF(DATA+0x96c));
        assert(updates==fps);
        assert(fabs(integrated_seconds-1.)<1e-6);
        assert(fabsf(MEMF(DATA+0x96c)-14.f)<.001f);
        assert(isfinite(MEMF(DATA+0x498)) && isfinite(MEMF(DATA+0x4a0)));
        unsigned previous_updates=updates;
        float previous=MEMF(DATA+0x498),linear=MEMF(DATA+0x96c);
        step(0,0);if(g_tsfp_native_timing_active)step(1,0);
        assert(updates==previous_updates && MEMF(DATA+0x498)==previous && MEMF(DATA+0x96c)==linear);
    }
    /* Execute the original rest countdown independently, under the same
     * outer cadence. Its actual DEC byte is not a fractional time consumer. */
    for(unsigned rate=0;rate<sizeof test_rates/sizeof test_rates[0];rate++) {
        unsigned fps=test_rates[rate], frame=0;
        g_tsfp_native_timing_active=rate!=0;reset();sleep_calls=0;MEM8(MATRIX+0x1c8)=60;MEM32(DATA+0x58)=0x10000;
        for(;frame<fps*2 && !sleep_calls;frame++) {
            unsigned whole=(frame+1)*60/fps-frame*60/fps;
            if(!g_tsfp_native_timing_active && !whole)continue;
            MEM32(0x4e7944)=MEM32(0x4e7940)=whole;
            MEMF(0x4e794c)=MEMF(0x4e7948)=60.f/fps;
            g_eax=PROP;g_esp=STACK;MEM32(STACK)=0x12345678;
            g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=0xdef0;
            sub_00176530();
            assert(g_esp==STACK+4 && g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef && g_ebp==0xdef0);
        }
        assert(sleep_calls==1 && frame==fps);
        printf("vehicle_rest fps=%u frame=%u sleep_seconds=%.9f\n",fps,frame,(double)frame/fps);
    }
    for(unsigned rate=1;rate<sizeof test_rates/sizeof test_rates[0];rate++)for(unsigned fast=0;fast<2;fast++) {
        unsigned fps=test_rates[rate];
        unsigned duration=fast?1:4,multiplier=fast?3:1;
        unsigned frames=fps*duration;
        g_tsfp_native_timing_active=1;reset();
        for(unsigned frame=0;frame<frames;frame++)
            step_time((frame+1)*60/fps-frame*60/fps,
                      (frame+1)*60*multiplier/frames-frame*60*multiplier/frames,
                      60.f/fps,60.f*multiplier/frames);
        assert(updates==frames && fabs(integrated_seconds-multiplier)<1e-6);
        assert(fabsf(MEMF(DATA+0x96c)-14.f)<.001f);
        assert(isfinite(MEMF(DATA+0x498)) && isfinite(MEMF(DATA+0x4a0)));
        printf("vehicle_speed fps=%u speed=%g updates=%u seconds=%.9f linear_state=%.9f\n",
               fps,(float)multiplier/duration,updates,integrated_seconds,MEMF(DATA+0x96c));
    }
    free(memory);puts("PASS: original vehicle suspension elapsed time, adjustment, rest cadence, ABI and pause; world/driver services controlled");return 0;
}
