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

uint32_t g_xbox_code_lo,g_xbox_code_hi;
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_trace_idx;
volatile uint64_t g_icall_count;
void recomp_icall_not_code_log(uint32_t a,uint32_t sp){(void)a;(void)sp;assert(0);}
enum { BARREL=0x60000, HUMAN=0x61000, CONTEXT=0x62000, EFFECT=0x63000, ENEMY=0x64000, STACK=0x20000 };
#define WORD_AT(a) MEM32(a)
static uint32_t story_effect_host_blast_source;
static int session=1, explosions, broadcasts, removals;
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static int story_effect_session(void){return session;}
#ifndef _WIN32
#define _WIN32 1
#endif
void sub_0035AF40_gen(void),sub_0035C870(void),sub_00355EC0(void),sub_000BB490(void),sub_000BA230(void),sub_0035BE90(void);
/* PRODUCTION_BLAST_GATES */
void sub_0035AF40(void){pc_story_network_blast_source(sub_0035AF40_gen);}
void sub_003533F0(void){g_eax=1;g_esp+=4;}
void sub_003593D0(void){g_eax=1;g_esp+=4;}
void sub_00359400(void){MEM32(g_eax)+=5;g_esp+=4;}
void sub_00353A90(void){assert(MEM32(g_esp+8)==0x10);++broadcasts;g_eax=1;g_esp+=4;}
void sub_000D6A80(void){g_eax=0;g_esp+=4;}
void sub_00238510(void){g_esp+=4;}
void sub_000A9070(void){assert(MEM32(g_esp+4)==BARREL);++removals;g_esp+=4;}
void sub_000EFBD0(void){
    assert(MEM32(g_esp)==0x218702 && MEM32(g_esp+4)==47);
    ++explosions;MEM32(EFFECT)=23;MEM32(EFFECT+4)=47;MEM32(EFFECT+0x10c)=MEM32(g_esp+20);g_eax=EFFECT;g_esp+=4;
}
void sub_000C6140(void){g_eax=0;g_esp+=4;}
void sub_000B9340(void){g_esp+=4;}
void sub_000B9430(void){g_eax=0;g_esp+=4;}
void sub_00124C70(void){g_eax=0;g_esp+=4;}
void sub_00160020(void){g_esp+=4;}
void sub_000BA8A0(void){g_esp+=4;}
/* UNREACHED */
/* ORIGINAL_BLAST */
static void radius_pass(void){
    /* The spatial query selects a nearby enemy; execute its real damage and
       reliable serializer with the explosion's original remote credit. */
    const uint32_t frame=g_esp;
    MEM32(CONTEXT)=ENEMY;MEM32(CONTEXT+4)=MEM32(CONTEXT+8)=HUMAN;
    MEM32(CONTEXT+0x18)=23;MEMF(CONTEXT+0x20)=10;MEM32(CONTEXT+0x24)=0xffffffff;
    g_esp=frame-1024;MEM32(g_esp+4)=CONTEXT;sub_000BB6C0();g_esp=frame+4;
}
int main(void){
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x76b140)=1;MEM8(0x7de455)=10;MEM32(0x4e7944)=1;
    MEM32(HUMAN+0x20)=8;MEM32(HUMAN+0x150)=0x65000;MEM32(0x65004)=0xffffffff;
    MEM32(HUMAN+0x94)=100;MEM32(ENEMY+0x94)=101;
    MEM32(ENEMY+0x7c)=0x68000;MEM32(0x68014)=1;MEM32(0x68008)=2;MEM32(ENEMY+0x20)=8;MEM32(ENEMY+0x28)=0x11;
    MEM32(BARREL+0x2d8)=0x66000;MEM32(0x66000)=0x50435833;
    MEMF(BARREL+0x1cc)=.5f;MEM32(CONTEXT)=BARREL;MEM32(CONTEXT+4)=HUMAN;
    MEM32(CONTEXT+0x18)=35;MEMF(CONTEXT+0x20)=1.5f;
    g_esp=STACK;MEM32(STACK+4)=CONTEXT;sub_00218650();
    assert(g_esp==STACK+4 && removals==1 && explosions==1 && MEMF(BARREL+0x1cc)==-1);
    MEMF(ENEMY+0x1cc)=30;g_esp=STACK;MEM32(STACK+4)=EFFECT;
    radius_pass();assert(MEMF(ENEMY+0x1cc)==30 && !broadcasts); /* stock rejects remote source */
    g_esp=STACK;pc_story_network_blast_update(radius_pass);
    assert(MEMF(ENEMY+0x1cc)==20 && broadcasts==1 && !story_effect_host_blast_source);
    g_esp=STACK;radius_pass();assert(MEMF(ENEMY+0x1cc)==20 && broadcasts==1); /* scope restored */
    MEM32(0x76b140)=2;g_esp=STACK;pc_story_network_blast_update(radius_pass);
    assert(MEMF(ENEMY+0x1cc)==20 && broadcasts==1);
    MEM32(0x76b140)=1;session=0;g_esp=STACK;pc_story_network_blast_update(radius_pass);
    assert(MEMF(ENEMY+0x1cc)==20 && broadcasts==1);
    puts("PASS: recorded218650 barrel death creates47; scoped host damage retains remote credit, broadcasts and subtracts enemy HP; other shots unchanged");free(ram);return 0;
}
