/* Execute the complete original mothership updater and vector helpers.
 * Effects/audio/asset services are bounded stubs; random wobble is centered.
 * This isolates the flight/crash integrator, not rendered scene fidelity. */
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
enum { ENTITY=0x1000, MODEL=0x2000, CAMERA=0x3000, PLAYER=0x4000, STACK=0x10000 };
static unsigned spawns, retired, calls, changed;
static float wreck[6];
typedef struct { float position[3], velocity[3], wreck[6], seconds; } Result;
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected mothership callback")
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected mothership switch target")
#define EFFECT(name) void name(void) { g_eax=1;g_esp+=4; }
EFFECT(sub_000468A0)
EFFECT(sub_00046A00)
EFFECT(sub_000EA9C0)
EFFECT(sub_000FAC70)
EFFECT(sub_0010E210)
EFFECT(sub_0010E730)
EFFECT(sub_0010EAE0)
EFFECT(sub_0010EB70)
EFFECT(sub_0010EB90)
EFFECT(sub_0010EC20)
EFFECT(sub_00111AE0)
void sub_0015BA10(void) { g_eax=0;g_esp+=4; }
void sub_00153B40(void) { g_eax=0x80000000u;g_esp+=4; }
void sub_003C848C(void)
{
    g_eax=(uint32_t)(int32_t)g_fp_stack[g_fp_top];
    g_fp_top=(g_fp_top+1)&7;g_esp+=4;
}
void sub_000A14B0(void)
{
    uint32_t dest=MEM32(g_esp+4);
    for(unsigned i=0;i<16;i++) MEMF(dest+i*4)=i%5==0?1.f:0.f;
    g_esp+=4;
}
void sub_000A6F90(void)
{
    assert(MEM32(g_esp+4)==0x53a);
    uint32_t p=MEM32(g_esp+8), a=MEM32(g_esp+12);
    for(unsigned i=0;i<3;i++) {
        wreck[i]=MEMF(p+i*4);wreck[i+3]=MEMF(a+i*4);
        assert(isfinite(wreck[i]) && isfinite(wreck[i+3]));
    }
    spawns++;g_eax=0x5000;g_esp+=4;
}
void sub_000A9070(void)
{ assert(MEM32(g_esp+4)==ENTITY);retired++;g_esp+=4; }
/* GENERATED_ROUTINES */
static void prepare(unsigned state)
{
    memset((void *)((uintptr_t)ENTITY+g_xbox_mem_offset),0,0x4000);
    memset((void *)((uintptr_t)0x75c0d0+g_xbox_mem_offset),0,0x30);
    memset((void *)((uintptr_t)0x78d500+g_xbox_mem_offset),0,0x60);
    spawns=retired=calls=changed=0;
    MEM32(ENTITY+4)=MODEL;MEM32(ENTITY+0x1a0)=state;
    MEM32(ENTITY+0x28)=0x1000; /* use the supplied vertical lift vector */
    MEMF(ENTITY+0x64)=-80;MEMF(ENTITY+0x68)=100;MEMF(ENTITY+0x6c)=-220;
    MEMF(ENTITY+0xb4)=3;MEMF(ENTITY+0xb8)=-10;MEMF(ENTITY+0xbc)=5;
    MEMF(ENTITY+0x70)=180;MEMF(ENTITY+0xd0)=180;
    MEM32(0x7b0cc0)=CAMERA;MEM32(0x7b0c7c)=PLAYER;
    MEM32(0x790950)=0;MEM32(0x79094c)=2;MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<16;i++)MEMF(ENTITY+0x274+i*4)=i%5==0?1.f:0.f;
}
static void step(unsigned whole,float delta)
{
    float before=MEMF(ENTITY+0x64);
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xfeed;g_ebp=0xabcd;
    sub_00217050();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    if(MEMF(ENTITY+0x64)!=before)changed++;
    calls++;
}
static Result run(unsigned fps,unsigned slow,int native)
{
        Result result={0};
        g_tsfp_native_timing_active=native;
        prepare(3);
        for(unsigned frame=0;frame<fps*slow;frame++)
            step((frame+1)*60/(fps*slow)-frame*60/(fps*slow),60.f/(fps*slow));
        assert(!spawns && !retired && MEM32(ENTITY+0x1a0)==3);
        assert(changed==(native?fps*slow:60));
        for(unsigned i=0;i<3;i++) {
            result.position[i]=MEMF(ENTITY+0x64+i*4);
            result.velocity[i]=MEMF(ENTITY+0xb4+i*4);
        }
        printf("mothership_motion fps=%u slow=%u native=%d calls=%u changed=%u x=%.9f y=%.9f z=%.9f vx=%.9f vy=%.9f vz=%.9f\n",
               fps,slow,native,calls,changed,MEMF(ENTITY+0x64),MEMF(ENTITY+0x68),MEMF(ENTITY+0x6c),
               MEMF(ENTITY+0xb4),MEMF(ENTITY+0xb8),MEMF(ENTITY+0xbc));
        unsigned char before[0x2e8];
        void *entity=(void *)((uintptr_t)ENTITY+g_xbox_mem_offset);
        memcpy(before,entity,sizeof(before));
        step(0,0);assert(!memcmp(before,entity,sizeof(before)));
        prepare(3);
        unsigned frame=0;
        while(!retired && frame<fps*slow*120) {
            step((frame+1)*60/(fps*slow)-frame*60/(fps*slow),60.f/(fps*slow));frame++;
        }
        assert(spawns==1 && retired==1);
        result.seconds=(float)frame/(fps*slow);
        memcpy(result.wreck,wreck,sizeof(wreck));
        printf("mothership_crash fps=%u slow=%u native=%d game_seconds=%.6f x=%.9f y=%.9f z=%.9f pitch=%.9f yaw=%.9f roll=%.9f\n",
               fps,slow,native,result.seconds,wreck[0],wreck[1],wreck[2],wreck[3],wreck[4],wreck[5]);
        return result;
}
static void variable_intervals(const Result *reference,unsigned slow)
{
    /* Alternate short/long frames using exact integer clock quanta. This
     * exercises carried whole ticks without assuming perfectly paced FPS.
     * Pausing each crash phase must neither advance it nor spawn a wreck. */
    const unsigned intervals[]={5,4,7,3,9,6,2,8,10,6};
    unsigned elapsed=0,frames=0,paused_states=0;
    const unsigned quanta_per_tick=12*slow;
    g_tsfp_native_timing_active=1;prepare(3);
    while(!retired && frames<20000) {
        unsigned state=MEM32(ENTITY+0x1a0);
        assert(state>=3 && state<=5);
        if(!(paused_states&(1u<<state))) {
            unsigned char before[0x2e8];
            memcpy(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before));
            unsigned before_spawns=spawns,before_retired=retired;
            step(0,0);step(0,0);
            assert(!memcmp(before,(void *)((uintptr_t)ENTITY+g_xbox_mem_offset),sizeof(before)));
            assert(spawns==before_spawns && retired==before_retired);
            paused_states|=1u<<state;
        }
        unsigned duration=intervals[frames%10];
        unsigned whole=(elapsed+duration)/quanta_per_tick-elapsed/quanta_per_tick;
        step(whole,(float)duration/quanta_per_tick);
        elapsed+=duration;frames++;
    }
    float seconds=(float)elapsed/(60*quanta_per_tick);
    assert(spawns==1 && retired==1 && paused_states==((1u<<3)|(1u<<4)|(1u<<5)));
    assert(fabsf(seconds-reference->seconds)<.05f);
    for(unsigned i=0;i<3;i++) {
        assert(fabsf(wreck[i]-reference->wreck[i])<.75f);
        assert(fabsf(wreck[i+3]-reference->wreck[i+3])<.05f);
    }
    printf("mothership_variable slow=%u frames=%u game_seconds=%.6f x=%.9f y=%.9f z=%.9f paused_states=%u\n",
           slow,frames,seconds,wreck[0],wreck[1],wreck[2],paused_states);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+i*4)=sinf((float)i*6.28318530718f/2048);
    Result reference=run(60,1,0);
    Result broken=run(144,1,0); /* Demonstrate the original gate's regression. */
    assert(broken.seconds>reference.seconds*2.5f);
    assert(broken.wreck[1]-reference.wreck[1]>17);
    for(unsigned slow=1;slow<=4;slow+=3)
    for(unsigned fps=60;fps<=144;fps+=(fps==60?60:24)) {
        Result actual=run(fps,slow,1);
        /* The crash phases cross height/time thresholds on sampled frames.
         * Allow one original frame at each transition, not an altered path. */
        assert(fabsf(actual.seconds-reference.seconds)<.05f);
        for(unsigned i=0;i<3;i++) {
            assert(fabsf(actual.position[i]-reference.position[i])<.03f);
            assert(fabsf(actual.velocity[i]-reference.velocity[i])<.005f);
            assert(fabsf(actual.wreck[i]-reference.wreck[i])<.75f);
            assert(fabsf(actual.wreck[i+3]-reference.wreck[i+3])<.05f);
        }
        if(fps==60 && slow==1)assert(!memcmp(&actual,&reference,sizeof(actual)));
    }
    variable_intervals(&reference,1);
    variable_intervals(&reference,4);
    /* Existing multi-tick damping and catch-up retain their exact behavior. */
    unsigned char original[0x2e8];
    for(unsigned whole=1;whole<=6;whole++) {
        prepare(3);g_tsfp_native_timing_active=0;step(whole,(float)whole);
        memcpy(original,memory+ENTITY,sizeof(original));
        prepare(3);g_tsfp_native_timing_active=1;step(whole,(float)whole);
        assert(!memcmp(original,memory+ENTITY,sizeof(original)));
    }
    free(memory);
    puts("PASS: native mothership advances every live interval; crash trajectory, pause, catch-up and ABI checked");
    return 0;
}
