/* Complete original HUD message update and queue eligibility routines.
 * Sound playback is observed, not performed. Test elapsed game time, delayed
 * messages, queue limits and pause across original/native update rates. */
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
RECOMP_TLS int g_fp_top,g_df;
static unsigned sounds;
static int playing;
void sub_001CDAE0(void) { g_eax=1;g_esp+=4; }
void sub_00045540(void) { g_eax=playing;g_esp+=4; }
void sub_00045390(void) { g_esp+=4; }
void sub_00045820(void) { sounds++;g_esp+=4; }
/* GENERATED_ROUTINES */
#define SLOT(n) (0x7a6198u+(n)*0x16cu)
static void message(unsigned slot,float delay,float duration)
{
    uint32_t address=SLOT(slot);
    MEM32(address)=2;
    MEMF(address+0x144)=-delay;
    MEMF(address+0x148)=duration;
    MEMF(address+0x150)=duration;
    MEMF(address+0x154)=duration;
    MEM32(address+0x158)=1;
    MEM32(address+0x15c)=123;
    MEM32(address+0x160)=0xffffffff;
}
static void update(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=0x7000;MEM32(g_esp)=0x12345678;
    sub_0014A5A0();assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    const unsigned rates[]={60,60,120,144,180,240};
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        if(mode==0 && slow!=1)continue;
        unsigned fps=rates[mode];g_tsfp_native_timing_active=mode!=0;
        memset(memory,0,0x800000);sounds=0;playing=0;
        /* ORIGINAL_CONSTANTS */
        MEM32(0x7b0c7c)=0x1000;MEM32(0x1004)=0;
        MEM32(0x4e8cec)=60;MEMF(0x475c78)=1;
        g_esp=0x7000;sub_0012D1E0();assert(g_esp==0x7004);
        message(0,0,1);message(1,.5f,1);
        unsigned ended[2]={0,0};
        for(unsigned frame=0;frame<fps*slow*3;frame++) {
            unsigned whole=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
            update(whole,60.f/(fps*slow));
            for(unsigned slot=0;slot<2;slot++)
                if(!MEM32(SLOT(slot)) && !ended[slot])ended[slot]=frame+1;
            if(frame==fps*slow/4) {
                float ages[2]={MEMF(SLOT(0)+0x144),MEMF(SLOT(1)+0x144)};
                unsigned before=sounds;
                for(unsigned pause=0;pause<fps;pause++)update(0,0);
                assert(MEMF(SLOT(0)+0x144)==ages[0] && MEMF(SLOT(1)+0x144)==ages[1]);
                assert(sounds==before);
            }
        }
        printf("hud native=%d fps=%u slow=%u first=%.6f delayed=%.6f sounds=%u\n",
               g_tsfp_native_timing_active,fps,slow,ended[0]/(double)fps/slow,
               ended[1]/(double)fps/slow,sounds);fflush(stdout);
        /* Type 2's original single-player limit allows both messages to advance.
         * Expiry may differ by one update due to float accumulation. */
        assert(ended[0] && fabs(ended[0]/(double)fps/slow-1)<.02);
        assert(ended[1] && fabs(ended[1]/(double)fps/slow-1.5)<.02);
        assert(sounds==2);
        /* With the original type-1 limit of one, the second message waits
         * at age zero and expires only after its own full display duration. */
        for(unsigned first_kind=1;first_kind<=6;first_kind+=5) {
            /* Type 6 conflicts with type 1 in the original conflict table too. */
            memset(memory+0x7a6160,0,0x177c);sounds=0;
            message(0,0,1);message(1,.5f,1);
            MEM32(SLOT(0))=first_kind;MEM32(SLOT(1))=1;
            unsigned done=0;
            for(unsigned frame=0;frame<fps*slow*3;frame++) {
                unsigned whole=(frame+1)*60/(fps*slow)-frame*60/(fps*slow);
                update(whole,60.f/(fps*slow));
                if(frame+1==fps*slow/2)assert(MEMF(SLOT(1)+0x144)<.02f);
                if(!MEM32(SLOT(1))) { done=frame+1;break; }
            }
            assert(done && fabs(done/(double)fps/slow-2)<.04 && sounds==2);
        }
    }
    /* Scotland uses type 6 with a five-second lifetime. Exercise that queue
     * policy with a representative twenty-second delay; live scripts choose
     * their delay independently. */
    for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        unsigned fps=rates[mode];g_tsfp_native_timing_active=mode!=0;
        memset(memory,0,0x800000);sounds=0;
        /* ORIGINAL_CONSTANTS */
        MEM32(0x7b0c7c)=0x1000;MEM32(0x4e8cec)=60;MEMF(0x475c78)=1;
        g_esp=0x7000;sub_0012D1E0();
        message(0,20,5);MEM32(SLOT(0))=6;
        MEMF(SLOT(0)+0x148)=3;MEMF(SLOT(0)+0x154)=3.5f;
        MEM32(SLOT(0)+0x15c)=0xffffffff;
        unsigned appeared=0,ended=0;
        for(unsigned frame=0;frame<fps*26;frame++) {
            update((frame+1)*60/fps-frame*60/fps,60.f/fps);
            if(!appeared && MEMF(SLOT(0)+0x144)>=0)appeared=frame+1;
            if(!MEM32(SLOT(0))) { ended=frame+1;break; }
        }
        printf("objective native=%d fps=%u appeared=%.6f expired=%.6f\n",
               g_tsfp_native_timing_active,fps,appeared/(double)fps,ended/(double)fps);
        assert(appeared && fabs(appeared/(double)fps-20)<.02);
        assert(ended && fabs(ended/(double)fps-25)<.02 && sounds==0);
    }
    /* Audio-bound messages hold at age zero while their sound is playing. */
    memset(memory,0,0x800000);g_tsfp_native_timing_active=1;
    MEM32(0x7b0c7c)=0x1000;MEM32(0x4e8cec)=60;MEMF(0x475c78)=1;
    MEM32(0x7490b4)=3;message(0,0,1);MEM32(SLOT(0)+0x160)=42;
    playing=1;sounds=0;
    for(unsigned frame=0;frame<144;frame++)update(frame%2,60.f/144);
    assert(MEMF(SLOT(0)+0x144)==0 && sounds==0);
    playing=0;update(0,60.f/144);
    assert(MEMF(SLOT(0)+0x144)>0 && sounds==1);
    /* A paused native interval must not age messages even with a stale whole tick. */
    float age=MEMF(SLOT(0)+0x144);update(1,0);
    assert(MEMF(SLOT(0)+0x144)==age);
    assert(pc_native_global_load(0x14a5ceu,0x4e7944,0)==0);
    g_tsfp_native_timing_active=0;
    assert(pc_native_global_load(0x14a5cdu,0x4e7944,0)==0);
    puts("PASS: original HUD message clocks, queueing, sound and pause at 60/120/144/180/240");
    free(memory);return 0;
}
