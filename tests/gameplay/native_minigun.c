/* Original minigun motor/readiness with controlled audio/model boundaries. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];RECOMP_TLS int g_fp_top,g_fp_cmp;
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"unexpected minigun tail")
/* MINIGUN_POLICY */
enum {PLAYER=0x1000,HAND=PLAYER+0x184,STATE=0x2000,ENTITY=0x3000,STACK=0x8000};
void sub_001B8530(void){uint32_t out=MEM32(g_esp+8);MEMF(out)=MEMF(out+4)=MEMF(out+8)=0;g_eax=0;g_esp+=4;}
void sub_000468A0(void){g_eax=0xffffffff;g_esp+=4;}
/* UNREACHED */
/* GENERATED_ROUTINES */
static unsigned call(unsigned event,int fixed)
{
 g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=event;MEM32(STACK+8)=HAND;MEM32(STACK+12)=STATE;MEM32(STACK+16)=0;
 PcMinigunIntent scope={0};if(fixed)scope=pc_native_minigun_prepare(STACK,g_xbox_mem_offset);
 sub_001C74D0();unsigned result=g_eax;pc_native_minigun_restore(scope);
 assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);return result;
}
int main(void)
{
 #ifdef _WIN32
 _set_abort_behavior(0,_CALL_REPORTFAULT);
 #endif
 setvbuf(stdout,NULL,_IONBF,0);
 unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
 /* CONSTANTS */
 MEM32(0x7b0c7c)=PLAYER;MEM32(0x4e8cec)=60;MEM32(PLAYER+0x18)=1;
 MEM32(HAND+0x30)=MEM32(STATE+0x30)=7;MEM32(HAND+0x3cc)=STATE;MEM32(HAND+0x44)=1;
 MEM32(STATE+0xe8)=ENTITY;MEM32(ENTITY+0x22c)=0xffffffff;
 /* Scoped input: remapping, inhibited fire, both pad routes and local guards. */
 g_esp=STACK;MEM32(STACK+4)=1;MEM32(STACK+8)=HAND;MEM32(STACK+12)=STATE;
 MEMF(0x4e794c)=.5f;g_tsfp_native_timing_active=1;MEM32(HAND+0x64)=23;
 MEM32(PLAYER+0x18)=8;MEM32(0x6b93dc)=8;
 PcMinigunIntent scope=pc_native_minigun_prepare(STACK,g_xbox_mem_offset);
 assert(scope.hand==HAND && MEM32(HAND+0x64)==1);pc_native_minigun_restore(scope);assert(MEM32(HAND+0x64)==23);
 MEM32(0x6b9408)=8;scope=pc_native_minigun_prepare(STACK,g_xbox_mem_offset);
 assert(scope.hand==HAND && MEM32(HAND+0x64)==0);pc_native_minigun_restore(scope);MEM32(0x6b9408)=0;
 MEM32(0x6b9888)=1;scope=pc_native_minigun_prepare(STACK,g_xbox_mem_offset);
 assert(scope.hand==HAND && MEM32(HAND+0x64)==0);pc_native_minigun_restore(scope);MEM32(0x6b9888)=0;
 MEM32(0x76b0fc)=1;MEM32(0x6b94d4)=8;scope=pc_native_minigun_prepare(STACK,g_xbox_mem_offset);
 assert(scope.hand==HAND && MEM32(HAND+0x64)==1);pc_native_minigun_restore(scope);MEM32(0x76b0fc)=0;
 MEM32(PLAYER+4)=1;assert(!pc_native_minigun_prepare(STACK,g_xbox_mem_offset).hand);MEM32(PLAYER+4)=0;
 MEM32(HAND+0x30)=6;assert(!pc_native_minigun_prepare(STACK,g_xbox_mem_offset).hand);MEM32(HAND+0x30)=7;
 MEM32(HAND+0x3cc)=0;assert(!pc_native_minigun_prepare(STACK,g_xbox_mem_offset).hand);MEM32(HAND+0x3cc)=STATE;
 g_tsfp_native_timing_active=0;assert(!pc_native_minigun_prepare(STACK,g_xbox_mem_offset).hand);
 MEM32(PLAYER+0x18)=1;MEM32(HAND+0x64)=0;
 const unsigned rates[]={60,120,144,180,240};
 for(int fixed=0;fixed<=1;fixed++)for(unsigned rate=0;rate<5;rate++) {
  unsigned fps=rates[rate],ready=0;float first=-1;
  MEMF(HAND+0xdc)=MEMF(HAND+0xe0)=MEMF(STATE+0x108)=0;MEM32(HAND+0x64)=0;
  g_tsfp_native_timing_active=fixed;
  for(unsigned n=0;n<fps*3;n++) {
   unsigned whole=(n+1)*60/fps-n*60/fps;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=60.f/fps;
   MEM32(0x6b93dc)=1;call(1,fixed);MEM32(HAND+0x64)=0;
   if(whole && call(10,0)==0){ready++;if(first<0)first=(n+1.f)/fps;}
  }
  printf("minigun fps=%u fixed=%d speed=%f first=%f ready=%u\n",fps,fixed,MEMF(HAND+0xdc),first,ready);
  if(fixed || fps==60){assert(MEMF(HAND+0xdc)>=4 && ready>0 && first<.6f);}
  else assert(MEMF(HAND+0xdc)<4 && ready==0); /* actual old failure */
  if(fixed) {
   for(unsigned n=0;n<fps*2;n++){
    MEM32(0x6b93dc)=0;MEM32(HAND+0x64)=0;MEMF(0x4e794c)=60.f/fps;call(1,1);
   }
   assert(fabsf(MEMF(HAND+0xdc)-10*powf(.97f,120))<.002f);
   float paused=MEMF(HAND+0xdc);MEMF(0x4e794c)=0;call(1,1);assert(MEMF(HAND+0xdc)==paused);
  }
 }
 puts("PASS: original minigun spin/readiness, old failure, release and pause");free(memory);return 0;
}
