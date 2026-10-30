/* Decode an actual stock weapon-drop packet without constructing game assets. */
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
void sub_00013000(void){assert(!"mode1 not used");}
void sub_00359390(void){assert(!"mode1 not used");}
static unsigned drops,constructed,real_drop;
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static unsigned test_humans=32;
static unsigned story_npc_humans(void){return test_humans;}
static int story_npc_session(void){return MEM8(0x7de455)==10;}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
void sub_000C2090_gen(void);
/* PRODUCTION_REPLAY */
void sub_00358130(void){g_eax=MEM32(0x541e9c);MEM32(0x541e9c)=1;g_esp+=4;}
void sub_0006BEC0(void){g_eax=1;g_esp+=4;}
void sub_00012F50(void){g_eax=0;g_esp+=4;}
void sub_001BADD0(void){g_eax=112;g_esp+=4;}
void sub_001BAE40(void){g_eax=0;g_esp+=4;}
void sub_000C1950(void)
{
    assert(MEM8(0x7de455)==0);
    assert(MEM32(g_esp+4)==1 && MEM32(g_esp+8)==2 && MEM32(g_esp+12)==25);
    assert(MEM32(g_esp+16)==0 && MEM32(g_esp+24)==0x4db9a8);
    assert(MEM32(g_esp+28)==18 && MEM32(g_esp+32)==112);
    assert(MEM32(g_esp+36)==0 && MEM32(g_esp+40)==0);
    ++constructed;g_eax=0;g_esp+=4; /* Controlled allocation failure after validating original budget. */
}
/* UNREACHED */
void sub_000C2090(void)
{
    uint32_t actor=MEM32(g_esp+8),hand=MEM32(g_esp+12);
    assert(MEM32(g_esp)==0x35b51c && MEM32(0x541e9c)==0);
    assert(MEM32(actor+8)==2 && MEM32(actor+0x94)==25);
    assert(MEM32(actor+0xbf0)==0); /* wire actor lacks Story NPC resource */
    assert(MEM32(hand+0x30)==14 && SMEM16(actor+0x61e+18*2)==112);
    ++drops;if(real_drop)pc_story_network_drop_replay(sub_000C2090_gen);else g_esp+=4;
}
static unsigned original_calls;
static void ordinary_drop(void){assert(MEM8(0x7de455)==10);++original_calls;g_esp+=4;}
/* ORIGINAL_DROP_RECEIVER */
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x7356d8)=0x50000;MEM32(0x50000+2*0xea0+8)=2;
    MEM32(0x4f9bac)=0x60000;MEM32(0x60000+14*0x29c+0x14)=18;
    MEM8(0x90000)=0;MEM32(0x90001)=123;MEM8(0x90005)=2;MEM32(0x90006)=0x79;
    MEM16(0x90025)=37;MEM8(0x90027)=14;MEM8(0x90028)=25;
    MEM16(0x90029)=112;MEM16(0x9002b)=5;
    g_esp=0x30000;MEM32(g_esp+4)=0x90000;sub_0035B190();
    assert(g_esp==0x30004 && drops==1 && MEM32(0x541e9c)==1 && MEM32(0x541ea0)==0xffffffff);
    MEM32(0x76b140)=2;MEM32(0x76b1a4)=15;MEM8(0x7de455)=10;MEM32(0x790950)=2;
    real_drop=1;g_esp=0x30000;MEM32(g_esp+4)=0x90000;sub_0035B190();
    assert(g_esp==0x30004 && constructed==1 && drops==2 && MEM8(0x7de455)==10);
    assert(MEM32(0x541e9c)==1 && MEM32(0x541ea0)==0xffffffff);
    test_humans=2;MEM32(0x79094c)=2;g_esp=0x30000;MEM32(g_esp+4)=0x90000;sub_0035B190();
    assert(g_esp==0x30004 && constructed==1 && drops==3); /* NPC stock replay waits for authoritative item state. */
    test_humans=32;
    g_esp=0x30000;MEM32(g_esp)=0x35b51c;MEM32(g_esp+8)=0x50000;MEM32(g_esp+12)=0x52000;
    MEM32(0x50008)=2;MEM32(0x50094)=25;MEM32(0x52030)=14;
    MEM32(0x76b140)=1;pc_story_network_drop_replay(ordinary_drop);
    g_esp=0x30000;MEM32(0x76b140)=2;MEM32(0x76b1a4)=3;pc_story_network_drop_replay(ordinary_drop);
    g_esp=0x30000;MEM32(0x76b1a4)=15;MEM32(0x50bf0)=0x60000;pc_story_network_drop_replay(ordinary_drop);
    g_esp=0x30000;MEM32(0x50bf0)=0;MEM32(g_esp)=0x12345;pc_story_network_drop_replay(ordinary_drop);
    assert(original_calls==4 && MEM8(0x7de455)==10);
    free(ram);puts("PASS: original NPC drop wire, temporary actor inventory and missing Story descriptor");
    return 0;
}
