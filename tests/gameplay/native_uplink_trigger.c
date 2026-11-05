/* Original request/repeat gates, weapon callback and Uplink charge routine; controlled targeting and
 * weapon-state handoff. The repeat gate is an original instruction block, not
 * the full weapon updater. This does not simulate the rigid solver or a map. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;int g_tsfp_native_timing_active=1;
#include "../src/gameplay/pc_native_weapon_guest.h"
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"unexpected Uplink callback")
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"unexpected Uplink tail")
enum {P=0x1000,H=P+0x184,S=0x4000,L=0x5000,A=0x6000,T=0x8000,O=0x10000,STACK=0x30000};
PcNativeStep g_tsfp_native_step;
uint32_t pc_native_demo_dispatch_mode(uint32_t v){return v;}
uint32_t pc_native_kind35_gate(uint32_t v){return v;}
uint32_t pc_native_rocking_gate(uint32_t site,uint32_t v){return v;}
static unsigned requests;
static unsigned fresh_press;
static int legacy;
static uint32_t load_gate(uint32_t site,uint32_t address,uint32_t value){
 if((legacy==1 && site==0x1c2b36) || (legacy==2 && site==0x1c397f))return value;
 return RECOMP_LOAD_GLOBAL_U32(site,address,value);
}
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) load_gate(site,address,value)
void sub_003508B0(void);
void sub_001C55F0(void);
void sub_001C395B(void);
void sub_001C2AF0(void);
void sub_001C2C00(void);
void sub_00042760(void){g_eax=1;g_esp+=4;}
void sub_000427B0(void){g_eax=fresh_press;g_esp+=4;}
void sub_001B5880(void){g_eax=0;g_esp+=4;}
void sub_00142990(void){g_esp+=4;}
void sub_00012F70(void){g_eax=0;g_esp+=4;}
/* The state-machine handoff is a controlled boundary here. Its accepted
 * request invokes original callback event 7 and the Uplink updater on the preselected object. */
