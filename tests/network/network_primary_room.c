/* Captured overlapping rooms5/27 through the original render candidate builder. */
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
static unsigned selected;
void sub_0009E430(void);
void sub_000A2430(void){assert(MEM32(g_esp+4)==0x20000);++selected;g_esp+=4;}
/* ORIGINAL_RENDER_ROOMS */
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
/* PRODUCTION_PRIMARY_ROOM */
static void render_room(unsigned room)
{
    g_esp=0x10000;sub_000A1EA0();assert(g_esp==0x10004);
    g_esp=0x10000;MEM32(g_esp+4)=room;sub_000A79B0();assert(g_esp==0x10004);
}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x72f5e8)=0x20000;MEM32(0x7b4fac)=1;MEM32(0x7b975c)=28;
    MEM32(0x715fb4)=0x50000;
    MEM32(0x20004)=0x30000;MEM32(0x20020)=8;MEM32(0x20028)=1;
    MEM8(0x20030)=2;MEM32(0x20034)=27;MEM32(0x20038)=5;MEM32(0x2003c)=27;
    MEM32(0x7b6260+5*4)=MEM32(0x7b6260+27*4)=8;
    render_room(5);assert(!selected && MEM32(0x7b54e0+27*4)==1);
    /* A fresh full-viewport scissor or forced view bits cannot repair this:
       the original never calls A2430 for the actor in the visible room5. */
    MEM32(0x2000c)=MEM32(0x20010)=3;
    MEMF(0x20118)=MEMF(0x2011c)=0;MEMF(0x20120)=639;MEMF(0x20124)=479;
    render_room(5);assert(!selected);
    pc_network_bots_entity_room(0x20000,5);
    assert(MEM32(0x20034)==5 && (MEM32(0x202b4)&0x80000000));
    for(unsigned i=0;i<16;i++) render_room(5);
    assert(selected==16 && MEM32(0x7b54e0+5*4)==1 && !MEM32(0x7b54e0+27*4));
    assert(MEM8(0x20030)==2 && MEM32(0x20038)==5 && MEM32(0x2003c)==27);
    assert(MEMF(0x20120)==639 && MEMF(0x20124)==479); /* original scissor untouched */
    /* Stale/foreign geometry, invalid IDs and corrupt lists cannot move it. */
    pc_network_bots_entity_room(0x20000,6);pc_network_bots_entity_room(0x20000,29);
    pc_network_bots_entity_room(0x20000,0);pc_network_bots_entity_room(0x20000,213);
    pc_network_bots_entity_room(0xfffffff0u,5);assert(MEM32(0x20034)==5);
    MEM8(0x20030)=11;pc_network_bots_entity_room(0x20000,27);assert(MEM32(0x20034)==5);
    MEM8(0x20030)=2;MEM32(0x202b4)=0;pc_network_bots_entity_room(0x20000,5);
    assert(!MEM32(0x202b4));
    pc_network_bots_entity_room(0x20000,27);render_room(27);assert(selected==17);
    free(ram);puts("PASS: captured stale primary omits original render candidate; host room restores it across repeated boundary views");
    return 0;
}
