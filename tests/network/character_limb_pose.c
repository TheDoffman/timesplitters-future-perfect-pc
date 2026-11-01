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
/* POSE_HEADER */
#ifdef TEST_NATIVE
#undef _WIN32
#define TSFP_NATIVE_NETWORK 1
#elif !defined(_WIN32)
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
enum { BODY=0x20000,MODEL=0x30000,ACTOR=0x40000,STACK=0x10000,ASSETS=0x50000,AIM=0x51000,PARAMS=0x52000 };
static unsigned samples,weapon_selects,attachments;
enum { GUN=0x22000,GUN_MODEL=0x32000,GUN_BONES=0x62000,BODY_BONES=0x63000 };
void sub_001BE870(void){assert(MEM32(g_esp+4)==BODY);++weapon_selects;g_esp+=4;}
void sub_000A0860(void){
    assert(MEM32(g_esp+4)==GUN && weapon_selects==2);
    memcpy((void *)((uintptr_t)GUN_BONES+g_xbox_mem_offset),(void *)((uintptr_t)BODY_BONES+g_xbox_mem_offset),64);
    ++attachments;g_esp+=4;
}
void sub_000966E0(void){assert(0);}
void sub_00057550(void){assert(0);}
void sub_00059320(void){
    assert(MEM32(g_esp+4)==AIM);assert(MEM32(g_esp+8)==11);
    assert(MEMF(g_esp+20)==17.25f);++samples;
    uint32_t output=MEM32(g_esp+16);memset((void *)((uintptr_t)output+g_xbox_mem_offset),0,28);
    MEMF(output+12)=1;g_esp+=4;
}
void sub_00150960(void){
    uint32_t output=MEM32(g_esp+4);memset((void *)((uintptr_t)output+g_xbox_mem_offset),0,28);
    MEMF(output+12)=1;g_esp+=4;
}
/* ORIGINAL_LIMB */
/* PRODUCTION_POSE */
/* PRODUCTION_WEAPONS */
int main(void){
    uint8_t *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* ORIGINAL_CONSTANTS */
    MEM32(BODY+4)=MODEL;MEM32(MODEL)=BODY;MEM32(BODY+0x7c)=ACTOR;MEM32(ACTOR)=BODY;
    MEM32(MODEL+0xd4)=42;MEM32(MODEL+0x9c)=8184;MEM32(MODEL+0xa0)=108;
    MEM32(MODEL+0xd0)=18;MEMF(MODEL+0xc8)=.4f;MEMF(MODEL+0xcc)=.1f;
    MEMF(ACTOR+0xb08)=17.25f;
    PcCharacterPose pose;pc_character_pose_read(&pose,ram+MODEL,ram+ACTOR);
    assert(sizeof pose==232 && pc_character_pose_valid(&pose));
    MEM32(MODEL+0x9c)=0;MEM32(MODEL+0xa0)=0;MEM32(MODEL+0xd0)=1;MEMF(ACTOR+0xb08)=0;
    MEM32(0x7e3954)=ASSETS;*(int16_t *)(ram+0x7e03c0+108*2)=7;MEM32(ASSETS+7*4)=AIM;
    /* Clip0 is deliberately unloaded; sending only a mask/enable bit used to
       enter original5AFB0 with that invalid local resource index. */
    *(int16_t *)(ram+0x7e03c0)=-1;
    g_esp=STACK;pc_character_pose_present(BODY,&pose);
    assert(MEM32(MODEL+0x9c)==8184 && MEM32(MODEL+0xa0)==108 && MEM32(MODEL+0xd0)==18);
    assert(MEMF(MODEL+0xcc)==.1f && MEMF(ACTOR+0xb08)==17.25f);
    MEM32(0x6d77e0)=PARAMS;MEM32(0x6d6e10)=PARAMS;MEM32(0x6d77e8)=0;
    g_esp=STACK;MEM32(STACK+4)=BODY;MEM32(STACK+8)=0x60000;MEM32(STACK+12)=11;
    if(!(MEM32(MODEL+0xd0)&1u))sub_0005AFB0();
    assert(samples>=4 && g_esp==STACK+4);
    *(int16_t *)(ram+0x7e03c0+108*2)=-1;g_esp=STACK;pc_character_pose_present(BODY,&pose);
    assert(MEM32(MODEL+0xd0)==1); /* wait for a locally loaded clip */
    /* A frozen replica's weapon must use the newly evaluated arm before its
       origin/room is refreshed. Its old root deliberately points elsewhere. */
    MEMF(BODY+0x1cc)=100;MEM32(BODY+0x28)=0x200001;MEM32(MODEL+0x10)=BODY_BONES;
    MEMF(BODY+0x64)=30;MEMF(BODY+0x68)=2;MEMF(BODY+0x6c)=20;MEM32(BODY+0x34)=27;
    MEMF(BODY_BONES+0x30)=.4f;MEMF(BODY_BONES+0x34)=1.2f;MEMF(BODY_BONES+0x38)=.5f;
    MEM32(ACTOR+0x18c)=GUN;MEM32(ACTOR+0x190)=ACTOR;
    MEM32(GUN+0x20)=0x20;MEM32(GUN+4)=GUN_MODEL;MEM32(GUN+0x7c)=ACTOR+0xa4;
    MEM32(GUN+0x78)=MODEL;MEM32(GUN_MODEL+0x10)=GUN_BONES;
    MEMF(GUN_BONES+0x30)=99;g_esp=STACK;pc_network_bots_pose_weapons(BODY);
    assert(attachments==1 && MEMF(GUN+0x248)==.4f && MEMF(GUN+0x24c)==1.2f);
    assert(MEMF(GUN+0x64)==30 && MEMF(GUN+0x68)==2 && MEM32(GUN+0x34)==27);
    MEM32(GUN+0x7c)=ACTOR;pc_network_bots_pose_weapons(BODY);assert(attachments==1);
    MEM32(GUN+0x7c)=ACTOR+0xa4;MEM32(GUN+0x28)=0x80000000;
    pc_network_bots_pose_weapons(BODY);assert(attachments==1);
    MEM32(GUN+0x28)=0;MEM32(BODY+0x28)=1;pc_network_bots_pose_weapons(BODY);assert(attachments==1);
    pose.limb_elapsed=NAN;assert(!pc_character_pose_valid(&pose));pose.limb_elapsed=.1f;
    pose.aim_frame=INFINITY;assert(!pc_character_pose_valid(&pose));pose.aim_frame=17.25f;
    pose.limb_clips_flags|=0x80000000u;assert(!pc_character_pose_valid(&pose));
    free(ram);puts("PASS: original arm sampler receives captured clip108 and aim frame through local assets; mask/blend restored and unloaded resources rejected");return 0;
}
