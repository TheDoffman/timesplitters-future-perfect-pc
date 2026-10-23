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
void sub_00064D10(void){assert(!"unexpected completed-mission path");}
void sub_000389B0(void){assert(!"unexpected movie completion path");}
void sub_001CD590(void){assert(MEM32(g_esp+4)==MEM32(0x4d1ae0));++prepared;g_esp+=4;}
void sub_00064DD0(void){g_eax=12;g_esp+=4;}
static unsigned ready_packets;
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
/* ORIGINAL_STAGE_BUILDER */
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
    puts("PASS: original Story stages, cinematic-ready transport and guest ABI");free(memory);return 0;
}
