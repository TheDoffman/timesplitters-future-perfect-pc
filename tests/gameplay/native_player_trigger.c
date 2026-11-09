/* Original fresh/held eligibility and request routines, with the real host
 * pulse buffer and Xbox edge decoder. Device input and the final weapon-state
 * transition are controlled: this counts accepted requests, not projectiles. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "../src/gameplay/pc_native_timing.h"
#include "../src/input/pc_reload_input.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active=1;
#include "../src/gameplay/pc_native_weapon_guest.h"
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { P=0x1000,L=0x3000,A=0x4000,S=0x6000,W=0x8000,T=0xa000,STACK=0x10000 };
static unsigned shots;
static int legacy;
static unsigned buttons;
void sub_0046E36D(void){
 uint32_t out=MEM32(g_esp+8);MEM32(out)=1;MEM16(out+4)=0;
 for(unsigned i=0;i<8;i++)MEM8(out+6+i)=0;
 MEM8(out+8)=buttons?255:0;
 for(unsigned i=0;i<4;i++)MEM16(out+14+i*2)=0;
 g_eax=0;g_esp+=12;
}
void sub_0046E3E0(void){g_eax=0;g_esp+=12;}
void sub_001C2AF0(void);
void sub_000427B0(void){g_eax=2;g_esp+=4;}
void sub_00042760(void){g_eax=1;g_esp+=4;}
void sub_001B5880(void){g_eax=0;g_esp+=4;}
void sub_00142990(void){g_esp+=4;}
void sub_001B6D10(void){assert(!"reload");}
void sub_001C0FA0(void){assert(!"mode");}
void sub_001C2090(void){shots++;MEM32(W+0xf0)=0;g_esp+=4;}
void sub_00012F70(void){g_eax=0;g_esp+=4;}
/* CODE */
void sub_001C2AF0(void){
 unsigned saved=MEM32(0x4e7944);
 float delta=MEMF(0x4e794c);
 if(legacy)sub_001C2AF0_gen();
 else pc_native_player_trigger_guest(sub_001C2AF0_gen,g_esp);
 assert(MEM32(0x4e7944)==saved && MEMF(0x4e794c)==delta);
}
int main(int argc,char **argv){
 legacy=argc>1;
 g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x800000);
 MEM32(0x7b0c7c)=P;MEM32(P+0x14)=L;MEM32(L+0x7c)=A;
 MEM32(S+0x3cc)=W;MEM32(S+0x44)=1;MEM32(0x4f9bac)=T;
 MEM32(0x4fa134)=-1;MEM32(0x4e8cec)=60;MEMF(0x475c78)=1;
 MEMF(T+0x78)=.083f;MEMF(T+0x7c)=.4f;
 /* CONSTANTS */
 MEM32(0x74d0fc)=1;MEM32(0x74d100)=0x100;
 for(unsigned axis=0;axis<4;axis++)MEM16(0x74d098+0xd+axis*2)=65535;
 MEM8(0x74d098+0xb)=MEM8(0x74d098+0xc)=255;
 MEM16(0x74d098+0x15)=65535;MEM16(0x74d098+0x17)=32767;
 for(unsigned fps=60;fps<=240 && !(argc>1 && strcmp(argv[1],"legacy-host")==0);fps+=60)for(unsigned phase=0;phase<fps/60;phase++){
  g_tsfp_native_timing_active=fps!=60;
  shots=0;MEM32(W+0xf0)=100;MEM32(S+0x3c)=0;
  for(unsigned frame=0;frame<fps*2;frame++){
   unsigned tick=(frame+1)*60/fps-frame*60/fps;
   MEM32(0x4e7944)=tick;MEMF(0x4e794c)=60.f/fps;
   MEM32(W+0xf0)+=tick;
   unsigned press=frame%(fps/10)==phase;
   unsigned local=frame%(fps/10);
   MEM32(P+0x18)=press?3:(local>phase && local<phase+(fps*34+999)/1000 ? 1:0);
   g_esi=S;g_esp=STACK;MEM32(STACK)=0;MEM32(STACK+4)=1;
   sub_001C2C00();assert(g_esp==STACK+4);
  }
  assert(shots==20 && "fresh trigger phase lost");
 }
 unsigned cases=0;
 for(unsigned fps=60;fps<=240;fps+=60)for(unsigned pause=0;pause<fps/60;pause++)
 for(unsigned width=1;width<=20;width+=19)for(unsigned gap=84;gap<=100;gap+=16)
 for(unsigned phase=0;phase<17;phase++){
  g_tsfp_native_timing_active=fps!=60;
  PcNativeTiming clock={0};pc_native_timing_step(&clock,0,1000,fps,0);
  PcReloadInput host={0};unsigned char roles[256]={0};roles[1]=2;
  unsigned frame=1,down=0,previous=0;shots=0;MEM32(W+0xf0)=100;MEM32(S+0x3c)=0;
  memset((void*)(g_xbox_mem_offset+0x6b93d0),0,0x3c);
  for(unsigned ms=1;ms<2500;ms++){
   down=ms>=100+phase && ms<100+phase+20*gap && (ms-100-phase)%gap<width;
   pc_reload_event(&host,1,previous,down,ms);previous=down;
   if(ms*fps<frame*1000)continue;
   PcNativeStep step=pc_native_timing_step(&clock,ms,1000,fps,frame<=pause);
   pc_reload_context(&host,P,1,0,ms);
   int fire=down?255:0,reload=0,secondary=0;
   pc_reload_sample(&host,roles,1,ms,&reload,&fire,&secondary);buttons=fire!=0;
   MEM32(0x4e7940)=step.raw_ticks;g_esp=STACK;MEM32(STACK+4)=0x6b93d0;MEM32(STACK+8)=0;
   pc_native_input_guest(sub_0018FF70);
   MEM32(P+0x18)=((MEM32(0x6b93dc)&16)?1:0)|((MEM32(0x6b93e0)&16)?2:0);
   MEM32(0x4e7944)=step.game_ticks;MEMF(0x4e794c)=step.game_delta;MEM32(W+0xf0)+=step.game_ticks;
   g_esi=S;g_esp=STACK;MEM32(STACK+4)=1;sub_001C2C00();
   frame++;
  }
  if(shots!=20)fprintf(stderr,"fps=%u pause=%u width=%u gap=%u phase=%u shots=%u\n",fps,pause,width,gap,phase,shots);
  assert(shots==20 && "buffered eligible clicks lost");cases++;
 }
 /* A fresh click inside the original cooldown must still be refused. */
 g_tsfp_native_timing_active=1;MEM32(W+0xf0)=3;MEM32(P+0x18)=3;
 MEM32(0x4e7944)=0;MEMF(0x4e794c)=.25f;
 unsigned before=shots;g_esi=S;g_esp=STACK;MEM32(STACK+4)=1;sub_001C2C00();
 assert(shots==before);
 /* Held input keeps its separate 24-tick cooldown. */
 MEM32(W+0xf0)=5;MEM32(P+0x18)=1;
 g_esi=S;g_esp=STACK;MEM32(STACK+4)=1;sub_001C2C00();assert(shots==before);
 printf("PASS: %u host tap schedules, original cooldowns and restored clocks\n",cases);
 free((void*)(uintptr_t)g_xbox_mem_offset);
}
