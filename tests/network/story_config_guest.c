/* Execute the original supported-US Story stage builder on isolated RAM. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t address){(void)address;assert(!"unexpected dispatch");return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t address){(void)address;assert(!"unexpected dispatch");return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t address){(void)address;assert(!"unexpected dispatch");return NULL;}
void recomp_icall_fail_log(uint32_t address){(void)address;assert(!"unexpected dispatch");}
static unsigned prepared;
void sub_0035DE70(void){g_eax=0;g_esp+=4;}
void sub_00355EC0(void){g_eax=MEM32(0x76b140)==1;g_esp+=4;}
void sub_00064D10(void){g_eax=MEM8(0x7de455)==10 && MEM32(0x79094c)!=100 && MEM32(0x79094c)!=56;g_esp+=4;}
void sub_000389B0(void){assert(!"unexpected movie completion path");}
void sub_001CD590(void){assert(MEM32(g_esp+4)==MEM32(0x4d1ae0) || MEM32(g_esp+4)==MEM32(0x4d1ae4) || MEM32(g_esp+4)==MEM32(0x4d1ae0+0x44));++prepared;g_esp+=4;}
void sub_00064DD0(void){g_eax=12;g_esp+=4;}
static unsigned ready_packets;
static unsigned pending_loads;
static unsigned movie_closes;
void sub_00029400(void){++movie_closes;g_esp+=4;}
void sub_001CEB00(void){assert(!"unexpected movie classification");}
void sub_000623E0(void){assert(!"unexpected auxiliary movie close");}
static unsigned char pending_config[0xa0];
void sub_00065940(void);
void sub_0006F4B0(void){assert(!MEM32(g_esp+4) && !MEM32(g_esp+8));g_esp+=4;}
void sub_000459E0(void){g_esp+=4;}
void sub_00069E60(void){g_eax=0;g_esp+=4;}
void sub_0005A120(void){g_esp+=4;}
void sub_000658D0(void){memcpy(pending_config,(void *)((uintptr_t)MEM32(g_esp+4)+g_xbox_mem_offset),sizeof pending_config);++pending_loads;g_esp+=4;}
void sub_0022D780(void){assert(!"unexpected custom-map asset filter");}
void sub_00230CC0(void){assert(!"unexpected custom-map asset filter");}
void sub_0006C1E0(void){assert(!"cinematic must not allocate gameplay bodies");}
void sub_000D50E0(void){assert(!"cinematic must not spawn gameplay players");}
void sub_000D5480(void){assert(!"cinematic must not copy gameplay bodies");}
void sub_000B91F0(void){assert(!"cinematic must not build gameplay actor packet");}
void sub_00353A90(void){
    assert(MEM32(g_esp+4)==0x30000 && MEM32(g_esp+8)==10);
    assert(MEM32(g_esp+16)==5 && MEM32(g_esp+20)==1);
    assert(MEM8(MEM32(g_esp+12)+4)==3 && MEM32(0x790950)==0);
    ++ready_packets;g_esp+=4;
}
void sub_000641C0(void){g_eax=0;g_esp+=4;}
void sub_000D6C90(void){g_eax=0;g_esp+=4;}
void sub_00064180(void){assert(!"unexpected team score");}
void sub_000BD090(void){assert(!"movie packet must not touch bodies");}
void sub_00067450(void){assert(!"unexpected result transition");}
static int pc_network_enabled(void){return 1;}
void sub_000646B0(void);
void sub_00064810(void);
/* ORIGINAL_STAGE_BUILDER */
#define WORD_AT(a) MEM32(a)
static unsigned story_lobby_mission,story_lobby_difficulty,story_pending_stage;
static int story_context_start_pending;
static int story_mode=1;
static int story_network_session(void){return story_mode;}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
/* PRODUCTION_TRANSITION_ADAPTERS */
void sub_00065940(void){pc_story_network_stage_builder(sub_00065940_gen);}
static void stage(unsigned value){
    memset((void *)((uintptr_t)0x10000+g_xbox_mem_offset),0xa5,0xa0);
    MEM32(0x78ad50)=value;MEM32(0x78ad48)=0;MEM32(0x78ad54)=2;MEM32(0x5236c4)=2;
    g_esp=0x20000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x10000;
    g_ebx=0x111;g_esi=0x222;g_edi=0x333;g_ebp=0x444;
    sub_00065940();
    assert(g_esp==0x20004 && MEM32(0x20000)==0x12345678);
    assert(g_ebx==0x111 && g_esi==0x222 && g_edi==0x333 && g_ebp==0x444);
}
static unsigned asset(unsigned flags){
    g_esp=0x20000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=flags;
    sub_0006B100();assert(g_esp==0x20004);return g_eax;
}
int main(void){
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_MISSION_DATA */
    stage(0);
    assert(MEM8(0x10000)==1 && MEM8(0x10001)==1 && MEM8(0x10002)==0);
    assert(MEM32(0x10058)==0x20000000 && MEM8(0x1009b)==MEM32(0x4d1ae0));
    assert(MEM8(0x10054)==INTRO_SCENE && prepared==1);
    stage(1);
    assert(MEM8(0x10000)==2 && MEM8(0x10001)==2);
    assert(MEM8(0x10054)==2 && MEM8(0x10055)==10 && MEM8(0x10056)==2 && MEM8(0x10057)==2);
    assert(MEM32(0x10058)==0x80008 && MEM32(0x10060)==0 && MEM32(0x10064)==0 && MEM32(0x10068)==0);
    assert(MEM8(0x10014)==12 && MEM8(0x10015)==75 && prepared==1);
    MEM32(0x79094c)=2;MEM8(0x7de455)=10;MEM32(0x7de458)=0x80008;MEM32(0x7de45c)=0x8000;
    stage(2);assert(MEM8(0x10054)==106 && MEM8(0x10055)==9 && MEM8(0x10056)==3 && !MEM32(0x10058));
    MEM32(0x79094c)=106;stage(2);
    assert(MEM8(0x10054)==OUTRO_SCENE && MEM8(0x1009b)==MEM32(0x4d1ae4) && MEM32(0x10058)==0x20000000);
    /* The stock routine rejects cinematic eligibility before emitting ready.
       Its original transport tail remains usable without its player body loop. */
    MEM32(0x7de458)=0x20000000;MEM32(0x790950)=4;MEM32(0x79094c)=24;
    MEM32(0x76b108)=1;MEM32(0x76b1a4)=10;MEM32(0x774028)=0x30000;
    MEM8(0x30000)=3;g_esp=0x20000;MEM32(g_esp)=0x12345678;
    sub_00357540();assert(g_esp==0x20004 && !ready_packets && MEM32(0x76b1a4)==10);
    MEM32(0x7de458)=0;MEM32(0x790950)=0;g_esp=0x20000;
    sub_00357540();
    assert(g_esp==0x20004 && ready_packets==1 && MEM32(0x76b1a4)==15);
    assert((MEM8(0x30034)&4) && MEM32(0x77419c+3*0x30c)==5);
    MEM32(0x7de458)=0x80008;MEM32(0x76b1a4)=10;g_esp=0x20000;
    sub_00357540();
    assert(g_esp==0x20004 && ready_packets==2 && MEM32(0x76b1a4)==15);
    MEM8(0x7de455)=10;MEM8(0x7de457)=1;MEM8(0x7de49a)=0;MEM32(0x790950)=4;
    assert(!asset(0x400200)); /* Original co-op assets reject four players. */
    assert(asset(0x200)); /* Removing only count restriction retains Normal. */
    assert(!asset(0x100) && !asset(0x400)); /* Easy/Hard still excluded. */
    assert(!asset(0x200200)); /* Single-player-only stays excluded. */
    MEM32(0x790950)=2;
    assert(!asset(0x80400200) && asset(0x400200)); /* Network-only exclusion. */
    MEM32(0x790950)=4;MEM32(0x7356d8)=0x40000;
    const uint32_t actor=0x40000+4*0xea0;
    MEM32(actor)=0x70000;MEM32(actor+8)=4;MEM32(0x7007c)=actor;
    g_esp=0x20000;MEM32(g_esp+4)=4;sub_003568C0();
    assert(g_eax==0 && g_esp==0x20004); /* NPC lacks a controller record. */
    MEM32(0x76b140)=1;g_esp=0x20000;MEM32(g_esp+4)=0x70000;sub_00358560();
    assert(g_eax==1 && g_esp==0x20004); /* Host retains NPC script authority. */
    MEM32(0x76b140)=2;g_esp=0x20000;sub_00358560();
    assert(g_eax==0 && g_esp==0x20004);
    /* Captured rewards: both peers remain in phase10 with no player bodies.
       Stock readiness rejects scene106. The production scope emits its normal
       packet without spawning players, and restores the frontend scene. */
    MEM32(0x79094c)=106;MEM32(0x790950)=0;MEM32(0x7de458)=0;
    MEM32(0x76b140)=2;MEM32(0x76b1a4)=10;story_pending_stage=3;
    const unsigned prior_ready=ready_packets;
    g_esp=0x20000;sub_00357540();assert(ready_packets==prior_ready && MEM32(0x76b1a4)==10);
    g_esp=0x20000;pc_story_network_ready(sub_00357540);
    assert(ready_packets==prior_ready+1 && MEM32(0x76b1a4)==15 && g_esp==0x20004);
    assert(MEM32(0x79094c)==106 && MEM32(0x790950)==0 && !MEM32(0x7de458));
    story_mode=0;MEM32(0x76b1a4)=10;g_esp=0x20000;pc_story_network_ready(sub_00357540);
    assert(ready_packets==prior_ready+1 && MEM32(0x76b1a4)==10);story_mode=1;
    /* A reset frontend stage reproduces the captured wrong mission0 intro.
       At the real65CCF boundary the adapter gives original65940 stage3 of the
       completed mission; it must select mission1's authored intro exactly once. */
    MEM32(0x76b140)=1;stage(0);assert(MEM8(0x10054)==INTRO_SCENE);
    story_lobby_mission=0;story_lobby_difficulty=2;story_pending_stage=3;
    MEM32(0x79094c)=106;MEM32(0x78ad48)=0;MEM32(0x78ad50)=0;
    g_esp=0x20000;MEM32(g_esp)=0x65ccf;MEM32(g_esp+4)=0x10000;
    pc_story_network_stage_builder(sub_00065940_gen);
    assert(g_esp==0x20004 && MEM32(0x78ad48)==1 && MEM32(0x78ad50)==0);
    assert(MEM8(0x10054)==NEXT_INTRO_SCENE && MEM8(0x1009b)==MEM32(0x4d1ae0+0x44));
    assert(MEM32(0x79094c)==106); /* no persistent fake scene or asset guard */
    /* The final host reward action selects the original65C70 load scheduler,
       rather than generic65470's lobby exit. Execute its original local frame
       and builder call, keeping file/movie services controlled. */
    MEM32(0x7de30c)=7;MEM32(0x7de398)=0;MEM32(0x78ad48)=0;MEM32(0x78ad50)=2;
    g_esp=0x20000;sub_00065470();
    assert(g_esp==0x20004 && MEM32(0x7de30c)==9 && !pending_loads && MEM32(0x78ad48)==0);
    MEM32(0x7de30c)=7;
    g_esp=0x20000;sub_00065C70();
    assert(g_esp==0x20004 && pending_loads==1 && pending_config[0x54]==NEXT_INTRO_SCENE);
    assert(MEM32(0x78ad48)==1 && MEM32(0x7de30c)==12);
    /* Original99BB2 calls advance then60310 even when a guest's advance is
       blocked. Reproduce the exact cached-frame invalidation: the subsequent
       original606E0/60410 lookup falls back to frame zero after the stream
       base is cleared. The production release guard must keep the last frame
       until the actual host-driven load, without bypassing later cleanup. */
    story_pending_stage=2;MEM32(0x76b140)=2;MEM32(0x76b1a4)=15;
    MEM32(0x7de30c)=7;MEM32(0x7de458)=0x20000000;MEM32(0x79094c)=44;
    MEM32(0x7bb248)=0xffffffff;MEM32(0x4d19c8)=8;
    MEM32(0x7bb288)=1;MEM32(0x7bb260)=100;MEM32(0x7bb284)=1;
    MEM32(0x4d19cc)=0;MEM32(0x4d19d0)=93;MEM32(0x4d19d4)=94;
    MEM32(0x6d782c)=0x45000;MEM32(0x6d7830)=0x45040;MEM32(0x7de530)=0x44000;
    MEM32(0x6d7818)=0x30000;MEM32(0x30004)=0x40;MEM32(0x6d783c)=0x31000;
    MEM32(0x31000)=0xffffffff;
    MEM32(0x45000)=0x12345678;
    g_esp=0x20000;MEM32(g_esp)=0x99bbc;
    pc_story_network_movie_release(sub_00060310);
    assert(g_esp==0x20004 && !movie_closes && MEM32(0x7de530)==0x44000);
    for(unsigned phase=10;phase<=15;phase++) if(phase==10 || phase==14) {
        MEM32(0x76b1a4)=phase;g_esp=0x20000;MEM32(g_esp)=0x99bbc;
        pc_story_network_movie_release(sub_00060310);
        assert(g_esp==0x20004 && !movie_closes && MEM32(0x7de530)==0x44000);
    }
    MEM32(0x76b1a4)=15;
    g_esp=0x20000;MEM32(g_esp+4)=93;MEM32(g_esp+8)=0x46000;
    MEM32(g_esp+12)=0;MEM32(g_esp+16)=0;sub_00060740();
    assert(g_esp==0x20004 && MEM32(0x46000)==0x12345678);
    /* Stock close at the same callback invalidates the next cached lookup. */
    g_esp=0x20000;sub_00060310();assert(movie_closes==1 && !MEM32(0x7de530));
    g_esp=0x20000;MEM32(g_esp+4)=93;sub_000606E0();assert(g_eax==0x3003f);
    /* A real pending load must still close/reset, including at this caller. */
    MEM32(0x76b1a4)=10;MEM32(0x7de30c)=12;
    MEM32(0x4d19c8)=9;MEM32(0x7de530)=0x44000;
    g_esp=0x20000;MEM32(g_esp)=0x99bbc;pc_story_network_movie_release(sub_00060310);
    assert(movie_closes==2 && !MEM32(0x7de530) && g_esp==0x20004);
    /* Neither a host's completion nor a different teardown caller may leak. */
    MEM32(0x76b1a4)=15;MEM32(0x7de30c)=7;MEM32(0x76b140)=1;
    MEM32(0x4d19c8)=10;g_esp=0x20000;pc_story_network_movie_release(sub_00060310);
    assert(movie_closes==3 && g_esp==0x20004);
    MEM32(0x76b140)=2;MEM32(0x4d19c8)=11;g_esp=0x20000;MEM32(g_esp)=0x12345678;
    pc_story_network_movie_release(sub_00060310);assert(movie_closes==4 && g_esp==0x20004);
    /* Replay an empty, delayed gameplay score snapshot against the ending's
       30 authored NPC slots. Original64810 initializes rankings for all slots,
       overflowing its 16-entry table into7DE530 before any camera lookup.
       The exact stock packet decoder must be held before touching that scene. */
    MEM32(0x79094c)=44;MEM32(0x790950)=2;MEM32(0x7a2958)=30;
    MEM32(0x7356d8)=0x40000;MEM32(0x76b190)=1;MEM32(0x6f1188)=0;
    MEM32(0x7de30c)=7;MEM32(0x7de458)=0x20000000;
    MEM32(0x7de530)=0x44000;MEM32(0x7de318)=123;
    g_esp=0x20000;MEM32(g_esp+4)=0;MEM32(g_esp+8)=0x50000;
    sub_003578A0();assert(g_esp==0x20004 && MEM32(0x7de530)==0xffffffff);
    MEM32(0x7de530)=0x44000;MEM32(0x7de318)=123;
    g_esp=0x20000;pc_network_guest_scores(sub_003578A0);
    assert(g_esp==0x20004 && MEM32(0x7de530)==0x44000 && MEM32(0x7de318)==123);
    assert(MEM32(0x7a2958)==30); /* preserve authored NPC membership */
    puts("PASS: original Story stages, rewards ready packets, next-mission selection, movie stream lifetime and guest ABI");free(memory);return 0;
}
