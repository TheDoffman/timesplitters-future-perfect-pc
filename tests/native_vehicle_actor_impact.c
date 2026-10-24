/* Original entity velocity publication -> vehicle actor-contact callback.
 * Exercise direct and original queued event delivery, real point velocity,
 * contact construction and impulse selection. Motion, broadphase geometry,
 * allocation and the final effect consumer are controlled. The original
 * suspension impulse consumer and its transform/mass helpers execute too.
 * Each case is an independent contact, not a repeated-contact simulation. */
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
enum { BODY=0x5000, VEHICLE=0x6000, ACTOR=0x6400, CHARACTER=0x6800,
       EVENT=0x7000, QUEUE=0x7100, EVENTS=0x7200, DATA=0x8000, STACK=0x18000 };
static int legacy,open_gate;
static unsigned notifications,impulses,effects,frees;
static float reported,impulse[3];
static void trace(unsigned address)
{
    if(address==0x17bc90u) {++notifications;reported=MEMF(g_esp+12);}
    if(address==0x174ec0u) {
        assert(MEM32(g_esp+4)==VEHICLE);++impulses;
        for(unsigned i=0;i<3;i++)impulse[i]=MEMF(MEM32(g_esp+12)+4*i);
    }
}
static void indirect(uint32_t target,uint32_t stack,uint32_t site);
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) trace(address)
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) indirect(target,stack,site)
#undef RECOMP_F32_DIV_AT
#define RECOMP_F32_DIV_AT(site,left,right) \
    ((site)==0x17cbd6u && legacy ? (left)/(right) : pc_native_divide(site,left,right))
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((site)==0x17cbafu && open_gate && g_tsfp_native_timing_active ? \
     (MEMF(0x4e794c)>0) : pc_native_global_load(site,address,value))
