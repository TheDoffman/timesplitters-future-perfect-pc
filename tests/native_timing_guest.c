#include "platform/xbox_winnt.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t memory[0x800000];
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_esp;
#if defined(_MSC_VER)
#define RECOMP_MANUAL_TLS __declspec(thread)
#else
#define RECOMP_MANUAL_TLS _Thread_local
#endif
static int64_t counter, deadline;
static unsigned paused_calls, resume_calls, original_calls, frames;
static const char *setting;
void recomp_config_commit_native_rate(void) {}
const char *recomp_config_lookup(const char *env, const char *key)
{ assert(!strcmp(env,"TSFP_NATIVE_FPS") && !strcmp(key,"native_fps")); return setting; }
static BOOL test_qpf(LARGE_INTEGER *v) { v->QuadPart = 144000000; return TRUE; }
#ifndef TIMING_COUNTER_OBSERVER
#define TIMING_COUNTER_OBSERVER() ((void)0)
#endif
static BOOL test_qpc(LARGE_INTEGER *v)
{ TIMING_COUNTER_OBSERVER(); v->QuadPart = counter; return TRUE; }
static BOOL test_timer(HANDLE a, const LARGE_INTEGER *b, LONG c, void *d, void *e, BOOL f)
{ (void)a;(void)c;(void)d;(void)e;(void)f; deadline = counter + (-b->QuadPart)*144000000/10000000; return TRUE; }
static HANDLE test_create(void) { return (HANDLE)(uintptr_t)1; }
static DWORD test_wait(HANDLE a, DWORD ms)
{ (void)a;(void)ms; counter = deadline > counter ? deadline : counter+1; return WAIT_OBJECT_0; }
static void test_sleep(DWORD ms) { counter += (int64_t)ms*144000; }
#define QueryPerformanceFrequency test_qpf
#define QueryPerformanceCounter test_qpc
#define CreateWaitableTimerExW(a,b,c,d) test_create()
#define CreateWaitableTimerW(a,b,c) test_create()
#define SetWaitableTimer test_timer
#define WaitForSingleObject test_wait
#define Sleep test_sleep
#define WORD(a) (*(uint32_t *)(memory+(a)))
#define FLOAT(a) (*(float *)(memory+(a)))
uint64_t xbox_ReadTimeStampCounter(void) { return (uint64_t)counter*733333333/144000000; }
void sub_00153600(void) { assert(!"unexpected character scale in clock fixture"); }
void sub_00120AB0(void) { assert(!"unexpected character update in clock fixture"); }
void sub_000455B0(void) { paused_calls++; g_esp+=4; }
void sub_000455C0(void) { resume_calls++; g_esp+=4; }
void sub_00150190(void) { WORD(0x74914C)+=WORD(0x4E7940); g_esp+=4; }
static float original_game_delta=-1.f;
static void original(void) {
    original_calls++;g_esp+=4;
    if(original_game_delta>=0.f)FLOAT(0x4E794C)=original_game_delta;
}
#include "../src/pc_native_timing_guest.inc"
/* Match the production manual TU: the clock include precedes ESI's definition. */
RECOMP_MANUAL_TLS uint32_t g_esi;
#include "../src/pc_native_numeric.h"
static int test_button;
static void sample_pad(void)
{
    uint32_t record=WORD(g_esp+4),held=WORD(record+12),next=test_button?128:0;
    WORD(record+8)=held;WORD(record+12)=next;
    WORD(record+16)=next&~held;WORD(record+20)=held&~next;
    g_esp+=4;
}
static void input_edges(void)
{
    for(int phase=0;phase<12;phase++) {
        unsigned edges=0,releases=0;
        memset(memory+0x2000,0,64);
        for(int i=0;i<72;i++) {
            /* Every phase of the 144:60 cadence, including zero-tick press
             * frames. A normal held press must arrive once, never disappear. */
            test_button=i>=phase && i<phase+12;
            WORD(0x4E7940)=((i+1)*5/12)-(i*5/12);
            g_esp=0x1000;WORD(g_esp+4)=0x2000;WORD(g_esp+8)=0;
            pc_native_input_guest(sample_pad);
            assert(g_esp==0x1004);
            if(WORD(0x2010)) edges++;
            if(WORD(0x2014)) releases++;
        }
        assert(edges==1 && releases==1 && WORD(0x200C)==0);
    }
    g_tsfp_native_timing_active=0;WORD(0x4E7940)=0;test_button=1;
    g_esp=0x1000;WORD(g_esp+4)=0x2000;WORD(g_esp+8)=0;
    pc_native_input_guest(sample_pad);
    assert(g_esp==0x1004 && WORD(0x2010)==128); /* Original path is ungated. */
    g_tsfp_native_timing_active=1;
}
/* Exercise the real raw/game remainder clocks together with input delivery.
 * Pause durations deliberately leave their whole-tick phases misaligned.
 * The pad service is a boundary: held/pressed/released use the original
 * record layout, while physical polling and weapon state machines are not
 * simulated here. A held press must reach a game-tick consumer after resume;
 * edge-triggered actions must still be delivered exactly once. */
