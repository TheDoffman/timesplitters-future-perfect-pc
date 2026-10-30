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

#include <stdarg.h>
/* CAMO_HEADER */
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
typedef struct GuestRegisters {uint32_t sp,ax,si;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;r->ax=g_eax;r->si=g_esi;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->ax;g_esi=r->si;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...){
    uint32_t frame=g_esp;va_list args;va_start(args,count);g_esp=frame-256;
    for(unsigned i=0;i<count;i++)MEM32(g_esp+4+i*4)=va_arg(args,uint32_t);
    va_end(args);fn();assert(g_esp==frame-252);g_esp=frame;return g_eax;
}
enum { BODY=0x20000,WEAPON=0x21000,MODEL=0x30000,GUN_MODEL=0x31000,ACTOR=0x40000,STACK=0x10000 };
static unsigned material_writes,random_calls;
static unsigned body_alpha=127,body_effect;
void sub_0011F780(void),sub_0011D400(void),sub_0011D510(void);
void sub_003C848C(void){g_eax=(uint32_t)(int32_t)g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;g_esp+=4;}
void sub_00153B40(void){++random_calls;g_eax=0;g_esp+=4;}
void sub_00153BD0(void){++random_calls;g_fp_top=(g_fp_top-1)&7;g_fp_stack[g_fp_top]=.5;g_esp+=4;}
void sub_00094220(void){
    const uint32_t model=MEM32(g_esp+4),property=MEM32(g_esp+8),value=MEM32(g_esp+20);
    assert(model==MODEL || model==GUN_MODEL);++material_writes;
    if(model==MODEL && property==15)body_alpha=value;
    if(model==MODEL && property==19)body_effect=value;
    g_esp+=4;
}
/* UNREACHED */
/* ORIGINAL_CAMO */
/* PRODUCTION_CAMO_PRESENT */
static float opacity(void){
    g_esp=STACK;MEM32(STACK+4)=ACTOR+0xc68;sub_0011F780();
    assert(g_esp==STACK+4);float v=(float)g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;return v;
}
int main(void){
    uint8_t *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x76b140)=2;MEM32(0x4e8cec)=60;MEM32(0x4e7944)=1;MEMF(0x4e794c)=1;
    MEM32(BODY+4)=MODEL;MEM32(BODY+0x7c)=ACTOR;MEM32(ACTOR)=BODY;MEM32(ACTOR+8)=7;
    MEM32(ACTOR+0x18c)=WEAPON;MEM32(WEAPON+4)=GUN_MODEL;
    MEM32(ACTOR+0xc68)=ACTOR;MEM32(ACTOR+0xc74)=0x12345678;MEM32(ACTOR+0xcbc)=0xffffffff;
    assert(opacity()==1.f); /* recorded guest: effect flags/phase all zero */
    PcCharacterCamo captured={0};captured.flags=0x01000010;captured.fade_phase=1;
    captured.fade_values[0]=2.116665f;captured.fade_values[2]=.5f;captured.fade_values[3]=.7f;
    captured.cycle_phase=1;captured.cycle_values[0]=4;captured.cycle_values[1]=.25f;
    captured.cycle_values[2]=2.43f;captured.cycle_values[3]=5.4f;
    assert(pc_character_camo_valid(&captured));pc_character_camo_apply(ram+ACTOR,&captured);
    assert(opacity()==0.f); /* original getter used by character/weapon rendering */
    g_esp=STACK;g_eax=0xabc;g_esi=0xdef;pc_character_camo_present(BODY);
    assert(material_writes && g_esp==STACK && g_eax==0xabc && g_esi==0xdef);
    PcCharacterCamo after;pc_character_camo_capture(&after,ram+ACTOR);assert(!memcmp(&after,&captured,sizeof after));
    assert(MEM32(ACTOR+0xc68)==ACTOR && MEM32(ACTOR+0xc74)==0x12345678 && MEM32(ACTOR+0xcbc)==0xffffffff);
    /* Fade-in/out uses the original material path while keeping host state. */
    captured.fade_phase=2;captured.fade_values[0]=.25f;pc_character_camo_apply(ram+ACTOR,&captured);
    assert(opacity()==.5f);g_esp=STACK;pc_character_camo_present(BODY);
    assert(opacity()==.5f);pc_character_camo_capture(&after,ram+ACTOR);assert(!memcmp(&after,&captured,sizeof after));
    captured.fade_values[0]=.063f;pc_character_camo_apply(ram+ACTOR,&captured);
    const unsigned before=material_writes;g_esp=STACK;pc_character_camo_present(BODY);
    assert(material_writes>before && body_effect==20); /* original shimmer band */
    pc_character_camo_capture(&after,ram+ACTOR);assert(!memcmp(&after,&captured,sizeof after));
    memset(&captured,0,sizeof captured);pc_character_camo_apply(ram+ACTOR,&captured);
    g_esp=STACK;pc_character_camo_present(BODY);assert(body_alpha==127 && opacity()==1.f);
    const unsigned writes=material_writes;g_esp=STACK;pc_character_camo_present(BODY);assert(material_writes==writes);
    MEM32(0x76b140)=1;captured.flags=0x10;captured.fade_phase=1;pc_character_camo_apply(ram+ACTOR,&captured);
    g_esp=STACK;pc_character_camo_present(BODY);assert(material_writes==writes);
    captured.fade_phase=3;assert(!pc_character_camo_valid(&captured));captured.fade_phase=0;
    assert(!pc_character_camo_valid(&captured));captured.fade_values[2]=.5f;
    assert(pc_character_camo_valid(&captured));captured.fade_values[0]=NAN;assert(!pc_character_camo_valid(&captured));
    free(ram);puts("PASS: recorded guest opacity1 corrected to original host opacity0; original material fade, restoration, local handles and host numbers preserved; no guest cloak-cycle RNG");return 0;
}
