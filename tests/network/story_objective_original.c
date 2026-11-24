/* Original US objective notices with Online Story's owned local HUD mapping. */
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
static int story_npc_session(void){return scoped;}
static int story_campaign_gameplay(void){return scoped;}
static unsigned story_npc_humans(void){return MEM8(0x7de400);}
static uint32_t story_npc_match(void){return MEM32(0x76b180);}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
typedef struct GuestRegisters {uint32_t ax,cx,dx,bx,si,di,sp,bp;} GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_eax,g_ecx,g_edx,g_ebx,g_esi,g_edi,g_esp,g_ebp};}
static void restore_registers(const GuestRegisters *r){g_eax=r->ax;g_ecx=r->cx;g_edx=r->dx;g_ebx=r->bx;g_esi=r->si;g_edi=r->di;g_esp=r->sp;g_ebp=r->bp;}
void sub_001CDAE0(void){g_eax=1;g_esp+=4;}
void sub_00064CD0(void){g_eax=1;g_esp+=4;}
void sub_00045390(void){g_esp+=4;}
void sub_00045820(void){g_esp+=4;}
void sub_00353A90(void){assert(!"Guest must not publish objective state");}
/* ORIGINAL_OBJECTIVE */
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    g_esp=0x2000;MEM32(g_esp)=0x12345678;
    va_list args;va_start(args,count);
    for(unsigned i=0;i<count;i++)MEM32(g_esp+4+i*4)=va_arg(args,uint32_t);
    va_end(args);fn();assert(g_esp==0x2004);return g_eax;
}
/* PRODUCTION_OBJECTIVE */
static void receive_state(unsigned serial,unsigned state)
{
    StoryObjective s={STORY_OBJECTIVE_MAGIC,7,serial,0,1,0,state,0};
    memcpy((void *)((uintptr_t)0x32000+g_xbox_mem_offset),&s,sizeof s);
    assert(story_objectives_receive(0x31000,0x32000,0x1e,sizeof s));
}
static void frame(void){g_esp=0x1900;story_objectives_frame();assert(g_esp==0x1900);}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_OBJECTIVE_CONSTANTS */
    MEM32(0x4e8cec)=60;MEM32(0x76b140)=2;MEM32(0x76b180)=7;
    MEM32(0x774040)=9;MEM32(0x31000)=9;MEM32(0x78cec8)=2;MEM32(0x78cecc)=0x100000;
    MEM16(0x100000)=1;MEM8(0x100008)=0;MEM8(0x10000b)=2;
    MEM32(0x100038)=0x12345678;MEM32(0x10003c)=0x12345678;
    MEM8(0x10004c+8)=2;MEM8(0x10004c+11)=2; /* Executable VM record stays local. */
    MEM32(0x7b0c48)=0x6000;
    for(unsigned count=2;count<=4;count++)for(unsigned owner=1;owner<count;owner++) {
        MEM32(0x790950)=count;MEM8(0x7de400)=(uint8_t)count;
        for(unsigned slot=0;slot<count;slot++) {
            MEM32(0x6000+slot*0x1584+4)=slot==owner?0:UINT32_MAX;
            MEM32(0x6000+slot*0x1584+0x114)=2;
        }
        memset(memory+0x7a6160,0,4*0x177c);story_objectives_reset();MEM8(0x10000b)=2;
        receive_state(1,2);frame();
        assert(MEM32(0x7a6198)==6 && MEM32(0x7a6198+0x164)==0x100000 && MEM32(0x7a6190)==1);
        assert(MEM32(0x7a6160+0x177c+0x30)==0); /* Actor slot is not a local HUD index. */
        /* Intro setup may discard a queued banner before its delay expires. */
        MEMF(0x7a6198+0x144)=-2.f;frame();assert(MEM32(0x7a6190)==1);
        memset(memory+0x7a6160,0,4*0x177c);MEM32(0x7de45c)=0x02000000;
        frame();assert(MEM32(0x7a6190)==0);
        MEM32(0x7de45c)=0;frame();assert(MEM32(0x7a6190)==1);
        frame(); /* Positive elapsed acknowledges actual visibility. */
        receive_state(2,2);frame();frame();assert(MEM32(0x7a6190)==1); /* Refresh/retry is silent. */
        receive_state(3,3);frame();assert(MEM32(0x7a6190)==2); /* Later transition still notifies. */
        memset(memory+0x7a6160,0,4*0x177c);story_objectives_reset();MEM8(0x10000b)=2;
        MEM32(0x6000+owner*0x1584+0x114)=256;receive_state(1,2);frame();assert(MEM32(0x7a6190)==0);
        receive_state(2,2);frame();assert(MEM32(0x7a6190)==0);
        MEM32(0x6000+owner*0x1584+0x114)=2;frame();assert(MEM32(0x7a6190)==1);
        story_objectives_reset();receive_state(1,2);frame();assert(MEM32(0x7a6190)==1); /* Already queued by native setup. */
        scoped=0;receive_state(2,3);frame();assert(MEM32(0x7a6190)==1);scoped=1;
        assert(MEM32(0x100038)==0x12345678 && MEM32(0x10003c)==0x12345678 && MEM8(0x10004c+11)==2);
    }
    free(memory);puts("PASS: original objective popup reaches owned P2/P3/P4 HUD0; initial, deferred and deduplicated notices");return 0;
}