static void input_after_pause(void)
{
    const int rates[]={60,120,144};
    unsigned scenarios=0,misaligned=0;
    for(unsigned r=0;r<3;r++) for(int pause_frames=1;pause_frames<=24;pause_frames++)
    for(int phase=0;phase<12;phase++) {
        int rate=rates[r],held_frames=(rate+9)/10;
        int press_frame=7+pause_frames+phase;
        PcNativeTiming local={0};
        unsigned edges=0,releases=0,game_observations=0;
        int first_edge=-1,last_release=-1;
        memset(memory+0x2000,0,64);
        pc_native_timing_step(&local,0,720000,rate,0);
        for(int i=1;i<=press_frame+held_frames+12;i++) {
            int paused=i>=7 && i<7+pause_frames;
            PcNativeStep step=pc_native_timing_step(&local,
                (int64_t)i*720000/rate,720000,rate,paused);
            test_button=i>=press_frame && i<press_frame+held_frames;
            WORD(0x4E7940)=step.raw_ticks;WORD(0x4E7944)=step.game_ticks;
            FLOAT(0x4E7948)=step.raw_delta;FLOAT(0x4E794C)=step.game_delta;
            g_esp=0x1000;WORD(g_esp+4)=0x2000;
            pc_native_input_guest(sample_pad);
            assert(g_esp==0x1004);
            if(WORD(0x2010)) { edges++;first_edge=i; }
            if(WORD(0x2014)) { releases++;last_release=i; }
            if(!paused && step.raw_ticks!=step.game_ticks)misaligned++;
            if(step.game_ticks && WORD(0x200C)==128)game_observations++;
            if(paused)assert(step.game_ticks==0 && step.game_delta==0);
        }
        assert(edges==1 && releases==1 && WORD(0x200C)==0);
        assert(game_observations>0);
        /* Digital edges remain bounded by one original 60-Hz input interval. */
        assert(first_edge>=press_frame && first_edge-press_frame<(rate+59)/60);
        assert(last_release>=press_frame+held_frames &&
               last_release-press_frame-held_frames<(rate+59)/60);
        scenarios++;
    }
    assert(misaligned>0);
    printf("PASS: %u input schedules across 60/120/144 and fractional pause offsets; %u misaligned clock frames\n",
           scenarios,misaligned);
}
static void frame(void)
{
    g_esp=0x1000;
    WORD(g_esp)=0x12345678;
    pc_native_timing_guest(original);
    assert(g_esp==0x1004 && WORD(0x1000)==0x12345678);
    assert(WORD(0x563918)==123); /* ISR state was never used as a fake frame counter. */
    if(!g_tsfp_native_timing_active) {
        assert(!g_tsfp_native_step.units_per_tick && !g_tsfp_native_step.game_units &&
               !g_tsfp_native_step.game_phase && !g_tsfp_native_step.raw_units &&
               !g_tsfp_native_step.raw_phase);
    } else {
        assert(g_tsfp_native_step.game_ticks==WORD(0x4E7944));
        assert(g_tsfp_native_step.game_delta==FLOAT(0x4E794C));
        assert(g_tsfp_native_step.raw_ticks==WORD(0x4E7940));
        assert(g_tsfp_native_step.raw_delta==FLOAT(0x4E7948));
        assert(g_tsfp_native_step.raw_units>=0 && g_tsfp_native_step.raw_phase>=0);
        assert(g_tsfp_native_step.raw_ticks==(g_tsfp_native_step.raw_phase+
            g_tsfp_native_step.raw_units)/g_tsfp_native_step.units_per_tick);
    }
    frames++;
}
static void live_rates(void)
{
    /* Start at a nonzero tick phase. Native switches must retain it rather
     * than repeatedly losing fractions or running at the old schedule. */
    const char *rates[]={"120","144","120","144"};
    uint32_t raw_before=WORD(0x7497B0);
    int64_t started=counter;
    double ticks=0;
    for(unsigned r=0;r<4;r++) {
        setting=rates[r];int rate=atoi(setting);
        for(int i=0;i<rate;i++) {
            frame();ticks+=FLOAT(0x4E7948);
            assert(g_tsfp_native_timing_active && FLOAT(0x4E7948)<1.01f);
        }
    }
    double elapsed=(double)(counter-started)*60/144000000;
    assert(fabs(ticks-elapsed)<.001);
    assert(fabs((double)(WORD(0x7497B0)-raw_before)-elapsed)<1.001);
    assert(elapsed>239.99 && elapsed<240.01);
    WORD(0x7497B4)=1;
    setting="120";frame();assert(FLOAT(0x4E794C)==0);
    setting="144";frame();assert(FLOAT(0x4E794C)==0);
    unsigned old_calls=original_calls;
    setting="0";frame();
    assert(original_calls==old_calls+1 && !g_tsfp_native_timing_active);
    assert(WORD(0x7A58B0)==WORD(0x563918));
    assert(RECOMP_I32_TO_F32(0x4E7944,1)==1);
    counter+=144000000*10; /* Time spent in the original clock must not leak. */
    setting="120";frame();
    assert(g_tsfp_native_timing_active && FLOAT(0x4E7948)==0 && FLOAT(0x4E794C)==0);
    frame();assert(FLOAT(0x4E7948)>.499f && FLOAT(0x4E7948)<.501f);
    WORD(0x7497B4)=UINT32_MAX;frame();assert(FLOAT(0x4E794C)>.499f);
    setting="0";frame();assert(!g_tsfp_native_timing_active);
    setting="144";frame();assert(FLOAT(0x4E7948)==0);
    frame();assert(FLOAT(0x4E7948)>.416f && FLOAT(0x4E7948)<.417f);
    puts("PASS: live 60/120/144 transitions, pacing, remainder continuity, pause/resume and stale clock reset");
}
static void physics_history_transitions(void)
{
    original_game_delta=1.f;setting="0";frame();
    for(unsigned i=0;i<15;i++)assert(FLOAT(0x74c414+4*i)==1.f);
    for(unsigned i=0;i<15;i++)FLOAT(0x74c414+4*i)=3.f;
    frame(); /* Ordinary Original 60 updates retain their authored history. */
    for(unsigned i=0;i<15;i++)assert(FLOAT(0x74c414+4*i)==3.f);
    setting="120";frame();
    for(unsigned i=0;i<15;i++)FLOAT(0x74c414+4*i)=.5f;
    original_game_delta=0.f;setting="0";frame();
    for(unsigned i=0;i<15;i++)assert(FLOAT(0x74c414+4*i)==.5f);
    original_game_delta=.25f;frame();
    for(unsigned i=0;i<15;i++)assert(FLOAT(0x74c414+4*i)==.25f);
    setting="144";frame();original_game_delta=1.f;setting="0";frame();
    for(unsigned i=0;i<15;i++)assert(FLOAT(0x74c414+4*i)==1.f);
    original_game_delta=-1.f;
    puts("PASS: physics history rebased only after native-to-original handoff, including pause and slow motion");
}
/* Asset-free clock-to-resource contract; full US walking integration is
 * exercised by native_locomotion.c when the supported XBE is available. */
