/* Execute the original health HUD animation against isolated guest RAM. */
#include <assert.h>
#include <stdint.h>
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
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static int scoped=1;
static int story_campaign_gameplay(void){return scoped;}
static unsigned story_npc_humans(void){return MEM8(0x7de400);}
static uint32_t story_npc_match(void){return MEM32(0x76b180);}
static unsigned story_reload_epoch;
static struct {int active;} story_cine_state;
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
typedef struct GuestRegisters {uint32_t ax,cx,dx,bx,si,di,sp,bp;} GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_eax,g_ecx,g_edx,g_ebx,g_esi,g_edi,g_esp,g_ebp};}
static void restore_registers(const GuestRegisters *r){g_eax=r->ax;g_ecx=r->cx;g_edx=r->dx;g_ebx=r->bx;g_esi=r->si;g_edi=r->di;g_esp=r->sp;g_ebp=r->bp;}
/* ORIGINAL_HUD */
static unsigned updates;
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    assert(count==2);g_esp=0x1800;MEM32(g_esp)=0x12345678;
    va_list args;va_start(args,count);
    for(unsigned i=0;i<count;i++)MEM32(g_esp+4+i*4)=va_arg(args,uint32_t);
    va_end(args);fn();assert(g_esp==0x1804);++updates;return g_eax;
}
/* PRODUCTION_HUD */
static void frame(void){++MEM32(0x7497b8);++MEM32(0x7de318);g_esp=0x1900;story_human_hud_frame();assert(g_esp==0x1900);}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_HUD_CONSTANTS */
    MEM32(0x4e8cec)=60;MEMF(0x4e7948)=1.f;
    MEM32(0x790950)=4;MEM8(0x7de400)=4;MEM32(0x76b140)=2;MEM32(0x7de30c)=7;
    MEM32(0x7b0c48)=0x6000;MEM32(0x7356d8)=0x50000;
    for(unsigned slot=0;slot<4;slot++) {
        uint32_t actor=0x50000+slot*0xea0,context=0x6000+slot*0x1584,body=0x74000+slot*0x800,hud=0x100000+slot*0x200;
        MEM32(actor)=body;MEM32(actor+8)=slot;MEM32(actor+0x10)=2;MEMF(actor+0x7c)=100;
        MEM32(context)=slot;MEM32(context+4)=slot?0:UINT32_MAX;MEM32(context+0x14)=body;MEM32(context+0x114)=2;
        MEM32(body+0x20)=8;MEM32(body+0x7c)=actor;MEM32(body+0x150)=context;MEM32(body+0x268)=hud;
        MEMF(body+0x1cc)=35;MEMF(body+0x1d8)=35;
        guest_call(sub_0012C830,2,hud,body);
    }
    const uint32_t body=0x74800,hud=0x100200;
    MEMF(body+0x1cc)=30;guest_call(sub_0013A2C0,2,hud,body);
    assert(MEMF(hud+0x140)==2.f); /* Original hit display without an update stays visible. */
    for(unsigned i=0;i<240;i++)assert(MEMF(hud+0x140)==2.f);
    const uint32_t scenes[13]={2,4,18,11,1,17,6,22,5,14,9,26,81};
    for(unsigned mission=0;mission<13;mission++)for(unsigned count=2;count<=4;count++)for(unsigned owner=1;owner<count;owner++) {
        MEM32(0x79094c)=scenes[mission];MEM32(0x790950)=count;MEM8(0x7de400)=(uint8_t)count;
        for(unsigned slot=0;slot<4;slot++) {
            MEM32(0x6000+slot*0x1584+4)=slot==owner?0:UINT32_MAX;
            MEMF(0x74000+slot*0x800+0x1cc)=30.f;MEMF(0x100000+slot*0x200+0x140)=2.f;
        }
        for(unsigned i=0;i<125;i++)frame();
        for(unsigned slot=0;slot<4;slot++) {
            assert(slot==owner?MEMF(0x100000+slot*0x200+0x140)<=0.f:MEMF(0x100000+slot*0x200+0x140)==2.f);
            assert(MEMF(0x74000+slot*0x800+0x1cc)==30.f);
        }
    }
    MEM32(0x6000+0x1584+4)=0;
    MEM32(0x6000+2*0x1584+4)=UINT32_MAX;MEM32(0x6000+3*0x1584+4)=UINT32_MAX;
    MEMF(body+0x1cc)=20.f;frame();assert(MEMF(hud+0x140)==2.f); /* A later hit displays again. */
    MEM32(0x6000+2*0x1584+4)=UINT32_MAX;MEM32(0x6000+3*0x1584+4)=UINT32_MAX;
    MEMF(hud+0x140)=2.f;MEM32(0x7b4cc0)=body;MEM32(body)=0;
    unsigned before=updates;frame();assert(updates==before); /* Native-list bodies get no second clock. */
    guest_call(sub_0013A2C0,2,hud,body);assert(MEMF(hud+0x140)>1.98f);
    MEM32(body+0x78)=1;frame();assert(updates==before+2);
    MEM32(body+0x78)=0;MEM32(body)=body;before=updates;frame();assert(updates==before); /* Cyclic list fails closed. */
    MEM32(body)=0;MEM32(0x7b4cc0)=0;
    frame();before=updates;story_human_hud_frame();assert(updates==before); /* Same guest frame. */
    MEMF(hud+0x140)=2.f;MEMF(0x4e7948)=.5f;
    for(unsigned i=0;i<240;i++){++MEM32(0x7497b8);story_human_hud_frame();}
    assert(updates==before+240 && MEMF(hud+0x140)<.001f && MEMF(hud+0x140)>-.01f);
    MEMF(0x4e7948)=1.f;before=updates; /* Fractional frames within one console tick. */
    ++story_reload_epoch;story_human_hud_frame();assert(updates==before+1);before=updates;
    ++MEM32(0x76b180);story_human_hud_frame();assert(updates==before+1);before=updates;
    ++MEM32(0x79094c);story_human_hud_frame();assert(updates==before+1);before=updates;
    MEM32(0x76b140)=1;frame();assert(updates==before);MEM32(0x76b140)=2;
    scoped=0;frame();assert(updates==before);scoped=1;
    story_cine_state.active=1;frame();assert(updates==before);story_cine_state.active=0;
    MEM32(0x7497b4)=1;frame();assert(updates==before);MEM32(0x7497b4)=0;
    MEM32(0x7de45c)=0x02000000;frame();assert(updates==before);MEM32(0x7de45c)=0;
    MEM32(0x74800+0x150)=0;MEM32(0x6000+2*0x1584+4)=UINT32_MAX;MEM32(0x6000+3*0x1584+4)=UINT32_MAX;
    frame();assert(updates==before); /* Incomplete links and unowned contexts. */
    free(memory);puts("PASS: original health fade; missing guest HUD updates recovered; native timing and health preserved");return 0;
}
