/* Original projectile pool dispatch and timed fuses. Stationary mines use
 * controlled fuse-audio and detonation consumers; no live boss claim. */
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

enum { SHOT=0x1000, STACK=0x30000 };
static unsigned random_state,random_calls;
void sub_00153B40(void) {
    random_state=random_state*1664525u+1013904223u;
    g_eax=random_state;random_calls++;g_esp+=4;
}

static unsigned retired;
void sub_000A4B70(void){g_eax=1;g_esp+=4;} /* Nearby target after arming. */
void sub_00088C60(void){g_eax=0;g_esp+=4;} /* Clear launch space. */
void sub_000217B0(void){g_esp+=4;} /* Fuse audio consumer. */
void pc_native_projectile_smoke_rng(void){assert(!"unexpected smoke RNG");}
void pc_native_projectile_smoke(void){assert(!"unexpected smoke");}
void pc_native_projectile_smoke_choice(void){assert(!"unexpected smoke choice");}
void sub_000F0430(void){retired++;MEM32(g_esi)=UINT32_MAX;g_esp+=4;}
void sub_00045610(void){g_eax=0;g_esp+=4;}
void sub_00045460(void){g_esp+=4;}
void sub_000F4920(void){g_esp+=4;}
int pc_model_transform_entry(void){return 0;}
void sub_000F39A0(void);
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) do{assert(target==0xf39a0);sub_000F39A0();}while(0)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) do{fprintf(stderr,"UNEXPECTED tail %x\n",(unsigned)(target));abort();}while(0)
/* SERVICES */
/* CODE */
static double run(unsigned kind,unsigned fps){
 memset((void*)(g_xbox_mem_offset+SHOT),0,0xb89c);
 memset((void*)(g_xbox_mem_offset+0x60000),0,0x2000);
 for(unsigned n=0;n<85;n++)MEM32(SHOT+n*0x22c)=UINT32_MAX;
 MEM32(SHOT+0xa8)=0x60000;MEM32(0x60004)=0x61000;MEM32(SHOT)=kind;MEMF(SHOT+0x17c)=2;
 MEM32(0x737bd4)=SHOT;MEM32(0x4deb28+kind*64+0x34)=0xf39a0;
 g_tsfp_native_timing_active=1;retired=0;unsigned prev=0;double armed=0;
 for(unsigned i=1;i<=fps*20;i++){
  unsigned ticks=(unsigned)(i*60.0/fps+1e-6);
  MEM32(0x4e7944)=ticks-prev;prev=ticks;MEMF(0x4e794c)=60.f/fps;
  g_esp=STACK;MEM32(STACK)=0xdeadbeef;sub_000DC7B0();
  assert(g_esp==STACK+4 && g_fp_top==0);
  if(kind==18 && !armed && MEM32(SHOT+0x178)>=2) {
   armed=(double)i/fps;assert(armed>=1.5 && armed<=1.5+2./60.+.0001);
  }
  if(retired){if(kind==18)assert(armed>0);printf("kind=%u fps=%u fuse=%g\n",kind,fps,(double)i/fps);return (double)i/fps;}
 }
 assert(!"mine never detonated");return 0;
}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
 /* CONSTANTS */
 MEM32(0x4e8cec)=60;
 unsigned kinds[]={17,19,18},rates[]={60,120,144,180,240};
 for(unsigned k=0;k<3;k++)for(unsigned r=0;r<5;r++){
  double actual=run(kinds[k],rates[r]),expected=k==0?6.:k==1?2.:2.0;
  assert(fabs(actual-expected)<=(k==2?3.:1.)/60.+.0001);
 }
 puts("PASS: original timed projectile dispatch probe");return 0;
}
