/* Execute the complete original weapon update and sine-table initializer.
 * Weapon state/render/input services are boundaries; two original model-store
 * blocks consume the motion argument. Observe the motion value
 * passed to all three weapon slots and preserve original RNG event cadence. */
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
/* VISUAL_TIME_POLICY */
enum { PLAYER=0x1000,ENTITY=0x3000,ACTOR=0x4000,STACK=0x10000 };
static unsigned updates,rng_calls;
static float output;
void sub_001C3659(void),sub_001C37CF(void),sub_000D613D(void);
static void consume(uint32_t slot,float value) {
    uint32_t saved_sp=g_esp,saved_bp=g_ebp,saved_seh=g_seh_ebp,saved_bx=g_ebx,
             saved_ax=g_eax,saved_cx=g_ecx,saved_dx=g_edx;
    RecompXmm x0=g_xmm0,x1=g_xmm1,x2=g_xmm2;
    uint32_t model=MEM32(slot+0x54);
    for(unsigned variant=0;variant<3;variant++) {
        g_esp=0x18000;MEM32(g_esp)=0x12345678;MEMF(g_esp+0x34)=value;
        g_ebp=g_seh_ebp=slot;g_ebx=0xb000;
        MEMF(0xb050)=.75f;MEM32(0xb00c)=variant==1?0x200000:0;
        g_xmm0.f[0]=0;g_xmm1.f[0]=.125f;
        /* First branch bypasses authored sway-disabled weapons. Second is
         * the other original model path. Both run their real C8 store. */
        if(variant==2)sub_001C37CF();else sub_001C3659();
        assert(g_esp==0x18004 && MEM32(0x18000)==0x12345678);
        assert(fabsf(MEMF(model+0xc8)-(.625f+(variant==1?0:value)))<.000001f);
    }
    g_esp=saved_sp;g_ebp=saved_bp;g_seh_ebp=saved_seh;g_ebx=saved_bx;
    g_eax=saved_ax;g_ecx=saved_cx;g_edx=saved_dx;
    g_xmm0=x0;g_xmm1=x1;g_xmm2=x2;
}
void sub_001C3100(void) {
    const unsigned slots[]={0x55c,0x184,0x934};
    assert(updates<3 && MEM32(g_esp+4)==PLAYER+slots[updates]);
    if(!updates)output=MEMF(g_esp+8);else assert(output==MEMF(g_esp+8));
    consume(MEM32(g_esp+4),MEMF(g_esp+8));
    updates++;g_esp+=4;
}
void sub_00153B40(void) { rng_calls++;g_eax=0x40000000;g_esp+=4; }
#define STUB(name) void name(void) { g_eax=0;g_esp+=4; }
#define UNUSED(name) void name(void) { assert(!"Unexpected " #name); }
STUB(sub_001B9AF0)
STUB(sub_001BF410)
UNUSED(sub_00056E90)
UNUSED(sub_001B8910)
UNUSED(sub_001B9E90)
UNUSED(sub_001B9EB0)
UNUSED(sub_001BDFD0)
UNUSED(sub_001BF140)
/* GENERATED_ROUTINES */
static void call(void (*function)(void)) {
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    function();assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
}
static float step(unsigned tick,unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    MEM32(0x7de318)=pc_native_visual_store(0x69265,tick);
    updates=0;call(sub_001C4400);assert(updates==3);
    assert(MEM32(0x7de318)==tick && MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    return output;
}
static void setup(void) {
    memset((void *)(g_xbox_mem_offset+PLAYER),0,0x2000);
    memset((void *)(g_xbox_mem_offset+ACTOR),0,0x2000);
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER+0x14)=ENTITY;MEM32(ENTITY+0x7c)=ACTOR;
    MEM32(PLAYER+4)=UINT32_MAX;MEM32(PLAYER+0x58c)=UINT32_MAX;
    MEMF(PLAYER+0xf64)=10;MEMF(PLAYER+0xfb4)=.25f;
    MEM32(PLAYER+0x55c+0x54)=0x8000;
    MEM32(PLAYER+0x184+0x54)=0x9000;
    MEM32(PLAYER+0x934+0x54)=0xa000;
    MEM32(0x4fa134)=UINT32_MAX;MEM32(0x4e8cec)=60;
    MEM32(0x7de318)=pc_native_visual_store(0x670e8,60);rng_calls=0;
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);call(sub_00153C20);
    float reference[721];unsigned reference_events[721];
    g_tsfp_native_timing_active=0;setup();reference[0]=step(60,0,0);reference_events[0]=0;
    for(unsigned t=1;t<=720;t++) {
        reference[t]=step(60+t,1,1);reference_events[t]=rng_calls;
    }
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) {
        unsigned fps=rates[r],previous=60,repeats=0;
        g_tsfp_native_timing_active=1;setup();float last=step(60,0,0);
        for(unsigned f=1;f<=fps*12;f++) {
            unsigned elapsed=f*60/(fps*slow),tick=60+elapsed;
            float current=step(tick,tick-previous,60.f/(fps*slow));
            if(current==last)repeats++;
            assert(rng_calls==reference_events[elapsed]);
            if(f*60%(fps*slow)==0)assert(fabsf(current-reference[elapsed])<.00001f);
            unsigned held_rng=rng_calls;
            assert(step(tick,0,0)==current && rng_calls==held_rng);
            float sample=RECOMP_F32_MUL_AT(0x1c4539,0,MEMF(PLAYER+0xfb4));
            for(unsigned view=0;view<4;view++)
                assert(RECOMP_F32_MUL_AT(0x1c4539,0,MEMF(PLAYER+0xfb4))==sample);
            assert(rng_calls==held_rng && MEM32(0x7de318)==tick);
            previous=tick;last=current;
        }
        printf("weapon_sway fps=%u slow=%u repeats=%u rng=%u final=%.9f\n",fps,slow,repeats,rng_calls,last);
        /* At normal speed the authored 2048-entry sine table has enough
         * distinct samples for every native update; quarter-speed may revisit
         * a table entry, but must match original-time values and events. */
        if(slow==1)assert(repeats==0);
    }
    /* Run the original displacement snapshot and duration stores. Position
     * advances after the snapshot, matching the real player's update order. */
    float walk_phase=0,walk_x=0,walk_y=0;
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) {
        unsigned fps=rates[r];float dt=60.f/(fps*slow);
        g_tsfp_native_timing_active=0;setup();pc_native_weapon_bob_clock();
        g_tsfp_native_timing_active=1;
        MEM32(PLAYER+0x148)=1;MEMF(PLAYER+0x174)=0;
        MEM32(0x4e7944)=1;MEMF(0x4e794c)=dt;
        g_eax=PLAYER;g_esp=STACK;MEM32(STACK)=0x12345678;sub_000D613D();
        MEMF(PLAYER+0xa8)+=.25f*dt;
        for(unsigned frame=0;frame<fps;frame++) {
            MEM32(0x4e7944)=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
            MEMF(0x4e794c)=dt;
            g_eax=PLAYER;g_esp=STACK;MEM32(STACK)=0x12345678;sub_000D613D();
            assert(g_esp==STACK+4 && !g_fp_top);
            step(60+(frame+1)*60/(fps*slow),MEM32(0x4e7944),dt);
            MEMF(PLAYER+0xa8)+=.25f*dt;
        }
        float phase=MEMF(PLAYER+0x174),x=MEMF(PLAYER+0x178),y=MEMF(PLAYER+0x17c);
        printf("walking_bob fps=%u slow=%u phase=%.9f position=%.9f,%.9f\n",fps,slow,phase,x,y);
        if(!r){walk_phase=phase;walk_x=x;walk_y=y;}
        else {assert(fabsf(phase-walk_phase)<.0001f);
              assert(fabsf(x-walk_x)<.001f && fabsf(y-walk_y)<.001f);}
    }
    /* Deliberately change frame duration: the distance belongs to the
     * preceding interval, rather than the current render frame. */
    const float intervals[]={1.f,.5f,.25f,1.f/3.f,60.f/144.f};
    for(unsigned player=0;player<4;player++) {
        MEM32(0x7b0c7c)=PLAYER+player*0x2000;
        MEM32(PLAYER+player*0x2000+0x14)=ENTITY;
        g_tsfp_native_timing_active=0;pc_native_weapon_bob_clock();
        g_tsfp_native_timing_active=1;MEMF(0x4e794c)=intervals[player];
        pc_native_weapon_bob_clock();
    }
    for(unsigned pass=0;pass<20;pass++)for(unsigned player=0;player<4;player++) {
        float previous=intervals[(player+pass)%5],next=intervals[(player+pass+1)%5];
        MEM32(0x7b0c7c)=PLAYER+player*0x2000;
        MEMF(0x4e794c)=next;pc_native_weapon_bob_clock();
        assert(fabsf(RECOMP_F32_DIV_AT(0x1c45bf,.25f*previous,previous)-.25f)<.000001f);
        assert(fabsf(RECOMP_F32_DIV_AT(0x1c4640,.25f*previous,previous)-.25f)<.000001f);
        assert(RECOMP_F32_DIV_AT(0x1c4641,2.f,4.f)==.5f);
    }
    MEM32(0x7b0c7c)=PLAYER;
    /* Disabled, paused/invalid samples and scene resets fall back safely. */
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_DIV_AT(0x1c45bf,2,4)==.5f);
    pc_native_weapon_bob_clock();g_tsfp_native_timing_active=1;
    MEMF(0x4e794c)=0;pc_native_weapon_bob_clock();
    assert(RECOMP_F32_DIV_AT(0x1c4640,2,4)==.5f);
    MEMF(0x4e794c)=NAN;pc_native_weapon_bob_clock();
    assert(RECOMP_F32_DIV_AT(0x1c4640,2,4)==.5f);
    MEMF(0x4e794c)=.25f;pc_native_weapon_bob_clock();pc_native_weapon_bob_clock();
    MEM32(0x7de30c)++;pc_native_weapon_bob_clock();
    assert(RECOMP_F32_DIV_AT(0x1c4640,2,4)==.5f);
    MEMF(0x4e794c)=2;pc_native_weapon_bob_clock();pc_native_weapon_bob_clock();
    assert(RECOMP_F32_DIV_AT(0x1c4640,2,4)==.5f);
    /* Only the audited final multiply changes. Native-disabled, rebased,
     * unobserved-clock and overflow paths keep the supplied original value. */
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_MUL_AT(0x1c4539,2,3)==6);
    g_tsfp_native_timing_active=1;
    assert(RECOMP_F32_MUL_AT(0x1c453a,2,3)==6);
    MEM32(0x7de318)=pc_native_visual_store(0x670e8,123);
    assert(RECOMP_F32_MUL_AT(0x1c4539,2,3)==6);
    MEM32(0x4e7944)=0;MEMF(0x4e794c)=.5f;
    MEM32(0x7de318)=pc_native_visual_store(0x69265,123);
    MEM32(0x7de318)=456;
    assert(RECOMP_F32_MUL_AT(0x1c4539,2,3)==6);
    MEM32(0x7de318)=pc_native_visual_store(0x670e8,0x7ffffffe);
    MEM32(0x7de318)=pc_native_visual_store(0x69265,0x7ffffffe);
    assert(RECOMP_F32_MUL_AT(0x1c4539,2,3)==6);
    free(memory);puts("PASS: original weapon update supplies fresh sway and preserves RNG cadence");return 0;
}
