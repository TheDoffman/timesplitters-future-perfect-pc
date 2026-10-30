/* Exercise the original room scissor with the production replica adapter. */
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
/* ORIGINAL_ROOM_SCISSOR */
#define WORD_AT(a) MEM32(a)
#ifndef _WIN32
#define _WIN32 1 /* exercise the Windows Online branch on either fixture host */
#endif
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static int replica;
int pc_story_network_replica(uint32_t entity){return replica && entity==0x20000;}
/* PRODUCTION_REPLICA_CLIP */
static void evaluate(int wrapped)
{
    g_esp=0x10000;MEM32(g_esp+4)=0x20000;
    if(wrapped) pc_story_network_replica_clip(sub_000862C0);
    else sub_000862C0();
    assert(g_esp==0x10004 && MEM32(0x20034)==5);
}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x7b0cc0)=0x40000;MEM32(0x7e71c8)=0x30000;
    MEM32(0x30000)=MEM32(0x30014)=0;MEM32(0x30004)=640;MEM32(0x30018)=480;
    MEMF(0x475db8)=1e20f;MEMF(0x4760d0)=-1e20f;
    MEM32(0x20034)=MEM32(0x20038)=5;MEM8(0x20030)=1;
    MEM32(0x40000+0x3bc+5*12)=0xffffffff; /* room has no local portal region */
    evaluate(0);assert(MEMF(0x20118)>MEMF(0x20120)); /* reproduce rejection */
    replica=1;evaluate(1);
    assert(MEMF(0x20118)>MEMF(0x20120) && (MEM32(0x202b4)&0x80000000));
    assert(MEM8(0x20030)==1 && MEM32(0x20038)==5);
    /* Keep the real portal scissor instead of overwriting the room with0. */
    MEM32(0x40000+0x3bc+5*12)=0;
    MEM16(0x40000+0x3bc+5*12+4)=100;MEM16(0x40000+0x3bc+5*12+6)=120;
    MEM16(0x40000+0x3bc+5*12+8)=300;MEM16(0x40000+0x3bc+5*12+10)=340;
    evaluate(1);assert(MEMF(0x20118)==100 && MEMF(0x2011c)==120 && MEMF(0x20120)==300 && MEMF(0x20124)==340);
    MEM8(0x20030)=0;evaluate(1);assert(MEM8(0x20030)==0);
    replica=0;MEM8(0x20030)=1;MEM32(0x202b4)=0;evaluate(1);
    assert(MEMF(0x20118)==100 && !MEM32(0x202b4));
    free(ram);puts("PASS: original portal clipping preserved, replica cache invalidated, spatial fields unchanged");
    return 0;
}
