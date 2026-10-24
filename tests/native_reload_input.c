/* Original US reload consumer and held/pressed getters. Boundary services
 * stand in for animation/ammunition operations; the actual reload decision,
 * binding lookup, guards, call ABI and elapsed-time update execute unchanged. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
/* TIMING_HEADER */
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp;
enum { PLAYER=0x1000, LINK=0x2000, ACTOR=0x3000, WEAPON=0x4000,
       STATE=0x5000, TABLE=0x6000, STACK=0x9000 };
static unsigned reloads,mode_changes,blocked,busy;
static uint16_t buttons;
void sub_0046E36D(void)
{
    uint32_t out=MEM32(g_esp+8);MEM32(out)=1;MEM16(out+4)=0;
    for(unsigned i=0;i<8;i++)MEM8(out+6+i)=0;
    MEM8(out+8)=buttons ? 255 : 0; /* X, produced by the default R binding. */
    for(unsigned i=0;i<4;i++)MEM16(out+14+i*2)=0;
    g_eax=0;g_esp+=12;
}
void sub_0046E3E0(void) { g_eax=0;g_esp+=12; }
void sub_00179E70(void) { assert(MEM32(g_esp+4)==PLAYER);g_eax=blocked;g_esp+=4; }
void sub_001B8160(void) { assert(g_ecx==STATE);g_eax=busy;g_esp+=4; }
void sub_001C0FA0(void) { mode_changes++;g_esp+=4; }
void sub_001C1E10(void)
{
    assert(MEM32(g_esp+4)==ACTOR && MEM32(g_esp+8)==WEAPON);
    assert(MEM32(g_esp+12)==STATE && MEM32(g_esp+16)==1);
    assert(MEM32(g_esp+20)==0xabcdef);
    reloads++; MEM32(STATE+0x44)=7; g_esp+=4;
}
void sub_001BC990(void) { assert(g_eax==WEAPON && g_edi==STATE);g_esp+=4; }
void sub_001B7CC0(void) { g_eax=0;g_esp+=4; }
void sub_001C2C00(void) { g_esp+=4; }
void sub_001C1C00(void) { g_esp+=4; }
void sub_001B6C90(void) { assert(!"unexpected automatic reload"); }
/* GENERATED_ROUTINES */
#define NT_WORD(address) MEM32(address)
/* INPUT_WRAPPER */
static void call(unsigned whole,float delta,unsigned pressed)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    MEM32(0x6b93e0)=pressed;
    g_eax=STATE;g_ebx=WEAPON;g_esi=0x1234;g_edi=0x5678;
    g_ebp=g_seh_ebp=0x8765;g_esp=STACK;
    MEM32(STACK)=0x12345678;MEM32(STACK+4)=0xabcdef;
    sub_001C2E80();
    assert(g_esp==STACK+4 && g_esi==0x1234 && g_edi==0x5678 && g_ebp==0x8765);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
