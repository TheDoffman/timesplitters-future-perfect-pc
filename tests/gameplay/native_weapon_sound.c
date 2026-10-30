/* Original US weapon sound selection/timer plus both sound parameter builders.
 * The audio backend, weapon callbacks and RNG are mocked. This does not test
 * firing rate, projectile creation, the enclosing weapon state machines or
 * audible output. */
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
enum { WEAPON=0x1000, TABLE=0x3000, POSITION=0x5000, STACK=0x7000,
       SOUND=TABLE+0x68+0x34 };
static unsigned sounds,random_calls;
static uint32_t sound_id;
static unsigned char parameters[48];
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target, stack, site) assert(!"Unexpected weapon callback")
void sub_001CD040(void) { g_eax=0;g_esp+=4; }
void sub_00153B40(void) { random_calls++;g_eax=0x80000000u;g_esp+=4; }
void sub_00045820(void)
{
    uint32_t address=MEM32(g_esp+4);
    memcpy(parameters,(void *)((uintptr_t)address+g_xbox_mem_offset),48);
    sound_id=MEM32(address);sounds++;g_eax=17;g_esp+=4;
}
/* GENERATED_ROUTINES */
static void call(unsigned whole,float delta,unsigned repeating,uint32_t position)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=WEAPON;MEM32(STACK+8)=0;
    MEM32(STACK+12)=position;MEM32(STACK+16)=repeating;
    g_esi=0xdead;g_edi=0xbeef;g_ebp=0xfeed;g_ebx=0xabcd;
    sub_001B6FF0();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebp==0xfeed && g_ebx==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void check(int condition,const char *message)
{
    if(!condition) {fprintf(stderr,"FAIL: weapon sound %s\n",message);abort();}
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144,180,240};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4f9bac)=TABLE;MEM32(0x4e8cec)=60;
    MEMF(0x7e71f0)=.75f;
    MEM32(SOUND)=0x123;MEM32(SOUND+4)=0x124;MEMF(SOUND+8)=10;
    MEMF(POSITION)=1;MEMF(POSITION+4)=2;MEMF(POSITION+8)=3;
    /* A one-shot invocation must not be dropped just because its time interval
       has not yet carried a whole tick. It must still produce only one sound. */
    for(int native=0;native<=1;native++) {
        g_tsfp_native_timing_active=native;
        for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) {
            unsigned before=sounds;
            call(0,60.f/rates[rate],0,POSITION);
            check(sounds==before+(native?1:0),"fractional one-shot dispatch");
            if(native) {
                check(sound_id==0x123 && MEM32(WEAPON+0x100)==17,"one-shot identity/handle");
                float position[3];memcpy(position,parameters+0x1c,12);
                check(parameters[0x28]==1 && position[0]==1 && position[1]==2 && position[2]==3,
                      "positional sound parameters");
                float range;memcpy(&range,parameters+0x2c,4);
                check(range==50,"original gunfire range field");
            }
            before=sounds;call(1,1,0,0);
            check(sounds==before+1 && sound_id==0x123 && parameters[0x28]==0,"non-positional sound");
        }
    }
    unsigned original_sounds=0,original_random=0;
    float original_timer=0;
    for(int native=0;native<=1;native++) {
        unsigned before=sounds,rng=random_calls;
        g_tsfp_native_timing_active=native;MEMF(WEAPON+0xfc)=0;MEMF(SOUND+8)=.1f;
        for(unsigned frame=0;frame<120;frame++) call(1,1,1,POSITION);
        if(!native) {
            original_sounds=sounds-before;original_random=random_calls-rng;
            original_timer=MEMF(WEAPON+0xfc);
        } else {
            check(sounds-before==original_sounds && random_calls-rng==original_random &&
                  MEMF(WEAPON+0xfc)==original_timer,"original one-tick cadence");
        }
    }
    /* Repeating timers consume every live interval; preserve the original
       reset-at-trigger behavior and its one-update threshold quantization. */
    for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned fps=rates[rate],before=sounds;
        float delta=60.f/(fps*slow);
        MEMF(WEAPON+0xfc)=0;MEMF(SOUND+8)=10;
        for(unsigned f=0;f<fps;f++)
            call((f+1)*60/(fps*slow)-f*60/(fps*slow),delta,1,POSITION);
        check(sounds==before && fabsf(MEMF(WEAPON+0xfc)-1.f/slow)<.00001f,
              "elapsed repeating time");
        float timer=MEMF(WEAPON+0xfc);unsigned rng=random_calls;
        call(0,0,1,POSITION);call(1,0,1,POSITION);
        check(sounds==before && random_calls==rng && MEMF(WEAPON+0xfc)==timer,"pause");
        MEMF(SOUND+8)=.5f;MEMF(WEAPON+0xfc)=.5f-delta/120;
        call(0,delta,1,POSITION);
        check(sounds==before+1 && sound_id==0x124 && MEMF(WEAPON+0xfc)==0,"repeat threshold/reset");
    }
    MEMF(SOUND+8)=10;MEMF(WEAPON+0xfc)=0;
    call(3,3,1,POSITION);call(2,2.5f,1,POSITION);
    check(fabsf(MEMF(WEAPON+0xfc)-5.5f/60)<.000001f,"catch-up duration");
    unsigned before=sounds,rng=random_calls;
    MEM32(SOUND)=UINT32_MAX;call(0,.5f,0,POSITION);
    check(sounds==before && random_calls==rng,"missing sound");
    assert(RECOMP_LOAD_GLOBAL_U32(0x1b6ff1,0x4e7944,7)==7);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0x1b6ff0,0x4e7944,3)==3);
    free(memory);
    puts("PASS: original weapon sound dispatch, elapsed repeat timer, pause and parameters at 60/120/144/180/240");
    return 0;
}
