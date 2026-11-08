/* Original Story/Arcade dart detachment followed by the original falling
 * update. World queries, allocation, sound and effects are controlled. */
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
/* PROJECTILE_POLICY */
enum { SHOT=0x1000, STACK=0x30000 };
static unsigned random_state,random_calls,smoke,sweeps,retired;
void sub_00153B40(void) {
    random_state=random_state*1664525u+1013904223u;
    g_eax=random_state;random_calls++;g_esp+=4;
}
void sub_00153BD0(void) {
    sub_00153B40();g_fp_top=(g_fp_top-1)&7;
    g_fp_stack[g_fp_top]=g_eax/4294967296.0;
}
void pc_native_kind35_rng(void) {
    pc_native_kind35_rng_guest(sub_00153B40,MEM32(g_esp),&g_eax,&g_esp);
}
void pc_native_projectile_smoke_rng(void) {
    if(!pc_native_projectile_skip_smoke()){sub_00153BD0();return;}
    g_fp_top=(g_fp_top-1)&7;g_fp_stack[g_fp_top]=.5;g_esp+=4;
}
void sub_000F85C0(void){smoke++;g_esp+=4;}
void pc_native_projectile_smoke(void) {
    if(!pc_native_projectile_skip_smoke()){sub_000F85C0();return;}
    g_esp+=4;
}
void sub_000DCB80(void){assert(g_esi==SHOT);sweeps++;g_eax=0;g_esp+=4;}
void sub_000DC550(void){retired++;g_esp+=4;}
void sub_000EDBE0(void){g_esp+=4;}
void sub_000E97A0(void){sweeps++;g_eax=0;g_esp+=4;}
void sub_000913D0(void){g_eax=0;g_esp+=4;}
void sub_000E9FE0(void){g_esp+=4;}
void sub_000F4920(void){g_esp+=4;}
void sub_000E94E0(void){g_esp+=4;}
/* Visibility query and particle allocator are scene services. */
void sub_001CDAE0(void){g_eax=1;g_esp+=4;}
void sub_000EFBD0(void){g_eax=0;g_esp+=4;}
void sub_000E33A0(void){g_esp+=4;}
void sub_000FFF70(void){g_eax=0;g_esp+=4;}
void sub_000FF420(void){g_esp+=4;}
void sub_000FF490(void){g_esp+=4;}
void sub_00153BB0(void){sub_00153B40();}
void sub_000F04B0(void){g_esp+=4;} /* Attached effect update. */
void sub_00045610(void){g_eax=0;g_esp+=4;} /* Sound handle query. */
void sub_00045460(void){g_esp+=4;}
void sub_0024A150(void){g_eax=0;g_esp+=4;} /* No scripted beam obstacle. */
void sub_00186480(void){g_esp+=4;} /* Ignition scene service. */
int pc_model_transform_entry(void){return 0;} /* Execute original matrix code. */
void sub_00101660(void){g_eax=0;g_esp+=4;} /* Particle allocation. */
void sub_0010E730(void){g_esp+=4;} /* Particle position service. */
void pc_native_projectile_smoke_choice(void){
    if(!pc_native_projectile_skip_smoke()){sub_00153BB0();return;}
    g_eax=1;g_esp+=4;
}
void sub_003C848C(void){g_eax=(int32_t)g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;g_esp+=4;}
void sub_003C8501(void){g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=floor(MEMD(g_esp+4));g_esp+=4;}
void sub_0003ED20(void){memset((void*)(g_xbox_mem_offset+0x5000),0,0x188);g_eax=0x5000;g_esp+=4;}
void sub_000873F0(void){
    memset((void*)(g_xbox_mem_offset+MEM32(g_esp+8)),0,12);
    memset((void*)(g_xbox_mem_offset+MEM32(g_esp+12)),0,12);g_esp+=4;
} /* Controlled empty scene. */
void sub_000C3E60(void){g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=0;g_esp+=4;}
void sub_00064D10(void){g_eax=1;g_esp+=4;}
void sub_00088C60(void){g_eax=1;g_esp+=4;}
static int legacy;
static float release_product(uint32_t site,float a,float b) {
 if(legacy && (site==0xe036b || site==0xe04e7))return a*b;
 if(site==0xe036b)return RECOMP_F32_MUL_AT(0xE036B,a,b);
 return RECOMP_F32_MUL_AT(0xE04E7,a,b);
}
#undef RECOMP_F32_MUL_AT
#define RECOMP_F32_MUL_AT(site,a,b) \
 (((site)==0xe036b || (site)==0xe04e7)?release_product(site,a,b):pc_native_multiply(site,a,b))