/* PHASE_SWEEP */
static void reset_poll(void)
{
    memset((void *)(g_xbox_mem_offset+0x6b93d0),0,0x3c);
    buttons=0;
}
static unsigned poll_edge(PcNativeStep step,unsigned down)
{
    buttons=(uint16_t)down;
    MEM32(0x4e7940)=step.raw_ticks; g_esp=STACK;
    MEM32(STACK+4)=0x6b93d0; MEM32(STACK+8)=0;
    pc_native_input_guest(sub_0018FF70);
    assert(g_esp==STACK+4);
    return MEM32(0x6b93e0);
}
static unsigned consume_edge(PcNativeStep step,unsigned edge)
{
    unsigned before=reloads;
    MEM32(STATE+0x44)=1;
    call(step.game_ticks,step.game_delta,edge);
    return reloads-before;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER+0x14)=LINK;MEM32(LINK+0x7c)=ACTOR;
    MEM32(0x4f9bac)=TABLE;MEM32(0x4e8cec)=60;MEMF(0x475c78)=1;
    MEM32(PLAYER+0x24)=16;MEM32(PLAYER+0x4c)=32;
    /* ORIGINAL_CONSTANTS */
    const unsigned rates[]={60,120,144,180,240};
    /* SWEEP_ONLY */
    for(int native=0;native<=1;native++) for(unsigned r=0;r<5;r++) {
        unsigned fps=rates[r];g_tsfp_native_timing_active=native;
        for(unsigned frame=0;frame<fps;frame++) {
            unsigned whole=(frame+1)*60/fps-frame*60/fps,before=reloads;
            MEM32(STATE+0x44)=1;MEMF(STATE+0x48)=0;
            call(whole,60.f/fps,16);
            assert(reloads==before+(native || whole>0));
            assert(fabsf(MEMF(STATE+0x48)-1.f/fps)<1e-6f);
            /* Holding a key does not repeat the reload event. */
            MEM32(0x6b93dc)=16;call(1,1,0);assert(reloads==before+(native || whole>0));
            MEM32(0x6b93dc)=0;
        }
    }
    g_tsfp_native_timing_active=1;
    unsigned before=reloads;
    MEM32(STATE+0x44)=1;
    call(0,0,16);call(1,0,16); /* Pause, including stale carried ticks. */
    assert(reloads==before);
    blocked=1;call(0,.5f,16);blocked=0;assert(reloads==before);
    MEM32(PLAYER+0x114)=16;call(0,.5f,16);MEM32(PLAYER+0x114)=0;assert(reloads==before);
    MEM32(0x6b9888)=1;call(0,.5f,16);MEM32(0x6b9888)=0;assert(reloads==before);
    busy=1;call(0,.5f,16);busy=0;assert(reloads==before);
    MEM32(STATE+0x44)=7;call(0,.5f,16);assert(reloads==before);
    MEM32(STATE+0x44)=1;
    MEM32(PLAYER+0x24)=128;call(0,.25f,16);assert(reloads==before);
    call(0,.25f,128);assert(reloads==before+1); /* Remapped reload. */
    MEM32(STATE+0x44)=1;call(3,3,128);assert(reloads==before+2);
    /* The same input gate also owns weapon-mode changes. */
    call(0,.5f,32);assert(mode_changes==1);
    call(0,.5f,0);assert(mode_changes==1);
    /* The host decoder publishes button edges on RAW whole ticks. Pausing
     * advances raw time but freezes game time, so their phases can differ.
     * Execute the real decoder/wrapper, timing model and reload consumer. */
temporal:
    MEM32(0x74d0fc)=1;MEM32(0x74d100)=0x100;
    for(unsigned axis=0;axis<4;axis++)MEM16(0x74d098+0xd+axis*2)=65535;
    MEM8(0x74d098+0xb)=MEM8(0x74d098+0xc)=255;
    MEM16(0x74d098+0x15)=65535;MEM16(0x74d098+0x17)=32767;
    MEM32(PLAYER+0x24)=16; /* Original X/reload binding. */
    reload_phase_sweep(reset_poll,poll_edge,consume_edge);
    g_tsfp_native_timing_active=1;
    for(unsigned r=1;r<5;r++) {
        unsigned fps=rates[r],seen=0;
        PcNativeTiming clock={0};
        pc_native_timing_step(&clock,0,fps*1000,fps,0);
        memset(memory+0x6b93d0,0,0x3c);
        for(unsigned frame=1;frame<fps*2;frame++) {
            PcNativeStep step=pc_native_timing_step(&clock,frame*1000,fps*1000,fps,frame==1);
            /* Press on a mismatched frame, release through the real decoder,
             * then press again. Retain its held/previous state throughout. */
            buttons=step.raw_ticks && !step.game_ticks && step.game_delta>0 && !MEM32(0x6b93dc)
                ? 1 : 0;
            MEM32(0x4e7940)=step.raw_ticks;g_esp=STACK;
            MEM32(STACK+4)=0x6b93d0;MEM32(STACK+8)=0;
            pc_native_input_guest(sub_0018FF70);
            assert(g_esp==STACK+4);
            unsigned edges=MEM32(0x6b93e0),count=reloads;
            MEM32(STATE+0x44)=1;
            call(step.game_ticks,step.game_delta,edges);
            if(buttons) { assert(edges==MEM32(PLAYER+0x24)); assert(reloads==count+1);seen++; }
            else assert(reloads==count);
        }
        assert(seen>0);
    }
    free(memory);puts("PASS: original reload edges, remapping, native rates, pause and eligibility");
    return 0;
}
