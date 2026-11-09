/* Original boss mine pool and enclosing update. World/render/audio services
 * are controlled; fixed-step arming and state transitions execute unmodified. */
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


enum {BOSS=0x10000,POOL=BOSS+0x11a0,OBJ=0x20000,STACK=0x30000};
static int legacy,nearby;static unsigned events,armed,exploded,burst;
#define pc_native_skip_boss_mines() (!legacy && (pc_native_skip_boss_mines)())
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) do{fprintf(stderr,"UNEXPECTED tail %x\n",(unsigned)(target));abort();}while(0)
void sub_00059DD0(void){g_eax=0;g_esp+=4;}
void sub_000468A0(void){g_esp+=4;}
void sub_0005F460(void){++events;g_esp+=4;}
void sub_00058670(void){g_esp+=4;}
void sub_002A8920(void){++burst;g_esp+=4;}
void sub_000217B0(void){++events;g_esp+=4;}
void sub_000EFBD0(void){++events;g_esp+=4;}
void sub_000E0080(void){g_esp+=4;}
void sub_000A9070(void){g_esp+=4;}
void sub_002AA330(void){g_esp+=4;}
/* SERVICES */
/* CODE */
static void setup(int native){
 memset((void*)(g_xbox_mem_offset+BOSS),0,0x20000);
 MEM32(0x75f5e8)=BOSS;MEM32(0x790950)=0;
 MEM32(POOL+0xf8)=2;MEMF(BOSS+0x1cfc)=nearby?0.f:10000.f;
 for(unsigned i=0;i<4;i++){
  MEM32(POOL+0x150+4*i)=OBJ+i*0x300;
  MEM32(OBJ+i*0x300+4)=OBJ+i*0x300+0x100;
  MEMF(POOL+i*12)=10.f+i;
 }
 g_tsfp_native_timing_active=native;events=armed=exploded=burst=0;
}
static void step(unsigned whole,float delta){
 MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
 g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=0;
 g_ebp=0x1111;g_ebx=0x2222;g_esi=0x3333;g_edi=0x4444;
 sub_0028F560();
 assert(g_esp==STACK+4 && g_ebp==0x1111 && g_ebx==0x2222 && g_esi==0x3333 && g_edi==0x4444);
 assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static unsigned run(unsigned fps,unsigned slow,int native){
 setup(native);unsigned previous=0;
 for(unsigned frame=1;frame<fps*slow*180;frame++){
  unsigned tick=frame*60/(fps*slow),whole=tick-previous;previous=tick;
  step(whole,60.f/(fps*slow));
  if(!armed && MEM32(POOL+0xf8)==3)armed=tick;
  if(!exploded && MEM32(POOL+0xf8)==5)exploded=tick;
  if(MEM32(POOL+0xf8)==0){
   printf("mine fps=%u slow=%u armed=%u exploded=%u retired=%u events=%u\n",fps,slow,armed,exploded,tick,events);
   assert(armed && exploded && (nearby || burst) && burst%4==0);return tick;
  }
 }
 fprintf(stderr,"state=%u y=%f vel=%f accel=%f cycle=%u arm=%u exp=%u burst=%u\n",MEM32(POOL+0xf8),MEMF(POOL+4),MEMF(POOL+0xd0),MEMF(POOL+0xcc),MEM32(POOL+0x14c),armed,exploded,burst);assert(0);return 0;
}
int main(int argc,char **argv){
 legacy=argc>1;g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
 /* CONSTANTS */
 for(nearby=0;nearby<2;nearby++){
 unsigned baseline=run(60,1,0),baseline_events=events,base_arm=armed,base_exp=exploded,base_burst=burst;
 const unsigned rates[]={60,120,144,180,240};
 for(unsigned slow=1;slow<=4;slow*=4)for(unsigned i=0;i<5;i++){
  unsigned got=run(rates[i],slow,1);
  if(got!=baseline || events!=baseline_events || armed!=base_arm || exploded!=base_exp || burst!=base_burst){fprintf(stderr,"FAIL: boss mine arming depends on frame rate\n");return 1;}
 }
 }
 setup(1);step(0,0);step(1,0);assert(MEMF(POOL+0xc0)==0);
 puts("PASS: original boss mine arming cadence");return 0;
}