void sub_001C2090(void){
 unsigned saved=g_esp;requests++;
 g_esp=STACK+0x800;MEM32(g_esp+4)=7;MEM32(g_esp+8)=H;MEM32(g_esp+12)=S;MEM32(g_esp+16)=0;
 sub_001C55F0();assert(g_esp==STACK+0x804);g_esp=saved+4;
}
void sub_001C0200(void){sub_001C2090();}
/* Exit from the isolated repeat-gate block: resume the caller, no guest RET. */
void sub_001C399C(void){}
void sub_001B7260(void){g_esp+=4;}
void sub_0025B7B0(void){g_eax=1;g_esp+=4;}
void sub_003501E0(void){MEM32(P+0x1550)=0x55;g_eax=1;g_esp+=4;}
void sub_00349390(void){g_esp+=4;}
void sub_00350120(void){g_esp+=4;}
void sub_0034EEE0(void){MEMF(O+0x2dc)=0;g_esp+=4;}
/* SERVICES */
/* CODE */
void sub_001C2AF0(void){pc_native_player_trigger_guest(sub_001C2AF0_gen,g_esp);}
int main(int argc,char **argv){
 #ifdef _WIN32
 _set_abort_behavior(0,_CALL_REPORTFAULT);
 #endif
 legacy=argc>1?(strcmp(argv[1],"legacy-request")==0?1:2):0;setvbuf(stdout,NULL,_IONBF,0);
 g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x800000);assert(g_xbox_mem_offset);
 /* CONSTANTS */
 MEM32(0x7b0c7c)=P;MEM32(P+0x14)=L;MEM32(L+0x7c)=A;
 MEM32(0x4f9bac)=T-36*0x29c;MEM32(P+0x18)=1;
 /* UPLINK_DESCRIPTOR */
 MEM32(H+0x30)=MEM32(S+0x30)=36;MEM32(H+0x38)=1;MEM32(H+0x3cc)=S;MEM32(S+0xf0)=100;MEM32(H+0x44)=1;
 MEM32(H+0x60)=1;MEM32(P+0x1550)=0x55;MEM32(P+0x1558)=0x7000;
 MEM32(0x72f5e8)=O;MEM32(O+0x2c)=0x55;MEM32(O+0x20)=2;
 MEM32(0x4e8cec)=60;
 unsigned rates[]={60,120,144,180,240};
 /* Derive the selected function through the original input/eligibility path,
  * rather than assuming that the primary button means function zero. */
 fresh_press=1;MEM32(0x4e7944)=0;MEMF(0x4e794c)=.25f;
 g_esi=H;g_esp=STACK;MEM32(STACK+4)=1;sub_001C2C00();
 assert(MEM32(H+0x5c)==1);fresh_press=0;
 for(unsigned route=0;route<2;route++)for(unsigned rate=0;rate<5;rate++)
 for(unsigned slow=1;slow<=4;slow+=3)for(unsigned phase=0;phase<4;phase++){
  unsigned fps=rates[rate];
  requests=0;MEMF(O+0x2dc)=0;
  for(unsigned frame=0;frame<fps;frame++){
   unsigned tick=(frame+phase+1)*60/fps-(frame+phase)*60/fps;
   MEM32(0x4e7944)=tick;MEMF(0x4e794c)=60.f/(fps*slow);
   g_esp=STACK;MEM32(STACK)=0x1c3dad;MEM32(STACK+4)=S;MEM32(STACK+8)=H;
   MEM32(STACK+12)=0;MEM32(STACK+16)=1;MEM32(STACK+20)=0;
   if(route){
    g_ebp=H;g_ebx=T+0xe4;g_edi=L;g_esi=S;MEM32(H+0xac)=0;MEM32(H+0xa8)=100;
    sub_001C395B();
   }else {g_esi=H;MEM32(STACK+4)=1;sub_001C2C00();}
   assert(MEM32(0x4e7944)==tick && fabsf(MEMF(0x4e794c)-60.f/(fps*slow))<1e-6);
   assert(g_esp==STACK+(route?0:4));
   /* Guest entity dispatcher replaces transient flags after each frame. */
   MEM32(O+8)=MEM32(O+0x18);MEM32(O+0x18)=0;
  }
  printf("route=%u fps=%u slow=%u phase=%u requests=%u charge=%g\n",route,fps,slow,phase,requests,MEMF(O+0x2dc));
  assert(requests==fps && "Uplink fractional holding requests lost");
  assert(fabsf(MEMF(O+0x2dc)-fminf(1,100.f/(60*slow)))<1e-5);
 }
 /* Guard scope: other guns, map mode, secondary, bad links, pause and Original. */
 MEM32(H+0x30)=MEM32(S+0x30)=36;MEM32(H+0x38)=1;MEM32(H+0x5c)=1;
 MEMF(0x4e794c)=.25f;assert(pc_native_uplink_gate(H,0)==1);
 g_tsfp_native_timing_active=0;assert(pc_native_uplink_gate(H,0)==0);g_tsfp_native_timing_active=1;
 MEMF(0x4e794c)=0;assert(pc_native_uplink_gate(H,0)==0);MEMF(0x4e794c)=.25f;
 MEM32(H+0x30)=7;assert(pc_native_uplink_gate(H,0)==0);MEM32(H+0x30)=36;
 MEM32(H+0x38)=0;assert(pc_native_uplink_gate(H,0)==0);MEM32(H+0x38)=1;
 MEM32(H+0x5c)=0;assert(pc_native_uplink_gate(H,0)==0);MEM32(H+0x5c)=1;
 MEM32(S+0x30)=7;assert(pc_native_uplink_gate(H,0)==0);MEM32(S+0x30)=36;
 assert(pc_native_uplink_gate(0,0)==0 && pc_native_uplink_gate(0xfffffff0,0)==0);
 assert(pc_native_uplink_gate(H,3)==3);
 puts("PASS: Uplink fractional requests and original charge integration");
}
