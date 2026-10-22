/* Original NPC aim branch plus its clamp/sign/blend helpers. Actor geometry
 * is controlled, RNG is deterministic, and post-aim firing is excluded.
 * Counterfactual opened=1 demonstrates why changing just the gate is unsafe. */
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

enum { ACTOR=0x1000, STATE=0x2000, TARGET=0x4000, TARGET_STATE=0x6000, STACK=0x10000 };
static unsigned random_calls,opened,rare,chances,events;
static unsigned event_ticks[8];
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected NPC aim tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected NPC aim callback")
#undef RECOMP_LOAD_GLOBAL_U32
static uint32_t audit_load(uint32_t site,uint32_t address,uint32_t value) {
    if(opened && site==0x2b93da && g_tsfp_native_timing_active)
        return MEMF(0x4e794c)>0;
    return pc_native_global_load(site,address,value);
}
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) audit_load(site,address,value)
#undef RECOMP_PUSH_IMM_U32_AT
#define RECOMP_PUSH_IMM_U32_AT(site,value) (opened?(uint32_t)(value):pc_native_push_imm(site,value))
#undef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site,function) ((!opened && (site)==0x2b9478 && pc_native_skip_aim_random())?pc_native_rng_interval_skip:(function))
void pc_native_rng_interval_skip(void) { g_eax=UINT32_MAX;g_esp+=4; }
void sub_00153B40(void) {
    random_calls++;g_eax=0x80000000u;
    if(MEM32(g_esp)==0x2b947d) {
        chances++;
        if(rare && chances%20==0) {
            assert(events<8);event_ticks[events++]=MEM32(0x7de318);g_eax=0;
        }
    }
    g_esp+=4;
}
void sub_003C848C(void) {
    g_eax=(uint32_t)(int32_t)g_fp_stack[g_fp_top];
    g_fp_top=(g_fp_top+1)&7;g_esp+=4;
}
void sub_000D6EC0(void) { g_eax=0;g_esp+=4; }
void sub_000D6D00(void) { g_eax=0;g_esp+=4; }
/* GENERATED_ROUTINES */
static void step(unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+0x60)=ACTOR;
    g_esi=STATE;g_ebp=0;g_edi=ACTOR;
    sub_002B93D5();
    assert(g_esp==STACK+4 && g_fp_top==0 && g_esi==STATE && g_ebp==0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void run(unsigned mode,unsigned fps,unsigned slow,unsigned force) {
    memset((void *)((uintptr_t)ACTOR+g_xbox_mem_offset),0,0x7000);
    MEM32(STATE+4)=TARGET;MEM32(TARGET+0x20)=mode?8:0;
    MEM32(TARGET+0x7c)=TARGET_STATE;
    MEM8(TARGET_STATE+0xc6c)=mode==2?0x10:0;
    MEMF(STATE+0xb90)=10;MEMF(STATE+0x9fc)=.75f;
    MEM32(0x4e8cec)=60;MEM8(0x7de455)=2;
    g_tsfp_native_timing_active=fps!=60;opened=force;random_calls=chances=events=0;
    unsigned frames=fps*slow,changed=0;
    for(unsigned f=0;f<frames;f++) {
        unsigned whole=(f+1)*60/(fps*slow)-f*60/(fps*slow);
        float before=MEMF(STATE+0x9fc);unsigned rng=random_calls;
        step(0,0);assert(MEMF(STATE+0x9fc)==before && random_calls==rng);
        if(g_tsfp_native_timing_active) {
            step(1,0);assert(MEMF(STATE+0x9fc)==before && random_calls==rng);
        }
        MEM32(0x7de318)=(f+1)*60/(fps*slow);
        step(whole,g_tsfp_native_timing_active?60.f/(fps*slow):(float)whole);
        changed+=MEMF(STATE+0x9fc)!=before;
        assert(isfinite(MEMF(STATE+0x9fc)));
    }
    printf("npc_aim rare=%u mode=%u fps=%u slow=%u opened=%u frames=%u changed=%u offset=%.9f rng=%u\n",
           rare,mode,fps,slow,opened,frames,changed,MEMF(STATE+0x9fc),random_calls);
    if(rare && mode==2 && !opened) {
        assert(chances==60 && random_calls==66 && events==3);
        assert(event_ticks[0]==20 && event_ticks[1]==40 && event_ticks[2]==60);
    }
}
static void check_edges_and_catchup(void) {
    const float steps[]={.25f,.75f,1.5f,.5f,3,0,.25f,.75f,2,6};
    for(unsigned mode=0;mode<3;mode++) {
        memset((void *)((uintptr_t)ACTOR+g_xbox_mem_offset),0,0x7000);
        MEM32(STATE+4)=TARGET;MEM32(TARGET+0x20)=mode?8:0;
        MEM32(TARGET+0x7c)=TARGET_STATE;MEM8(TARGET_STATE+0xc6c)=mode==2?0x10:0;
        MEMF(STATE+0xb90)=10;MEMF(STATE+0x9fc)=.75f;
        g_tsfp_native_timing_active=1;opened=rare=0;random_calls=chances=events=0;
        double elapsed=0;unsigned old_ticks=0;
        for(unsigned round=0;round<4;round++)for(unsigned i=0;i<10;i++) {
            elapsed+=steps[i];unsigned ticks=(unsigned)elapsed;
            MEM32(0x7de318)=ticks;
            step(ticks-old_ticks,steps[i]);old_ticks=ticks;
        }
        assert(elapsed==60);
        float expected=mode?-.01f+.76f*powf(.95f,60):.75f-1/MEMF(0x475d20);
        assert(fabsf(MEMF(STATE+0x9fc)-expected)<.00001f);
        assert(random_calls==(mode==2?28u:0u));
    }
    MEM32(TARGET+0x20)=0;
    for(int sign=-1;sign<=1;sign++) {
        MEMF(STATE+0x9fc)=sign*.1f;
        for(unsigned i=0;i<60;i++)step((i+1)*5/12-i*5/12,5.f/12);
        assert(MEMF(STATE+0x9fc)==0);
    }
    MEM32(STATE+4)=0;MEMF(STATE+0x9fc)=.5f;
    unsigned before=random_calls;step(1,1);
    assert(MEMF(STATE+0x9fc)==.5f && random_calls==before);
    puts("PASS: NPC aim uneven/zero/2-/6-tick intervals, decay sign/zero clamp and absent target");
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);
    const unsigned rates[]={60,120,144};
    for(unsigned mode=0;mode<3;mode++) {
        for(unsigned slow=1;slow<=4;slow*=4)
            for(unsigned i=0;i<3;i++)run(mode,rates[i],slow,0);
        run(mode,60,1,0);run(mode,120,1,1);run(mode,144,1,1);
    }
    rare=1;
    for(unsigned slow=1;slow<=4;slow*=4)
        for(unsigned i=0;i<3;i++)run(2,rates[i],slow,0);
    check_edges_and_catchup();
    puts("PASS: original NPC aim branch executed with bounded geometry, pause and timing-field checks");
    free(memory);return 0;
}