static void boost_resource_clock(void)
{
    const char *rates[]={"60","120","144"};
    const unsigned frames_per_second[]={60,120,144};
    const float scales[]={.25f,1.f,3.f};
    for(unsigned r=0;r<3;r++) for(unsigned speed=0;speed<3;speed++) {
        setting="0";frame();setting=rates[r];frame();
        float remaining=1.f,recovered=0.f;
        for(unsigned f=0;f<frames_per_second[r];f++) {
            frame();FLOAT(0x4E794C)=FLOAT(0x4E7948)*scales[speed];
            remaining=RECOMP_F32_SUB_AT(0x259060u,remaining,.006f);
            recovered=RECOMP_F32_ADD_AT(0x6552Du,recovered,.003f);
        }
        float expected=1.f-.36f*(speed==0?.25f:1.f);
        assert(fabsf(remaining-expected)<.00002f);
        assert(fabsf(recovered-.18f)<.00002f);
    }
    FLOAT(0x4E7948)=.5f;FLOAT(0x4E794C)=0;
    assert(RECOMP_F32_SUB_AT(0x259060u,1.f,.006f)==1.f);
    FLOAT(0x4E7948)=3.f;FLOAT(0x4E794C)=3.f;
    assert(RECOMP_F32_SUB_AT(0x259060u,1.f,.25f)==.75f);
    assert(RECOMP_F32_SUB_AT(0x259061u,1.f,.25f)==.75f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_SUB_AT(0x259060u,1.f,.25f)==.75f);
    g_tsfp_native_timing_active=1;FLOAT(0x4E7948)=0;
    assert(RECOMP_F32_ADD_AT(0x6552Du,4.f,.25f)==4.f);
    FLOAT(0x4E7948)=3;
    assert(RECOMP_F32_ADD_AT(0x6552Du,4.f,.25f)==4.25f);
    FLOAT(0x4E7948)=.5f;
    assert(RECOMP_F32_ADD_AT(0x6552Eu,4.f,.25f)==4.25f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_ADD_AT(0x6552Du,4.f,.25f)==4.25f);
    puts("PASS: boost drain and recovery retain original time budgets at 60/120/144");
}
/* Asset-free policy coverage on both CI hosts. The private-US fixture also
 * executes the actual callback, pose arithmetic and event crossings. */
