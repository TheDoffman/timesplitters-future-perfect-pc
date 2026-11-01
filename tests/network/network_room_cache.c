/* Original room-dependent clipping cache after a replicated actor moves. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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
/* UNREACHED */
/* ORIGINAL_ROOM_CACHE */
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
void sub_0009FBE0(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    va_list args;va_start(args,count);
    assert(fn==sub_0009FBE0 && count==1 && va_arg(args,uint32_t)==0x20000);va_end(args);
    MEM32(0x20034)=2;return 0;
}
/* PRODUCTION_REFRESH */
static unsigned cached_mode(void)
{
    g_esp=0x10000;MEM32(g_esp+4)=0x20000;MEM32(g_esp+8)=1;
    sub_000A1AB0();assert(g_esp==0x10004);return g_eax;
}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x20020)=8;MEM32(0x20024)=0x70000;MEM8(0x20032)=2;MEM32(0x20034)=2;
    MEM32(0x7b975c)=3;MEM32(0x708788)=0x60000;MEM32(0x7b0cc0)=0x40000;
    MEM32(0x202b4)=0x40000008; /* inherited clipping plane from previous room */
    assert(cached_mode()==1); /* original positive cache is trusted, despite room change */
    pc_network_bots_entity_refresh(0x20000);
    assert(cached_mode()==0 && !(MEM32(0x202b4)&0x800000ff));
    assert(MEM32(0x202b4)&0x40000000); /* preserve unrelated cache flag */
    free(ram);puts("PASS: stale room-plane mode reproduced, network refresh recomputes original room cache");
    return 0;
}
