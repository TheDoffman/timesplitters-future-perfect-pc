/* Execute original pickup construction and network identity allocation. */
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

#include <math.h>
static volatile unsigned marker;
void sub_00353A90(void){marker=1;}void sub_000A9070(void){assert(0);}
void sub_00358160(void),sub_00355EC0(void);
void sub_003533F0(void){g_eax=MEM32(0x76b108);g_esp+=4;}
void sub_000BE860(void){assert(MEM32(g_esp+4)==2 && MEM32(g_esp+8)==25);g_eax=0x65000;g_esp+=4;}
void sub_000DB2C0(void){g_eax=1825;g_esp+=4;}
void sub_000A6F90(void){assert(MEM32(g_esp+4)==1825);MEM32(0x72004)=0x74000;MEM32(0x7202c)=123;MEM32(0x72028)=1;g_eax=0x72000;g_esp+=4;}
void sub_000A5CE0(void){g_esp+=4;}
void sub_0010E110(void){g_esp+=4;}
void sub_00153B40(void){g_eax=0;g_esp+=4;}
void sub_000966E0(void){g_esp+=4;}
/* UNREACHED */
/* ORIGINAL_CONSTRUCTOR */
#ifndef _WIN32
#define _WIN32 1
#endif
#define WORD_AT(a) MEM32(a)
typedef struct GuestRegisters {uint32_t sp,ax;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;r->ax=g_eax;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->ax;}
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<0x800000 && n<=0x800000-a;}
static int story_npc_session(void){return 1;}
static unsigned story_npc_humans(void){return 2;}
static uint32_t story_npc_match(void){return 42;}
void pc_network_bots_entity_refresh(uint32_t e){assert(e==0x72000);}
#include "pc_physical_state.h"
void pc_rigid_body_capture(PcRigidBodyState *s,uint32_t e){(void)e;memset(s,0,sizeof *s);}
void pc_rigid_body_remove(uint32_t e){(void)e;}
int pc_rigid_body_present(uint32_t e,const PcRigidBodyState *s){(void)e;(void)s;return 1;}
void pc_character_ragdoll_capture(PcRagdollState *s,uint32_t e){(void)e;memset(s,0,sizeof *s);}
void pc_character_ragdoll_present(uint32_t e,const PcRagdollState *s){(void)e;(void)s;}
void sub_0034FF30(void){}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
 assert(fn==sub_000C1950 && count==10);uint32_t frame=g_esp;g_esp-=44;MEM32(g_esp)=0;
 va_list ap;va_start(ap,count);for(unsigned i=0;i<count;i++)MEM32(g_esp+4+i*4)=va_arg(ap,uint32_t);va_end(ap);
 fn();assert(g_esp==frame-40);g_esp=frame;return g_eax;
}
#if defined(_MSC_VER) && !defined(__STDC_VERSION__)
#define _Static_assert(c,m) typedef char fixture_size_contract[(c)?1:-1]
#endif
/* PRODUCTION_DROP_STATE */
int main(void)
{
 void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
 g_esp=0x20000;MEM32(0x76b108)=1;MEM32(0x76b140)=2;MEM32(0x76b1a4)=15;MEM32(0x79094c)=2;MEM32(0x7de30c)=7;MEM8(0x7de455)=10;
 MEM32(0x4db9a8)=1;MEM32(0x4db9ac)=0x3f800000;MEM32(0x4db9b0)=0x3f000000;MEM32(0x475c78)=0x3f800000;
 MEM32(0x65020)=0x3f800000;MEM32(0x541ea4)=777;
 StoryDropState s={0};s.magic=STORY_DROP_MAGIC;s.match=42;s.slot=28;s.serial=1;s.present=1;s.id=91;s.source=6;s.weapon=25;
 s.ammo[0]=18;s.quantity[0]=126;s.position[0]=60.25885f;s.position[1]=-.807326f;s.position[2]=-1.695261f;s.angles[1]=1.2f;
 assert(story_drop_apply(&s));const uint32_t owner=MEM32(0x7207c);
 assert(owner>=0x7b0d20 && owner<0x7b0d20+30*0xd8 && MEM32(owner)==0x72000);
 assert(MEM32(0x72020)==0x40 && MEM32(0x72230)==91 && MEM32(0x541ea4)==777);
 assert(MEM32(owner+0xc)==2 && MEM32(owner+0x14)==25 && MEM32(owner+0x40)==18 && MEM32(owner+0x44)==126);
 assert(MEM32(owner+0xa4)==6 && MEM32(owner+0xa0)==1 && MEM32(owner+0xc0)==3 && g_esp==0x20000);
 float y;memcpy(&y,&MEM32(0x72068),4);assert(y==s.position[1]);
 free(ram);puts("PASS: original pickup constructor creates captured weapon25/126ammo with host runtimeID, original placement defaults and restored ID scope");return 0;
}
