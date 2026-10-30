/* Execute original blast construction, startup and later radius-damage gates. */
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
recomp_func_t recomp_lookup(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t a){(void)a;assert(0);return NULL;}
void recomp_icall_fail_log(uint32_t a){(void)a;assert(0);}
enum { EFFECT=0x60000,BLAST_POINT=0x20000,STACK=0x10000 };
static unsigned particles,sounds,damage,cells,story_events;
static unsigned story_effect_barrel_replay,story_effect_cosmetic_depth;
static uint32_t story_effect_host_blast_source;
static int session=1;
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static int story_effect_session(void){return session;}
void sub_000EDD40_gen(void);
void sub_000EDBE0(void);
void sub_00186480_gen(void){++cells;g_esp+=4;}
/* PRODUCTION_BLAST_GATES */
void sub_000EDD40(void){pc_story_network_blast_start(sub_000EDD40_gen);}
void sub_00186480(void){pc_story_network_blast_cells(sub_00186480_gen);}
void sub_000DC4A0(void){assert(MEM32(g_esp+4)==23);memset((void *)((uintptr_t)EFFECT+g_xbox_mem_offset),0,0x200);MEM32(EFFECT)=23;g_eax=EFFECT;g_esp+=4;}
void sub_000FF8A0(void){assert(MEM32(g_esp+8)==3 || MEM32(g_esp+8)==8);++particles;g_eax=0;g_esp+=4;}
void sub_00153B40(void){g_eax=0;g_esp+=4;}
void sub_00045390(void){memset((void *)((uintptr_t)MEM32(g_esp+4)+g_xbox_mem_offset),0,0x30);g_esp+=4;}
void sub_00045820(void){assert(MEM32(MEM32(g_esp+4))==0xc33);++sounds;g_esp+=4;}
void sub_000C3C50(void){g_eax=5;g_esp+=4;}
void sub_000B91F0(void){memset((void *)((uintptr_t)MEM32(g_esp+4)+g_xbox_mem_offset),0,0x58);g_esp+=4;}
void sub_000BB850(void){++damage;g_esp+=4;}
void sub_0015BFB0(void){++story_events;g_esp+=4;}
/* UNREACHED */
/* ORIGINAL_BLAST */
static void run(int replay)
{
    const int cosmetic=replay && session && MEM32(0x76b140)==2;
    particles=sounds=damage=cells=story_events=0;
    story_effect_barrel_replay=(unsigned)replay;g_esp=STACK;
    MEM32(STACK+4)=1;MEM32(STACK+8)=0xffffffff;MEM32(STACK+12)=BLAST_POINT;
    MEM32(STACK+16)=MEM32(STACK+20)=MEM32(STACK+24)=MEM32(STACK+28)=0;MEM32(STACK+32)=1;
    sub_000EFBD0();assert(g_esp==STACK+4 && g_eax==EFFECT);
    assert(particles==2 && sounds==1 && !story_events && !story_effect_cosmetic_depth);
    assert(MEMF(EFFECT+0x1c)==MEMF(BLAST_POINT) && MEMF(EFFECT+0x20)==MEMF(BLAST_POINT+4));
    assert((MEM32(EFFECT+0xc)&0x20001)==(cosmetic?0x20001u:0u));
    story_effect_barrel_replay=0;
    for(unsigned i=0;i<3;i++){g_esp=STACK;MEM32(STACK+4)=EFFECT;sub_000EFF90();assert(g_esp==STACK+4);}
    assert(cells==(cosmetic?0u:1u) && damage==(cosmetic?0u:3u));
}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x76b140)=2;MEM8(0x7de455)=10;MEM32(0x7de458)=0x80008;
    MEMF(BLAST_POINT)=-56.13f;MEMF(BLAST_POINT+4)=-11.21f;MEMF(BLAST_POINT+8)=-90.5f;
    MEM32(0x4e0fa0+3*28)=MEM32(0x4e0fa0+8*28)=1;
    run(0);run(1); /* Same original particles/audio, no second damage/world pass. */
    MEM32(0x76b140)=1;run(1); /* host retains all original behavior */
    MEM32(0x76b140)=2;session=0;run(1);session=1;
    run(1);
    g_esp=STACK;pc_story_network_blast_cells(sub_00186480_gen);assert(cells==1);
    free(ram);puts("PASS: original barrel particles/audio retained; initial Story event, navigation mutation and later radius damage excluded from replay");return 0;
}
