/* Characterize the complete original generic firing routine and its original
 * integer cooldown update. Actor/socket data,
 * projectile/effect creation and the audio backend are mocked, so this is not damage,
 * ammunition, rendered-weapon or full caller/state-machine acceptance. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
/* FIRE_POLICY */
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { ACTOR=0x1000, STATE=0x3000, WEAPON=0x5000, TABLE=0x8000,
       INFO=0xa000, STACK=0x10000, MODE=TABLE+0x68 };
static unsigned projectiles,sounds,effects,random_calls,force_gate;
static unsigned fire_mode,repeating;
static float expected_event_delta;
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected indirect weapon callback")
static uint32_t audit_load(uint32_t site,uint32_t address,uint32_t value)
{
    if(force_gate && site==0x244fcdu && address==0x4e7944u &&
       g_tsfp_native_timing_active) return MEMF(0x4e794c)>0;
    return pc_native_global_load(site,address,value);
}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) audit_load(site,address,value)
void sub_000B3CD0(void) { g_eax=INFO;g_esp+=4; }
void sub_000B4680(void)
{
    assert(MEM32(g_esp+4)==ACTOR && MEM32(g_esp+8)==WEAPON);
    uint32_t position=MEM32(g_esp+20);
    MEMF(position)=1;MEMF(position+4)=2;MEMF(position+8)=3;
    g_eax=1;g_esp+=4;
}
void sub_000B6140(void)
{
    uint32_t direction=MEM32(g_esp+20);
    MEMF(direction)=0;MEMF(direction+4)=0;MEMF(direction+8)=1;
    g_esp+=4;
}
void sub_00153B40(void) { random_calls++;g_eax=0x80000000u;g_esp+=4; }
void sub_00059D20(void) { g_eax=0;g_esp+=4; }
void sub_001B7680(void)
{
    assert(MEM32(g_esp+4)==ACTOR && MEM32(g_esp+8)==WEAPON);
    assert(MEMF(0x4e794c)==expected_event_delta);
    projectiles++;g_eax=0;g_esp+=4;
}
void sub_000DFEC0(void) { effects++;g_esp+=4; }
void sub_000389B0(void) { g_esp+=4; }
void sub_001CD040(void) { g_eax=0;g_esp+=4; }
void sub_00045820(void)
{
    uint32_t params=MEM32(g_esp+4);
    assert(MEM32(params)==(repeating?0x124u:0x123u));
    assert(MEMF(0x4e794c)==expected_event_delta);
    sounds++;g_eax=17;g_esp+=4;
}
#define UNUSED(name) void name(void) { assert(!"Unexpected " #name); }
UNUSED(sub_00242C00)
UNUSED(sub_002446F0)
UNUSED(sub_002445C0)
UNUSED(sub_00242C20)
UNUSED(sub_00153440)
UNUSED(sub_00152090)
UNUSED(sub_00153490)
UNUSED(sub_000A14B0)
UNUSED(sub_0010E700)
UNUSED(sub_00238510)
UNUSED(sub_00045CF0)
UNUSED(sub_001B7E80)
UNUSED(sub_0010E6F0)
/* GENERATED_ROUTINES */
void sub_00244F30(void)
{
    if(force_gate) sub_00244F30_gen(); /* Deliberately unsafe audit alternative. */
    else pc_native_weapon_fire_guest(sub_00244F30_gen);
}
static void call(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    expected_event_delta=g_tsfp_native_timing_active && !force_gate ?
                         (delta>0?(float)whole:0):delta;
    g_eax=WEAPON;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    sub_001BC890();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=ACTOR;MEM32(STACK+8)=WEAPON;
    MEM32(STACK+12)=fire_mode;MEM32(STACK+16)=repeating;
    MEM32(STACK+20)=0;MEM32(STACK+24)=0;
    g_ebp=0xdead;g_edi=0xbeef;g_esi=0xfeed;g_ebx=0xabcd;
    sub_00244F30();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebp==0xdead && g_edi==0xbeef && g_esi==0xfeed && g_ebx==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
typedef struct Snapshot {
    unsigned shots,audio,visual,rng,primary,secondary;
    float timer,phase,sound_timer;
} Snapshot;
static Snapshot snapshot(void)
{
    Snapshot value={projectiles,sounds,effects,random_calls,
        MEM32(WEAPON+0xf0),MEM32(WEAPON+0xf4),
        MEMF(WEAPON+0x104),MEMF(WEAPON+0x108),MEMF(WEAPON+0xfc)};
    return value;
}
static void equal(Snapshot a,Snapshot b)
{
    assert(a.shots==b.shots && a.audio==b.audio && a.visual==b.visual &&
           a.rng==b.rng && a.primary==b.primary && a.secondary==b.secondary);
    assert(fabsf(a.timer-b.timer)<.00001f && fabsf(a.phase-b.phase)<.00001f &&
           fabsf(a.sound_timer-b.sound_timer)<.00001f);
}
static void prepare(unsigned mode,unsigned repeat,unsigned pellets,unsigned phase,float interval)
{
    memset((void *)((uintptr_t)WEAPON+g_xbox_mem_offset),0,0x130);
    memset((void *)((uintptr_t)TABLE+g_xbox_mem_offset),0,0x29c);
    MEM32(WEAPON+0xec)=STATE;
    MEM32(TABLE+0xc)=phase?0x200:0;
    MEM32(MODE+8)=pellets;
    MEMF(MODE+0x10)=MEMF(MODE+0x14)=interval;
    MEM32(MODE+0x34)=0x123;MEM32(MODE+0x38)=0x124;MEMF(MODE+0x3c)=.1f;
    memcpy((void *)((uintptr_t)MODE+0x7c+g_xbox_mem_offset),
           (void *)((uintptr_t)MODE+g_xbox_mem_offset),0x7c);
    if(mode==2) MEM32(MODE+0x7c)=UINT32_MAX; /* Secondary falls back to primary. */
    fire_mode=mode!=0;repeating=repeat;force_gate=0;
    projectiles=sounds=effects=random_calls=0;
}
static void check_scenarios(void)
{
    const unsigned rates[]={60,120,144,180,240};
    unsigned scenarios=0;
    for(unsigned slow=1;slow<=4;slow*=4) for(unsigned mode=0;mode<3;mode++)
    for(unsigned repeat=0;repeat<2;repeat++) for(unsigned pellets=1;pellets<=3;pellets+=2)
    for(unsigned phase=0;phase<2;phase++) for(unsigned interval=0;interval<2;interval++) {
        Snapshot reference={0};
        for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) {
            unsigned fps=rates[rate];
            prepare(mode,repeat,pellets,phase,interval?.1f:0);
            g_tsfp_native_timing_active=rate!=0;
            for(unsigned frame=0;frame<fps;frame++) {
                unsigned whole=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
                /* The original clock supplies matching integer/float ticks.
                 * A quarter-speed schedule carries 15 game ticks per second. */
                call(whole,rate?60.f/(fps*slow):(float)whole);
                Snapshot before=snapshot();
                call(0,0);
                equal(before,snapshot());
            }
            if(!rate) reference=snapshot();else equal(reference,snapshot());
        }
        scenarios++;
        /* Compare identical carried-tick schedules with fractional and
         * original integer float deltas, including three-tick catch-up. */
        const unsigned whole[]={0,1,1,1,3,0,0,1};
        const float delta[]={.25f,.75f,1.5f,.5f,3,0,.25f,.75f};
        for(unsigned native=0;native<2;native++) {
            prepare(mode,repeat,pellets,phase,interval?.1f:0);
            g_tsfp_native_timing_active=native;
            for(unsigned round=0;round<20;round++) for(unsigned step=0;step<8;step++)
                call(whole[step],native?delta[step]:(float)whole[step]);
            if(!native) reference=snapshot();else equal(reference,snapshot());
        }
    }
    /* A local zero-float pause must suppress the transaction even if a
     * caller retained a whole tick. Counter update is outside this wrapper. */
    prepare(0,0,1,0,0);g_tsfp_native_timing_active=1;
    call(1,0);
    assert(!projectiles && !sounds && !effects && !random_calls &&
           MEMF(WEAPON+0x104)==0 && MEMF(WEAPON+0xfc)==0);
    printf("PASS: %u weapon configurations, original nested sound timers, quarter speed, pause, variable steps and catch-up\n",
           scenarios);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144,180,240};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4f9bac)=TABLE;MEM32(0x4e8cec)=60;
    MEM32(ACTOR+0x7c)=STATE;MEM32(ACTOR+0x28)=0x200000;
    MEM32(STATE+8)=0x40;MEM32(STATE+0xb0)=1;
    MEM32(STATE+0x94)=1;MEM32(0x4fa174)=UINT32_MAX;
    MEMF(STATE+0xb08)=MEMF(STATE+0xb0c)=1;
    MEM32(MODE+8)=1;
    MEM32(MODE+0x34)=0x123;MEM32(MODE+0x38)=0x124;
    MEMF(MODE+0x3c)=.1f;
    for(unsigned burst=0;burst<2;burst++) for(unsigned gate=0;gate<2;gate++)
    for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) {
        unsigned fps=rates[rate];
        memset((void *)((uintptr_t)WEAPON+g_xbox_mem_offset),0,0x130);
        MEM32(WEAPON+0xec)=STATE;
        MEMF(MODE+0x10)=burst?.1f:0;
        projectiles=sounds=effects=random_calls=0;
        force_gate=gate;g_tsfp_native_timing_active=rate!=0;
        for(unsigned frame=0;frame<fps;frame++)
            call((frame+1)*60/fps-frame*60/fps,60.f/fps);
        float timer=MEMF(WEAPON+0x104);
        printf("fire_audit fps=%u native=%d open_gate=%u interval=%.3f shots=%u sounds=%u rng=%u timer=%.9f\n",
               fps,g_tsfp_native_timing_active,gate,MEMF(MODE+0x10),
               projectiles,sounds,random_calls,timer);
        assert(effects==projectiles && sounds==projectiles && random_calls==4*projectiles);
        if(!rate && !gate) {
            assert(projectiles==(burst?10:60));
        }
        /* Python compares the unmodified native path with the 60-Hz
         * reference. Never assert that the known timer discrepancy persists. */
        if(gate && !burst) assert(projectiles==fps);
        unsigned before=projectiles;
        call(0,0);
        assert(projectiles==before && MEMF(WEAPON+0x104)==timer);
    }
    check_scenarios();
    free(memory);
    puts("PASS: firing audit execution and callback/ABI checks; parity evaluated separately");
    return 0;
}
