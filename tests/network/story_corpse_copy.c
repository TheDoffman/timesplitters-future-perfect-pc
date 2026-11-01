/* Execute the original cosmetic corpse constructor and its script callback gate. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo,g_xbox_code_hi;
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_trace_idx;
volatile uint64_t g_icall_count;
void recomp_icall_not_code_log(uint32_t a,uint32_t b){(void)a;(void)b;assert(0);}
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t a){(void)a;assert(0);return NULL;}
void recomp_icall_fail_log(uint32_t a){(void)a;assert(0);}

#include <math.h>
/* CHARACTER_POSE_HEADER */
static unsigned copies,events,recycled;
void sub_000A6EC0(void){assert(MEM32(g_esp+4)==0x1000);++copies;MEM32(0x22004)=0x42000;g_eax=0x22000;g_esp+=4;}
void sub_000A9070(void){assert(MEM32(g_esp+4)==0x22000);++recycled;g_esp+=4;}
void sub_0009F900(void){assert(MEM32(g_esp+4)==0x22000 && MEM32(g_esp+8)==0x20000);g_esp+=4;}
void sub_0009FAA0(void){assert(MEM32(g_esp+4)==0x22000 && MEM32(g_esp+8)==0x20000);MEM32(0x420d4)=MEM32(0x400d4);MEM32(0x420dc)=MEM32(0x400dc);g_esp+=4;}
void sub_000B3560(void){g_esp+=4;}
void sub_000A5CE0(void){g_esp+=4;}
void sub_0009E790(void){g_eax=1;g_esp+=4;}
void sub_000D6ED0(void){g_eax=0;g_esp+=4;}
void sub_0027CBD0(void),sub_0027CEF0(void),sub_0027CB10(void);
void sub_0015C310_gen(void);
void sub_0015C310(void){assert(MEM32(g_esp+8)==0x3ef);assert(!MEM32(0x30010)&&!MEM32(0x30014)&&!MEM32(0x30074));++events;sub_0015C310_gen();}
/* UNREACHED */
/* ORIGINAL_CORPSE_COPY */
#define WORD_AT(a) MEM32(a)
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    assert(fn==sub_000B3630 && count==1);va_list args;va_start(args,count);uint32_t entity=va_arg(args,uint32_t);va_end(args);
    uint32_t frame=g_esp;g_esp-=8;MEM32(g_esp)=0;MEM32(g_esp+4)=entity;fn();assert(g_esp==frame-4);g_esp=frame;return g_eax;
}
void pc_character_ragdoll_preserve(uint32_t e,uint32_t c){(void)e;(void)c;}
/* PRODUCTION_CORPSE_COPY */
/* PRODUCTION_BOT_CORPSE_BRIDGE */
/* PRODUCTION_BOT_REMOVAL */
static void retire_body(void)
{
    assert(copies==4); /* must be copied before original retirement */
    MEM32(0x20028)|=0x80000000;MEM32(0x30000)=0;g_esp+=4;
}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM8(0x7de455)=10;MEM32(0x79094c)=2;
    g_esp=0x10000;MEM32(0x732a10)=8;MEM32(0x732d18)=0;
    MEM32(0x20020)=8;MEM32(0x2007c)=0x30000;MEM32(0x30000)=0x20000;MEM32(0x20004)=0x40000;
    MEM32(0x30008)=9;MEM16(0x3004a)=91;MEM32(0x30010)=1;MEM32(0x30074)=0x12345;
    MEM32(0x400d4)=74;MEM32(0x400dc)=0x3fe66666; /* recorded death clip74 at1.8seconds */
    pc_character_preserve_corpse(0x30000,0x20000);
    assert(copies==1 && events==1 && MEM32(0x22020)==0x1000);
    assert(MEM32(0x732a1c)==8 && MEM32(0x732a20)==91 && MEM32(0x7b2e20)==0x22000);
    assert(MEM32(0x420d4)==74 && MEM32(0x420dc)==0x3fe66666);
    assert(g_esp==0x10000 && MEM32(0x30bf0)==0 && MEM32(0x30074)==0x12345 && MEM32(0x30010)==1);
    assert(MEM32(0x30000)==0x20000); /* independent cosmetic body; actor retirement can follow */
    MEM32(0x732a10)=1;MEM32(0x732d18)=0;MEM32(0x30bf0)=0x50000;MEM8(0x50058)=8;
    pc_character_preserve_corpse(0x30000,0x20000);
    assert(copies==2 && events==2 && recycled==1 && MEM32(0x732d18)==0);
    assert(MEM32(0x30bf0)==0x50000 && MEM32(0x732a18)==1);
    MEM32(0x201cc)=0x3f800000;pc_character_preserve_corpse(0x30000,0x20000);assert(copies==2);
    MEM32(0x201cc)=0;MEM32(0x20028)=0x80000000;pc_character_preserve_corpse(0x30000,0x20000);assert(copies==2);
    MEM32(0x20028)=0;MEM32(0x732a10)=0;pc_character_preserve_corpse(0x30000,0x20000);assert(copies==2);
    MEM32(0x732a10)=8;MEM32(0x732d18)=0;
    MEM32(0x76b140)=2;MEM32(0x76b1a4)=15;
    MEM8(0x7de400)=2;MEM8(0x7de402)=8;
    MEM32(0x7356d8)=0x30000-9*0xea0;MEM32(0x20258)=123;
    assert(pc_network_bots_preserve_corpse(0x20000,123) && copies==3);
    assert(!pc_network_bots_preserve_corpse(0x20000,124) && copies==3);
    MEM32(0x76b140)=1;assert(!pc_network_bots_preserve_corpse(0x20000,123));
    MEM32(0x76b140)=2;MEM32(0x7356d8)=0x30000; /* human slot */
    assert(!pc_network_bots_preserve_corpse(0x20000,123) && copies==3);
    MEM32(0x7356d8)=0x30000-9*0xea0;MEM32(0x20258)=124;
    MEM32(g_esp+4)=0x20000;pc_story_network_world_remove(retire_body);
    assert(copies==4 && MEM32(0x30000)==0 && (MEM32(0x20028)&0x80000000));
    g_esp-=4;MEM32(0x20028)=0;MEM32(0x30000)=0x20000;
    assert(pc_network_bots_preserve_corpse(0x20000,124) && copies==4); /* same generation replay */
    free(ram);puts("PASS: original bounded corpse copy retains death pose independently of retired actor; restores guest descriptor and script state");return 0;
}