/* UNREACHED_SERVICES */
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected projectile tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected projectile callback")
/* GENERATED_ROUTINES */
static void call(void (*f)(void)) {
    g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=SHOT;
    g_esi=0xabc;g_edi=0xdef;g_ebx=0x123;g_ebp=0x456;
    f();assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xabc && g_edi==0xdef && g_ebx==0x123 && g_ebp==0x456);
}
static void call_update(void){pc_native_kind35_update_guest(UPDATER,MEM32(g_esp+4));}
static void noop(void){}

static float run(unsigned kind,unsigned fps,int native,unsigned mode,float slow) {
 memset((void*)(g_xbox_mem_offset+SHOT),0,0x22c);
 MEM32(SHOT)=kind;MEM32(SHOT+0x15c)=UINT32_MAX;
 MEM32(SHOT+0xa8)=0x2000;MEM32(0x207c)=0x4000;
 MEM32(SHOT+0x158)=0x6000;MEM32(0x6020)=8;MEM32(0x6028)=0x10000;
 MEMF(0x6068)=0;MEMF(SHOT+0x20)=2;
 MEM32(0x737e44)=MEM32(0x736680)=0;MEM8(0x7de455)=mode;
 MEM32(0x4e7944)=1;MEMF(0x4e794c)=native?60.f/fps*slow:1.f;
 g_tsfp_native_timing_active=native;
 g_esp=STACK;MEM32(STACK)=0xdeadbeef;MEM32(STACK+4)=0x6000;
 sub_000E0520();assert(g_esp==STACK+4 && !MEM32(SHOT+0x158));
 printf("release fps=%u native=%d mode=%u slow=%.2f velocity=%.9f\n",fps,native,mode,slow,MEMF(SHOT+0x68));
 if(fabsf(MEMF(SHOT+0x68)+.5f/60)>=.0000001f){fputs("FAIL: dart release depends on frame rate\n",stderr);exit(1);}
 assert(!(MEM32(0x6028)&0x10000));
 pc_native_kind35_init_guest(noop,SHOT);
 unsigned prev=0;double ticks=0;
 for(unsigned i=0;i<(unsigned)(fps*.5/slow);i++) {
  ticks+=60.0/fps*slow;unsigned whole=(unsigned)(ticks+1e-6)-prev;prev+=whole;
  MEM32(0x4e7944)=whole;MEMF(0x4e794c)=60.f/fps*slow;
  call(call_update);
 }
 return 2-MEMF(SHOT+0x20);
}
int main(int argc,char **argv) {
 (void)argv;legacy=argc>1;
 setbuf(stdout,NULL);void *memory=calloc(1,0x900000);assert(memory);
 g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
 /* ORIGINAL_CONSTANTS */
 MEM32(0x737bd4)=SHOT;MEM32(0x4e8cec)=60;
 for(unsigned i=0;i<2048;i++)MEMF(0x7a38a0+i*4)=(float)sin(i*6.2831853071795864769/2048);
 const unsigned rates[]={60,120,144,180,240};
 for(unsigned kind=32;kind<=33;kind++)for(unsigned mode=3;mode<=10;mode+=7) {
  float reference=run(kind,60,0,mode,1);
  for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4) {
   float distance=run(kind,rates[r],1,mode,1.f/slow);
   printf("fall reference=%f actual=%f\n",reference,distance);
   assert(fabsf(distance-reference)<.03f);
  }
 }
 puts("PASS: original detached dart release and falling motion");free(memory);return 0;
}