static void tree_phase_clock(void)
{
    const unsigned rates[]={60,120,144,180,240};
    g_tsfp_native_timing_active=1;
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned count=rates[r]*slow,previous=0;
        double phase=0;
        for(unsigned f=1;f<=count;f++) {
            unsigned tick=f*60/count,whole=tick-previous;previous=tick;
            WORD(0x4e7944)=whole;FLOAT(0x4e794c)=60.f/count;
            if(pc_native_global_load(0x1d88b0,0x4e7944,whole))
                phase+=FLOAT(0x4e794c)/60.;
            assert(WORD(0x4e7944)==whole);
            assert(pc_native_global_load(0x1d88b1,0x4e7944,whole)==whole);
        }
        assert(phase>.99999 && phase<1.00001);
    }
    FLOAT(0x4e794c)=0;
    assert(!pc_native_global_load(0x1d88b0,0x4e7944,1));
    g_tsfp_native_timing_active=0;FLOAT(0x4e794c)=.25f;
    assert(!pc_native_global_load(0x1d88b0,0x4e7944,0));
    assert(pc_native_global_load(0x1d88b0,0x4e7944,3)==3);
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    WORD(0x563918)=123; WORD(0x4E8CEC)=60;
    setting=NULL; frame(); assert(original_calls==1);
    FLOAT(0x4E794C)=.25f;
    assert(RECOMP_I32_TO_F32(0x4E7944,1)==1);
    assert(RECOMP_I32_TO_F64(0x4E7944,1)==1);
    setting="144oops"; frame(); assert(original_calls==2);
    setting="300"; frame(); assert(original_calls==3);
    setting="144"; frame();
    double distance=0;
    int64_t exact_units=0;
    for(unsigned i=0;i<144;i++) {
        frame();distance+=FLOAT(0x4E794C);
        assert(g_tsfp_native_step.units_per_tick==144000000);
        assert(g_tsfp_native_step.game_phase==exact_units%144000000);
        assert(g_tsfp_native_step.raw_phase==g_tsfp_native_step.game_phase);
        assert(g_tsfp_native_step.raw_units==g_tsfp_native_step.game_units);
        exact_units+=g_tsfp_native_step.game_units;
    }
    assert(exact_units==60LL*144000000);
    assert(WORD(0x7497B0)==60 && WORD(0x74914C)==60);
    assert(distance>59.999 && distance<60.001);
    assert(WORD(0x7497B8)==145);
    assert(RECOMP_I32_TO_F32(0x4E7944,WORD(0x4E7944))==FLOAT(0x4E794C));
    assert(RECOMP_I32_TO_F64(0x4E7940,WORD(0x4E7940))==FLOAT(0x4E7948));
    assert(RECOMP_I32_TO_F64(0x400000,INT32_MAX)==2147483647.0);
    /* The integer field is still 0/1 for counters, independently from the
     * float conversion used for motion. Guest-local pause scopes win. */
    FLOAT(0x4E794C)=0;
    assert(RECOMP_I32_TO_F32(0x4E7944,1)==0);
    FLOAT(0x4E794C)=.4166667f;
    float saved_delta=pc_native_save_animation_delta();
    FLOAT(0x4E794C)=1; /* Original frozen-pose evaluation restores integer time. */
    pc_native_restore_animation_delta(saved_delta);
    assert(FLOAT(0x4E794C)==.4166667f);
    WORD(0x7497B4)=1;
    for(unsigned i=0;i<144;i++) {
        frame();assert(FLOAT(0x4E794C)==0 && WORD(0x4E7944)==0);
        assert(!g_tsfp_native_step.game_units && !g_tsfp_native_step.game_ticks);
        assert(g_tsfp_native_step.raw_phase==exact_units%144000000);
        assert(g_tsfp_native_step.raw_units>0);
        exact_units+=g_tsfp_native_step.raw_units;
    }
    assert(paused_calls==1 && resume_calls==0);
    assert(WORD(0x7497B0)==120);
    assert(exact_units==120LL*144000000);
    WORD(0x7497B4)=UINT32_MAX; frame();
    assert(resume_calls==1 && WORD(0x7497B4)==0 && FLOAT(0x4E794C)>0);
    assert(original_calls==3 && frames==293);
    live_rates();
    input_edges();
    input_after_pause();
    physics_history_transitions();
    boost_resource_clock();
    tree_phase_clock();
    puts("Native guest timing: original fallback, 144 distinct steps, ABI, pause and console clock passed");
    return 0;
}
