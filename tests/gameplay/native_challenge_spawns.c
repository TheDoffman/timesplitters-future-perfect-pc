/* Original scheduler -> spawner -> challenge admission. Collision, allocation
 * and unrelated NPC services are controlled; no wave timing is reimplemented. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum {LEVEL=0x1000,TABLE=0x2000,DEF=0x3000,STATE=0x4000,WAVE=0x5000,
      ACTOR=0x6000,ENTITY=0x8000,STACK=0x20000};
static unsigned spawned,callbacks,npc_calls;
static int legacy;
static float multiply(uint32_t site,float a,float b) {
 if(legacy && site==0x164116)return a*b;
 return pc_native_multiply(site,a,b);
}
#undef RECOMP_F32_MUL_AT
#define RECOMP_F32_MUL_AT(site,a,b) multiply(site,a,b)
void sub_00164010(void);
void sub_00228D90(void);
static void callback(uint32_t target) {
 assert(target==0x164010);++callbacks;sub_00164010();
}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) callback(target)
void sub_0006C890(void){g_eax=1;g_esp+=4;}
void sub_00163F10(void){g_eax=1;g_esp+=4;}
void sub_0006AF90(void){g_eax=ENTITY;g_esp+=4;}
void sub_0009CCA0(void){g_eax=ENTITY;g_esp+=4;}
void sub_0022DCC0(void){sub_00228D90();}
void sub_0006BC50(void){MEM32(MEM32(g_esp+8))=0;MEM32(MEM32(g_esp+12))=0;g_esp+=4;}
void sub_0015DF20(void){++spawned;g_eax=0;g_esp+=4;}
void sub_0025F320(void){g_esp+=4;}
void sub_0015D1F0(void){++npc_calls;g_esp+=4;}
void sub_002B0B80(void){g_esp+=4;}
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) do{fprintf(stderr,"UNEXPECTED tail %x\n",(unsigned)(target));abort();}while(0)
/* SERVICES */
/* CODE */
static void setup(int native,unsigned rotations) {
 memset((void*)(g_xbox_mem_offset+LEVEL),0,0x8000);
 MEM32(0x74c36c)=LEVEL;MEM32(LEVEL+0x6c)=TABLE;MEM32(LEVEL+0x70)=rotations;
 for(unsigned n=0;n<rotations;n++)MEM32(TABLE+n*4)=0x164010;
 MEM32(LEVEL+0x74)=DEF;MEM32(LEVEL+0x78)=1;
 MEM32(DEF+0x60)=20;MEM32(DEF+0x5c)=20;MEM32(DEF+0x28)=0x435a5331;
 MEMF(DEF+0x64)=2.f;MEM32(STATE+0x10)=1;MEMF(STATE+0xc)=5.f;
 MEM32(0x7a2820)=STATE;MEM32(0x75cc88)=WAVE;MEM32(WAVE+0x24)=20;
 MEM32(0x7356d8)=ACTOR;MEM32(ACTOR)=ENTITY;
 MEM32(0x7de458)=0x40;MEM32(0x79094c)=61;MEM32(0x7de30c)=7;
 MEM32(0x790950)=0;MEM32(0x74c37c)=0;MEM8(0x7de455)=3;
 MEM32(0x74c38c)=0;MEM32(0x74c388)=0;MEM32(0x7a2958)=0;
 spawned=callbacks=npc_calls=0;g_tsfp_native_timing_active=native;
}
static void step(unsigned whole,float delta) {
 MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
 g_esp=STACK;MEM32(STACK)=0xdeadbeef;g_ebp=0x1111;g_ebx=0x2222;g_esi=0x3333;g_edi=0x4444;
 sub_0015E830();
 assert(g_esp==STACK+4 && g_ebp==0x1111 && g_ebx==0x2222 && g_esi==0x3333 && g_edi==0x4444);
 assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(int argc,char **argv) {
 (void)argv;legacy=argc>1;setbuf(stdout,NULL);
 g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
 /* CONSTANTS */
 MEM32(0x4e8cec)=60;
 const unsigned rates[]={60,120,144,180,240};
 for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)for(unsigned native=0;native<2;native++) {
  unsigned fps=rates[r],denom=fps*slow,previous=0;setup(native,1);
  double first=0,second=0;
  for(unsigned i=1;i<=denom*30;i++) {
   unsigned ticks=(unsigned)((double)i*60/denom+1e-7),whole=ticks-previous;previous=ticks;
   unsigned before=spawned;step(whole,native?60.f/denom:(float)whole);
   if(spawned!=before) {
    if(spawned==1)first=(double)i/denom;
    if(spawned==2){second=(double)i/denom;break;}
   }
  }
  printf("spawn fps=%u slow=%u native=%u first=%.6f second=%.6f\n",fps,slow,native,first,second);
  if(fabs(first-5)>1./60+.0001 || fabs(second-7)>2./60+.0001) {
   fputs("FAIL: spawn delay depends on frame rate\n",stderr);return 1;
  }
  assert(callbacks==npc_calls && MEM8(WAVE+0xa)==2 && MEM8(WAVE+0xc)==2);
  unsigned before=callbacks;float remaining=MEMF(STATE+0xc);step(0,0);
  assert(callbacks==before && MEMF(STATE+0xc)==remaining);
  step(1,0);assert(MEMF(STATE+0xc)==remaining);
 }
 /* Preserve original callback rotation and catch-up semantics, including
  * when more than one callback shares the authored scheduler. */
 for(unsigned rotations=1;rotations<=3;rotations++) {
  float reference=0;unsigned ref_count=0;
  for(unsigned native=0;native<2;native++) {
   setup(native,rotations);
   for(unsigned i=0;i<120;i++)step(i%3==0?2:0,native?.25f:(i%3==0?2.f:0.f));
   if(!native){reference=MEMF(STATE+0xc);ref_count=callbacks;}
   else assert(MEMF(STATE+0xc)==reference && callbacks==ref_count);
   assert(MEM32(0x74c38c)==callbacks%rotations);
  }
 }
 puts("PASS: original spawn scheduler/admission, rate, slow motion, pause, callback rotation and ABI");
 free((void*)g_xbox_mem_offset);return 0;
}