void sub_00057040(void) {g_esp+=4;}
void sub_0009FBE0(void) {g_esp+=4;}
void sub_0009FD60(void) {g_esp+=4;}
void sub_000A5F40(void) {g_esp+=4;}
void sub_000A6290(void) {g_esp+=4;}
void sub_000A64D0(void) {g_esp+=4;}
void sub_000A6530(void) {g_esp+=4;}
void sub_00129920(void) {g_esp+=4;}
void sub_0012C1F0(void) {g_esp+=4;}
void sub_00238510(void) {g_esp+=4;}
void sub_00179F70(void) {assert(MEM32(g_esp+4)==VEHICLE);++effects;g_esp+=4;}
void sub_0003E1E0(void)
{
    assert(MEM32(g_esp+4)==QUEUE || MEM32(g_esp+4)==EVENTS);++frees;g_esp+=4;
}
/* UNREACHED */
/* ROUTINES */
static void indirect(uint32_t target,uint32_t stack,uint32_t site)
{
    assert(site==0x16c126 && target==0x17cae0);
    sub_0017CAE0();assert(g_esp==stack-8);
}
static void check(float producer_delta,unsigned whole,float event_delta,int direction,int queued)
{
    memset((void *)(g_xbox_mem_offset+BODY),0,0x4000);
    MEM32(0x74c458)=BODY-512;MEM32(VEHICLE+0x80)=1;
    MEM32(VEHICLE+0x7c)=DATA;MEMF(BODY+0x1a8)=.5f;
    for(unsigned i=0;i<16;i++)MEMF(BODY+i*4)=i%5==0?1.f:0.f;
    MEM32(ACTOR+0x20)=8;MEM32(ACTOR+0x7c)=CHARACTER;
    MEM32(0x4e8cec)=60;MEM32(0x4e7944)=1;MEMF(0x4e794c)=producer_delta;
    /* The original post-update writes units per authored tick, not displacement. */
    int moving=producer_delta>0?direction:0;
    const float speed[]={6.f*moving,-3.f*moving,2.f*moving};
    for(unsigned i=0;i<3;i++)MEMF(ACTOR+0x64+4*i)=speed[i]*producer_delta/60.f;
    g_esp=STACK;g_eax=ACTOR;g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=g_seh_ebp=0xdef0;
    sub_000A7360();assert(g_esp==STACK+4);
    for(unsigned i=0;i<3;i++)assert(fabsf(MEMF(ACTOR+0xf8+4*i)-speed[i]/60.f)<1e-6f);
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=event_delta;
    g_esp=STACK;MEM32(STACK+4)=VEHICLE;MEM32(STACK+8)=7;MEM32(STACK+12)=0x17cae0;
    sub_000A5CE0();assert(MEM32(VEHICLE+0x170)==0x17cae0 && g_esp==STACK+4);
    MEM32(EVENT)=ACTOR;MEMF(EVENT+4)=.6f;MEMF(EVENT+8)=.8f;
    notifications=impulses=effects=frees=0;memset(impulse,0,sizeof impulse);
    g_esp=STACK;MEM32(STACK)=0x12345678;
    if(queued) {
        MEM32(0x74c484)=QUEUE;MEM32(0x74c474)=EVENTS;MEM32(0x74c47c)=1;
        g_ecx=EVENT;MEM32(STACK+4)=VEHICLE;sub_0016C070();assert(g_esp==STACK+4);
        assert(MEM32(0x74c490)==1);g_esp=STACK;sub_0016C0D0();
        assert(frees==2 && MEM32(0x74c490)==0 && MEM32(0x74c484)==0 && MEM32(0x74c474)==0);
    } else {MEM32(STACK+4)=VEHICLE;MEM32(STACK+8)=EVENT;sub_0017CAE0();}
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef);
    float expected=.6f*speed[0]+.8f*speed[2];
    if(!g_tsfp_native_timing_active && whole>0)expected/=event_delta;
    if(fabsf(reported-expected)>=.00001f)printf("contact whole=%u delta=%g queued=%d normal=%g expected=%g\n",whole,event_delta,queued,reported,expected);
    assert(notifications==1 && fabsf(reported-expected)<.00001f);
    assert(impulses==(unsigned)(expected<0) && effects==impulses);
    const float normal[]={-.6f,0,-.8f};
    for(unsigned i=0;i<3;i++)assert(fabsf(impulse[i]-(expected<0?normal[i]*expected*MEMF(0x47de1c):0))<.0001f);
    /* Horizontal impact produces opposite changes in opposing components. */
    float factor=MEMF(0x475d4c)*.5f;
    const float component[]={-impulse[2]*factor,impulse[0]*factor,impulse[2]*factor,-impulse[0]*factor};
    for(unsigned i=0;i<4;i++)assert(fabsf(MEMF(DATA+0x4a0+0x18*i)-component[i])<.0001f);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==event_delta);
}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);legacy=argc>1;open_gate=legacy && !strcmp(argv[1],"open-gate-only");
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    const unsigned rates[]={60,120,144};
    for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];g_tsfp_native_timing_active=rate!=0;
        printf("actor_impact fps=%u\n",fps);
        for(unsigned frame=0;frame<fps;frame++)for(int direction=-1;direction<=1;direction++)for(int queued=0;queued<2;queued++) {
            unsigned whole=(frame+1)*60/fps-frame*60/fps;
            check(60.f/fps,whole,60.f/fps,direction,queued);
        }
    }
    /* Producer and consumer can be adjacent frames of different lengths. */
    const float intervals[]={.125f,.5f,1.5f,3.f,.375f};
    for(unsigned i=0;i<5;i++)for(unsigned j=0;j<5;j++)for(int queued=0;queued<2;queued++)
        check(intervals[i],1,intervals[j],1,queued);
    for(int queued=0;queued<2;queued++) {check(0,0,0,1,queued);check(0,1,0,1,queued);}
    g_tsfp_native_timing_active=0;check(.5f,1,.5f,1,0);check(3,3,3,1,1);
    free(memory);puts("PASS: original entity publication, direct/queued actor contact and impulse selection");return 0;
}
