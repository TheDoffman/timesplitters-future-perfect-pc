/* Execute the complete original NPC firing routine with its real sound
 * selector/builders. Actor services, projectile construction, effects and
 * audio output are mocked. Caller cadence and damage are not covered. */
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
enum { ACTOR=0x1000, STATE=0x3000, TARGET=0x6000, TABLE=0x8000,
       STACK=0x10000, MODE=TABLE+0x68 };
static unsigned projectiles,sounds,effects,random_calls;
static unsigned hand,mode,repeating,scoped;
static int animation_fixed=1;
static uint32_t weapon;
static float expected_delta;
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected indirect NPC weapon callback")
void sub_00153B40(void) { random_calls++;g_eax=0x80000000u;g_esp+=4; }
void sub_001CD040(void) { g_eax=0;g_esp+=4; }
void sub_000DC270(void) { g_eax=0;g_esp+=4; }
void sub_000DC250(void) { g_eax=0;g_esp+=4; }
void sub_001B7680(void)
{
    assert(MEM32(g_esp+4)==ACTOR && MEM32(g_esp+8)==weapon);
    assert(MEMF(0x4e794c)==expected_delta);
    uint32_t direction=MEM32(g_esp+32);
    assert(isfinite(MEMF(direction)) && isfinite(MEMF(direction+4)) &&
           isfinite(MEMF(direction+8)));
    projectiles++;g_eax=0;g_esp+=4;
}
void sub_00045820(void)
{
    uint32_t params=MEM32(g_esp+4);
    assert(MEM32(params)==(repeating?0x124u:0x123u));
    assert(MEMF(0x4e794c)==expected_delta);
    sounds++;g_eax=17;g_esp+=4;
}
void sub_000DFEC0(void) { effects++;g_esp+=4; }
#define UNUSED(name) void name(void) { assert(!"Unexpected " #name); }
UNUSED(sub_000124F0)
UNUSED(sub_003C848C)
UNUSED(sub_00355EC0)
UNUSED(sub_000B91F0)
UNUSED(sub_000BD0F0)
UNUSED(sub_002B8EA0)
UNUSED(sub_00088C60)
UNUSED(sub_0010E700)
/* Original event selector reads these controlled animation bounds and table. */
enum { ANIM=0xa000, EVENT_TABLE=0xa100 };
static unsigned event_previous,event_current,event_caller;
void sub_00059CF0(void) { g_eax=ANIM;g_esp+=4; }
void sub_00056FC0(void) { g_eax=EVENT_TABLE;g_esp+=4; }
void sub_000585E0(void) {
    MEM32(MEM32(g_esp+8))=event_current;
    MEM32(MEM32(g_esp+12))=event_previous;
    g_esp+=4;
}
/* GENERATED_ROUTINES */
void sub_001BB5C0(void) {
    if(animation_fixed) pc_native_npc_weapon_fire_guest(sub_001BB5C0_gen,MEM32(g_esp));
    else pc_native_weapon_fire_guest(sub_001BB5C0_gen);
}
static void call(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    expected_delta=scoped && g_tsfp_native_timing_active ?
                   (delta>0?(float)whole:0):delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=ACTOR;MEM32(STACK+8)=hand;
    MEM32(STACK+12)=mode;MEM32(STACK+16)=repeating;
    g_ebp=0xdead;g_edi=0xbeef;g_esi=0xfeed;g_ebx=0xabcd;
    /* Unscoped execution remains solely as a pre-fix regression reproducer. */
    if(scoped) sub_001BB5C0();
    else sub_001BB5C0_gen();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebp==0xdead && g_edi==0xbeef && g_esi==0xfeed && g_ebx==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void prepare(unsigned selected_hand,unsigned selected_mode,unsigned repeat,unsigned pellets)
{
    memset((void *)((uintptr_t)STATE+g_xbox_mem_offset),0,0x1000);
    memset((void *)((uintptr_t)TABLE+g_xbox_mem_offset),0,0x29c);
    hand=selected_hand;mode=selected_mode!=0;repeating=repeat;
    weapon=STATE+(hand?0x1d4:0xa4);
    MEM32(ACTOR+0x7c)=STATE;MEM32(ACTOR+0x34)=TARGET;
    MEM32(ACTOR+0x150)=0;
    MEM32(STATE)=ACTOR;
    MEM32(STATE+4)=TARGET;MEM32(STATE+8)=0x40;
    MEM32(STATE+0x18)=1;
    MEM16(STATE+0x61e)=1000;
    MEMF(TARGET+0x6c)=10;
    MEMF(STATE+0xb90)=10;MEMF(STATE+0xb84)=1;
    MEMF(STATE+0xb08)=MEMF(STATE+0xb0c)=1;
    MEM32(weapon+0xec)=STATE;
    MEM16(weapon+0x38)=1000;
    MEM32(MODE+8)=pellets;
    MEM32(MODE+0x34)=0x123;MEM32(MODE+0x38)=0x124;MEMF(MODE+0x3c)=.1f;
    MEM32(MODE+0x24)=MEM32(MODE+0x28)=1;
    MEM32(MODE+0x68)=UINT32_MAX;
    memcpy((void *)((uintptr_t)MODE+0x7c+g_xbox_mem_offset),
           (void *)((uintptr_t)MODE+g_xbox_mem_offset),0x7c);
    if(selected_mode==2) MEM32(MODE+0x7c)=UINT32_MAX;
    projectiles=sounds=effects=random_calls=0;
}
typedef struct Snapshot {
    unsigned shots,audio,effect,rng,actor_ammo,weapon_ammo,events;
    float timer,sound_timer;
} Snapshot;
static Snapshot snapshot(void)
{
    Snapshot result={projectiles,sounds,effects,random_calls,
                     MEM16(STATE+0x61e),MEM16(weapon+0x38),MEM32(weapon+0x10c),
                     MEMF(weapon+0x104),MEMF(weapon+0xfc)};
    return result;
}
static void equal(Snapshot a,Snapshot b)
{
    assert(a.shots==b.shots && a.audio==b.audio && a.effect==b.effect && a.rng==b.rng);
    assert(a.actor_ammo==b.actor_ammo && a.weapon_ammo==b.weapon_ammo && a.events==b.events);
    assert(fabsf(a.timer-b.timer)<.00001f && fabsf(a.sound_timer-b.sound_timer)<.00001f);
}
static void check_scoped(void)
{
    const unsigned rates[]={60,120,144};
    unsigned scenarios=0;
    for(unsigned side=0;side<2;side++) for(unsigned selected_mode=0;selected_mode<3;selected_mode++)
    for(unsigned repeat=0;repeat<2;repeat++) for(unsigned pellets=1;pellets<=3;pellets+=2)
    for(unsigned slow=1;slow<=4;slow*=4) for(unsigned pool=0;pool<=2;pool+=2) {
        Snapshot reference={0};
        for(unsigned rate=0;rate<3;rate++) {
            prepare(side,selected_mode,repeat,pellets);
            MEM32(TABLE+0x18)=pool;
            scoped=rate!=0;g_tsfp_native_timing_active=rate!=0;
            unsigned fps=rates[rate];
            for(unsigned frame=0;frame<fps;frame++) {
                unsigned whole=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
                call(whole,rate?60.f/(fps*slow):(float)whole);
                Snapshot before=snapshot();call(0,0);equal(before,snapshot());
            }
            assert(projectiles==60/slow*pellets);
            assert(MEM32(weapon+0x10c)==60/slow);
            assert(MEM16(pool?weapon+0x38:STATE+0x61e)==1000-60/slow);
            if(!rate) reference=snapshot();else equal(reference,snapshot());
        }
        scenarios++;
        const unsigned whole[]={0,1,1,1,3,0,0,1};
        const float delta[]={.25f,.75f,1.5f,.5f,3,0,.25f,.75f};
        for(unsigned native=0;native<2;native++) {
            prepare(side,selected_mode,repeat,pellets);
            MEM32(TABLE+0x18)=pool;
            scoped=native;g_tsfp_native_timing_active=native;
            for(unsigned round=0;round<20;round++) for(unsigned step=0;step<8;step++)
                call(whole[step],native?delta[step]:(float)whole[step]);
            if(!native) reference=snapshot();else equal(reference,snapshot());
        }
    }
    /* Keep the original ineligible-actor exits; no timer/event changes. */
    for(unsigned rejection=0;rejection<3;rejection++) {
        prepare(0,0,0,1);scoped=1;g_tsfp_native_timing_active=1;
        if(rejection==0) MEM32(STATE+4)=0;
        if(rejection==1) MEM32(ACTOR+0x34)=0;
        if(rejection==2) MEM32(weapon+0x30)=UINT32_MAX;
        Snapshot before=snapshot();call(1,1);equal(before,snapshot());
    }
    prepare(0,0,0,1);scoped=1;g_tsfp_native_timing_active=1;
    Snapshot before=snapshot();call(1,0);equal(before,snapshot());
    printf("PASS: scoped scope, %u NPC weapon configurations, original ammo counters and sound positions, quarter-speed schedules, pause, variable/catch-up steps and ineligible actors\n",scenarios);
}
static void event_frame(unsigned whole,float delta,unsigned previous,unsigned current) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    event_previous=previous;event_current=current;
    expected_delta=g_tsfp_native_timing_active && delta>0 && !whole?1:(float)whole;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=STATE;g_edi=ACTOR;g_ebx=0xabcd;g_ebp=0xdead;
    if(event_caller) {
        g_ebp=STATE;g_esi=ACTOR;g_ebx=0xa200;
        sub_0027AE14();
        assert(g_ebp==STATE && g_esi==ACTOR && g_ebx==0xa200);
        g_ebp=0xdead;g_esi=STATE;g_ebx=0xabcd;g_edi=ACTOR;
    } else sub_002B9BCE();
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==STATE && g_edi==ACTOR && g_ebx==0xabcd && g_ebp==0xdead);
    assert(!(MEM32(STATE+0x2c)&0x400));
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void check_animation_event(void) {
    MEM16(EVENT_TABLE+6)=1;MEM16(EVENT_TABLE+8)=1;MEM16(EVENT_TABLE+10)=0x200;
    /* Pre-fix wrapper drops the crossed marker; the following whole-tick
     * frame cannot retry it because the animation bounds have advanced. */
    for(event_caller=0;event_caller<2;event_caller++) {
        prepare(1,0,0,1);animation_fixed=0;g_tsfp_native_timing_active=1;
        event_frame(0,5.f/12,0,1);event_frame(1,1,1,1);
        assert(!projectiles && !sounds);
    }
    animation_fixed=1;
    const float deltas[]={1,.5f,5.f/12,5.f/48};
    for(event_caller=0;event_caller<2;event_caller++) for(unsigned i=0;i<4;i++) {
        prepare(1,0,0,1);scoped=1;g_tsfp_native_timing_active=i!=0;
        event_frame(i?0:1,deltas[i],0,1);
        printf("npc_animation_event delta=%.6f shots=%u sounds=%u\n",deltas[i],projectiles,sounds);
        assert(projectiles==1 && sounds==1);
        assert(MEM16(STATE+0x61e)==999 && MEM32(weapon+0x10c)==1);
        assert(fabsf(MEMF(weapon+0x104)-1.f/60)<.00001f);
        Snapshot once=snapshot();event_frame(1,1,1,1);equal(once,snapshot());
    }
    event_caller=0;
    prepare(1,0,0,1);g_tsfp_native_timing_active=1;
    event_frame(0,0,0,1);assert(!projectiles && !sounds);
    event_frame(1,0,0,1);assert(!projectiles && !sounds);
    for(unsigned reject=0;reject<3;reject++) {
        prepare(1,0,0,1);
        if(reject==0)MEM32(STATE+4)=0;
        if(reject==1)MEM32(ACTOR+0x34)=0;
        if(reject==2)MEM32(weapon+0x30)=UINT32_MAX;
        Snapshot before=snapshot();event_frame(0,5.f/12,0,1);equal(before,snapshot());
    }
    puts("PASS: two original animation-event caller branches, dropped-event counterfactual, single shot/audio/ammo transaction, no retry duplicates, pause and rejected actors");
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4f9bac)=TABLE;MEM32(0x4e8cec)=60;
    for(scoped=0;scoped<2;scoped++) for(unsigned rate=0;rate<3;rate++) {
        unsigned fps=rates[rate];
        prepare(0,0,0,1);g_tsfp_native_timing_active=rate!=0;
        for(unsigned frame=0;frame<fps;frame++)
            call((frame+1)*60/fps-frame*60/fps,60.f/fps);
        printf("npc_fire_audit fps=%u scoped=%u shots=%u sounds=%u rng=%u timer=%.9f\n",
               fps,scoped,projectiles,sounds,random_calls,MEMF(weapon+0x104));
        assert(projectiles==60 && sounds==60);
        if(scoped || !rate) assert(fabsf(MEMF(weapon+0x104)-1.f)<.00001f);
        unsigned before=projectiles;
        call(0,0);assert(projectiles==before);
    }
    check_scoped();
    check_animation_event();
    free(memory);
    puts("PASS: original NPC firing execution and scoped checks; production parity evaluated separately");
    return 0;
}
