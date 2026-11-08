/* Original terminal dispatcher, text and password callbacks. Rendering and
 * state-transition services are controlled; typing timing is not reimplemented. */
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

enum {PLAYER=0x1000,T=0x2000,CONFIG=0x3000,CHILD=0x4000,ENTRY=0x5000,
      LINES=0x6000,STRING=0x7000,DESC=0x8000,STACK=0x20000};
static unsigned random_calls,callbacks,transition;static int legacy,ugenix;
void sub_0036D000(void),sub_0036D3C0(void),sub_00372190(void),sub_00376640(void);
/* Keep the production call-site selection; ablate only the new wrappers. */
static void (*select_call(uint32_t site,void (*fn)(void)))(void) {
 if(legacy && (site==0x370424 || site==0x3721ee || site==0x3766b0))return fn;
 if(site==0x370424)return RECOMP_DIRECT_CALLEE_AT(0x370424,sub_0036D000);
 if(site==0x3721ee)return RECOMP_DIRECT_CALLEE_AT(0x3721ee,sub_0036D3C0);
 if(site==0x3766b0)return RECOMP_DIRECT_CALLEE_AT(0x3766b0,sub_0036D3C0);
 return fn;
}
#undef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site,fn) select_call(site,fn)
static void callback(uint32_t target) {++callbacks;if(target==0x376640)sub_00376640();else{assert(target==0x372190);sub_00372190();}}
void sub_0036E440(void){g_esp+=4;}
void sub_00370530(void){transition=MEM32(g_esp+8);g_esp+=4;}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) callback(target)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) do{fprintf(stderr,"UNEXPECTED tail %x\n",(unsigned)(target));abort();}while(0)
void sub_0003D7B0(void){g_eax=STRING;g_esp+=4;}
void sub_00046570(void){g_esp+=4;}
void sub_0036CEB0(void){g_eax=1;g_esp+=4;}
void sub_0036D380(void){g_esp+=4;}
void sub_00153B40(void){++random_calls;g_eax=0x80000000;g_esp+=4;}
void sub_0036C8B0(void){g_eax=0;g_esp+=4;}
void sub_00045390(void){g_eax=0;g_esp+=4;}
void sub_0036FD30(void){g_esp+=4;}
void sub_0036D5A0(void){g_esp+=4;}
void sub_0036DA80(void){g_esp+=4;}
void sub_0036DC00(void){g_esp+=4;}
void sub_0036DDA0(void){g_esp+=4;}
/* SERVICES */
/* CODE */
static void setup(int native){
 memset((void*)(g_xbox_mem_offset+PLAYER),0,0x9000);
 MEM32(PLAYER+0x7c)=T;MEM32(T+0x30)=ENTRY;
 MEM32(0x76c274)=CONFIG;MEM32(CONFIG)=DESC;MEM32(CONFIG+0xc)=CHILD;
 MEM32(CONFIG+0x10)=1;MEM32(CHILD+0x14)=2;MEM32(CHILD+0x28)=ugenix?0x376640:0x372190;
 MEM32(T+0x10)=0x9000;MEM32(0x9000)=ugenix==2?0x544f5332:0x544f5353;
 MEM32(T+0x154)=1;MEM32(T+0x12c)=0;
 MEM32(T+0x24)=LINES;MEM32(LINES)=1;MEM32(LINES+4)=1;
 MEM32(T+0x40)=0xffffffff;MEM32(0x76c948)=2;MEM32(0x76d088)=0;
 strcpy((char*)(g_xbox_mem_offset+STRING),"Password accepted");
 g_tsfp_native_timing_active=native;random_calls=callbacks=transition=0;
}
static void step(unsigned whole,float delta){
 MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
 g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=PLAYER;
 g_ebp=0x1111;g_ebx=0x2222;g_esi=0x3333;g_edi=0x4444;
 sub_00370490();
 if(g_esp!=STACK+4 || g_ebp!=0x1111 || g_ebx!=0x2222 || g_esi!=0x3333 || g_edi!=0x4444)fprintf(stderr,"ABI esp=%x ebp=%x ebx=%x esi=%x edi=%x\n",g_esp,g_ebp,g_ebx,g_esi,g_edi);
 assert(g_esp==STACK+4 && g_ebp==0x1111 && g_ebx==0x2222 && g_esi==0x3333 && g_edi==0x4444);
 assert(MEMF(0x4e794c)==delta && MEM32(0x4e7944)==whole);
}
static unsigned run(unsigned fps,unsigned slow,int native){
 setup(native);unsigned previous=0,done=0;
 for(unsigned frame=1;frame<fps*slow*30;frame++){
  unsigned tick=frame*60/(fps*slow),whole=tick-previous;previous=tick;
  step(whole,60.f/(fps*slow));
  if(ugenix?transition!=0:MEM32(0x76d088)!=0){assert(!ugenix || transition==(ugenix==2?6u:4u));done=tick;break;}
 }
 printf("terminal fps=%u slow=%u native=%d complete_tick=%u callbacks=%u rng=%u\n",fps,slow,native,done,callbacks,random_calls);
 assert(done);return done;
}
int main(int argc,char **argv){
 legacy=argc>1;g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)calloc(1,0x900000);assert(g_xbox_mem_offset);
 /* CONSTANTS */
 const unsigned rates[]={60,120,144,180,240};
 for(ugenix=0;ugenix<3;ugenix++){
 unsigned baseline=run(60,1,0),rng=random_calls;
 for(unsigned slow=1;slow<=4;slow*=4)for(unsigned i=0;i<5;i++){
  unsigned got=run(rates[i],slow,1);
  if(abs((int)got-(int)baseline)>1 || random_calls!=rng){fprintf(stderr,"FAIL: terminal entry depends on frame rate\n");return 1;}
 }
 }
 setup(1);step(0,0);assert(!callbacks && !random_calls);
 setup(1);step(1,0);assert(MEMF(T+8)==0 && !random_calls);
 puts("PASS: original terminal text and password timing");return 0;
}
