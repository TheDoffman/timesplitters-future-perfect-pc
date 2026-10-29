/* Original US interaction selection and input getters. Geometry/audio services
 * are controlled boundaries; input edges, guards and activation branches run. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
/* TIMING_HEADER */
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { PLAYER=0x1000, ACTOR=0x3000, DATA=0x5000,
       TARGET=0x6000, LEVER=0x7000, STACK=0x10000 };
static unsigned activations,blocked;
static uint16_t buttons;
void sub_0046E36D(void)
{
    uint32_t out=MEM32(g_esp+8);MEM32(out)=1;MEM16(out+4)=0;
    for(unsigned i=0;i<8;i++)MEM8(out+6+i)=0;
    MEM8(out+6)=buttons?255:0; /* A: keyboard Interact and controller confirm. */
    for(unsigned i=0;i<4;i++)MEM16(out+14+i*2)=0;
    g_eax=0;g_esp+=12;
}
void sub_0046E3E0(void) { g_eax=0;g_esp+=12; }
void sub_00153440(void)
{
    uint32_t out=MEM32(g_esp+4);
    for(unsigned i=0;i<16;i++)MEMF(out+4*i)=1.f;
    g_esp+=4;
}
void sub_00088C60(void) { g_eax=blocked;g_esp+=4; }
void sub_000B8C60(void)
{
    assert(MEM32(g_esp+4)==TARGET && MEM32(g_esp+8)==ACTOR);
    activations++;g_esp+=4;
}
void sub_000A0D30(void) { assert(!"unexpected alternate target"); }
void sub_00012E90(void) { assert(!"unexpected model attachment"); }
void sub_00152090(void) { assert(!"unexpected model attachment transform"); }
void sub_002321C0(void) { assert(!"unexpected special mode"); }
void sub_0003D7B0(void) { assert(!"unexpected occupied target"); }
void sub_0012D350(void) { assert(!"unexpected occupied target message"); }
/* GENERATED_ROUTINES */
#define NT_WORD(address) MEM32(address)
/* INPUT_WRAPPER */
static void reset(void)
{
    memset((void *)(g_xbox_mem_offset+0x6b93d0),0,0x3c);
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER)=0;MEM32(PLAYER+4)=0;
    MEM32(PLAYER+0x14)=ACTOR;MEM32(PLAYER+0x20)=0x80;
    MEM32(ACTOR+0x7c)=DATA;MEMF(ACTOR+0x1cc)=1.f;
    MEM32(0x7b6258)=1;MEM32(0x7b4fc0)=TARGET;
    MEM32(TARGET+0x20)=0x20000;MEM32(TARGET+0x28)=4;
    MEM32(TARGET+0x154)=1;MEM32(TARGET+0x7c)=LEVER;
    MEM32(LEVER+0xc)=4;MEM32(LEVER+0x10)=0;
    MEM32(LEVER+0x18)=0;MEM32(LEVER+0x50)=UINT32_MAX;
    MEMF(TARGET+0x64)=1.f;MEMF(TARGET+0x25c)=100.f;
    MEMF(TARGET+0x260)=0.5f;blocked=0;
    MEM32(DATA+0x9dc)=0x10000; /* Held-repeat cooldown stays ineligible. */
}
static unsigned update(PcNativeStep step,unsigned down)
{
    buttons=(uint16_t)down;
    MEM32(0x4e7940)=step.raw_ticks;MEM32(0x4e7944)=step.game_ticks;
    MEMF(0x4e794c)=step.game_delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=0x6b93d0;MEM32(STACK+8)=0;
    pc_native_input_guest(sub_0018FF70);
    assert(g_esp==STACK+4);
    unsigned edge=MEM32(0x6b93e0),before=activations;
    g_esp=STACK;g_ebx=0x1234;g_esi=0x5678;g_edi=0xabcd;g_ebp=0x9876;
    sub_000A0D90();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(g_ebx==0x1234 && g_esi==0x5678 && g_edi==0xabcd && g_ebp==0x9876);
    assert(g_fp_top==0 && MEM32(0x4e7944)==step.game_ticks && MEMF(0x4e794c)==step.game_delta);
    if(edge && step.game_delta>0.f && !blocked && activations!=before+1)fprintf(stderr,"lost native=%d raw=%u game=%u dt=%g held=%x edge=%x mask=%x result=%u\n",g_tsfp_native_timing_active,step.raw_ticks,step.game_ticks,step.game_delta,MEM32(0x6b93dc),edge,MEM32(PLAYER+0x20),g_eax);
    if(edge && step.game_delta>0.f && !blocked && MEMF(ACTOR+0x1cc)>0.f)assert(activations==before+1 && "interaction edge lost");
    else assert(activations==before);
    return edge!=0;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x74d0fc)=1;MEM32(0x74d100)=0x100;
    MEM8(0x74d098+0xb)=MEM8(0x74d098+0xc)=255;
    const unsigned rates[]={60,120,144,180,240};unsigned trials=0,presses=0;
    for(unsigned mode=0;mode<2;mode++)for(unsigned r=0;r<5;r++)
    for(unsigned phase=0;phase<12;phase++) {
        if(!mode && r)continue;
        PcNativeTiming clock={0};reset();
        g_tsfp_native_timing_active=mode;
        unsigned fps=g_tsfp_native_timing_active?rates[r]:60,edges=0,base=activations;
        pc_native_timing_step(&clock,0,1000000,fps,0);
        for(unsigned f=1;f<=fps;f++) {
            /* Pause offsets raw input ticks from game ticks, as in real menus. */
            unsigned paused=f<phase+1;
            PcNativeStep step=pc_native_timing_step(&clock,(int64_t)f*1000000/fps,1000000,fps,paused);
            if(!mode) { step.raw_ticks=1;step.game_ticks=paused?0:1;step.game_delta=paused?0.f:1.f; }
            unsigned down=!paused && ((f-phase)%20>=2 && (f-phase)%20<10);
            edges+=update(step,down);
        }
        if(!edges || activations-base!=edges)fprintf(stderr,"trial mode=%u fps=%u phase=%u edges=%u acts=%u\n",mode,fps,phase,edges,activations-base);
        assert(edges && activations-base==edges);presses+=edges;trials++;
    }
    /* A paused frame must not use a stale carried whole count. */
    reset();g_tsfp_native_timing_active=1;
    update((PcNativeStep){.raw_ticks=1,.game_ticks=1,.game_delta=0.f},1);
    assert(activations==presses);
    /* Preserve the original obstruction and player-alive guards. */
    reset();g_tsfp_native_timing_active=1;blocked=1;
    update((PcNativeStep){.raw_ticks=1,.game_ticks=0,.game_delta=.25f},1);
    assert(activations==presses);
    reset();MEMF(ACTOR+0x1cc)=0;
    update((PcNativeStep){.raw_ticks=1,.game_ticks=0,.game_delta=.25f},1);
    assert(activations==presses);
    free(memory);printf("PASS: interaction trials=%u activations=%u at 60/120/144/180/240 with pause phases\n",trials,presses);
    return 0;
}
