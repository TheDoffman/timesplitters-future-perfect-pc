/* Original fixed-length beam traces and damage dispatch. A controlled collision
 * target replaces the scene; original hit and damage calculation still execute. */
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
static unsigned random_state,random_calls,sweeps;
void sub_00153B40(void) {
    random_state=random_state*1664525u+1013904223u;
    g_eax=random_state;random_calls++;g_esp+=4;
}
/* Collision, impact effects and character hit classification are controlled. */
void sub_000DCB80(void){sweeps++;g_eax=0x2000;g_esp+=4;}
void sub_000913D0(void){g_eax=0;g_esp+=4;}
void sub_000E9FE0(void){g_esp+=4;}
void sub_0024A150(void){g_eax=0;g_esp+=4;}
int pc_model_transform_entry(void){return 0;}
void sub_000D6A80(void){g_eax=0;g_esp+=4;}
void sub_000B93E0(void){g_eax=1;g_esp+=4;}
void sub_0011E7B0(void){g_esp+=4;}
void sub_000DC5D0(void){g_esp+=4;}
static double damage;
void sub_000BB6C0(void){damage+=MEMF(MEM32(g_esp+4)+0x20);g_esp+=4;}
/* SERVICES */
/* CODE */
static double run(unsigned fps,int native,unsigned kind,unsigned target,float slow){
 memset((void*)(g_xbox_mem_offset+SHOT),0,0x22c);
 MEM32(SHOT)=kind;MEM32(SHOT+8)=kind==26?18:24;MEM32(0x2000+0x20)=target;
 MEM32(0x2000+4)=0x3000;MEM32(0x3000+0x8c)=0x4000;MEM32(0x4000+0x5c)=0x5000;
 MEMF(SHOT+0x64)=1;MEMF(SHOT+0x98)=.1f;
 random_calls=sweeps=0;random_state=12345;damage=0;
 g_tsfp_native_timing_active=native;
 unsigned prev=0;
 for(unsigned i=1;i<=(unsigned)(fps/slow);i++){
  unsigned ticks=(unsigned)(i*60.0/fps*slow+1e-6);
  MEM32(0x4e7944)=ticks-prev;prev=ticks;MEMF(0x4e794c)=60.f/fps*slow;
  g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=SHOT;
  if(kind==26)sub_000EBB00();else sub_000EC120();
  assert(g_esp==STACK+4 && g_fp_top==0);
 }
 assert(sweeps==60 && random_calls==240);
 double saved=damage;unsigned saved_rng=random_calls;
 MEM32(0x4e7944)=0;MEMF(0x4e794c)=0;
 g_esp=STACK;MEM32(STACK+4)=SHOT;
 if(kind==26)sub_000EBB00();else sub_000EC120();
 assert(damage==saved && saved_rng==random_calls && sweeps==60);
 printf("kind=%u target=%u fps=%u slow=%g native=%d damage=%.8f\n",kind,target,fps,slow,native,damage);
 return damage;
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);
 g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
 /* CONSTANTS */
 MEM32(0x4e8cec)=60;
 for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+i*4)=(float)sin(i*6.2831853071795864769/2048);
 for(unsigned kind=25;kind<=26;kind++)for(unsigned t=0;t<2;t++){
 printf("kind=%u target=%u\n",kind,t?8:2);
 double base=run(60,0,kind,t?8:2,1);
 assert(base>0);
 unsigned rates[]={60,120,144,180,240};for(unsigned r=0;r<5;r++)for(unsigned slow=0;slow<(argc==1?2u:1u);slow++){
 double actual=run(rates[r],argc==1,kind,t?8:2,slow?.25f:1.f);
 if(fabs(actual-base)>.001){fprintf(stderr,"FAIL: beam damage depends on frame rate\n");return 1;}
 }
 }puts("PASS: original beam damage, cadence and pause");return 0;
}
