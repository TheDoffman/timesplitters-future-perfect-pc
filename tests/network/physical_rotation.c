/* Captured overlapping rooms5/27 through the original render candidate builder. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
enum {POOL=0x20000,RECORD=POOL+512,MATRIX=0x30000,STACK=0x18000};
static float *matrix(uint32_t a){return (float *)((uintptr_t)a+g_xbox_mem_offset);}
void sub_001531D0(void){memcpy(matrix(MEM32(g_esp+4)),matrix(MEM32(g_esp+8)),64);g_esp+=4;}
void sub_00153240(void){float *m=matrix(MEM32(g_esp+4));memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;g_esp+=4;}
void sub_001530E0(void){float v[16],*a=matrix(MEM32(g_esp+8));for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)v[i*4+j]=a[j*4+i];memcpy(matrix(MEM32(g_esp+4)),v,64);g_esp+=4;}
void sub_00153270(void){float v[16]={0},*a=matrix(MEM32(g_esp+8)),*b=matrix(MEM32(g_esp+12));for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)for(unsigned k=0;k<4;k++)v[i*4+j]+=a[i*4+k]*b[k*4+j];memcpy(matrix(MEM32(g_esp+4)),v,64);g_esp+=4;}
static unsigned sleeps;
void sub_002572A0(void){assert(MEM32(g_esp+4)==0x40000 && !MEM32(g_esp+8));sleeps++;g_esp+=4;}
void sub_00152540(void);
/* ORIGINAL_PHYSICAL_ROTATION */
int main(void){
 void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
 /* ORIGINAL_CONSTANTS */
 MEM32(0x74c458)=POOL;
 float *inertia=matrix(RECORD+0x80);inertia[0]=11.19056f;inertia[5]=13.88609f;inertia[10]=29.26760f;inertia[15]=1;
 float before[16];memcpy(before,inertia,64);
 float *rotation=matrix(MATRIX);rotation[1]=1;rotation[4]=-1;rotation[10]=rotation[15]=1;
 g_esp=STACK;MEM32(STACK)=0;MEM32(STACK+4)=0x90001;MEM32(STACK+8)=MATRIX;MEM32(STACK+12)=0;
 sub_00165D80();assert(g_esp==STACK+4);
 assert(!memcmp(matrix(RECORD),rotation,64) && !memcmp(matrix(RECORD+0x80),before,64));
 float *q=matrix(RECORD+0x100);assert(fabsf(q[0])<.0001f && fabsf(q[1])<.0001f);
 assert(fabsf(fabsf(q[2])-.70710678f)<.0001f && fabsf(q[3]-.70710678f)<.0001f);
 float *world=matrix(RECORD+0x40);assert(fabsf(world[0]-before[5])<.0001f && fabsf(world[5]-before[0])<.0001f && world[10]==before[10]);
 MEM32(0x74c46c)=0x50000;MEM32(0x50004)=0x40000;MEM32(0x7a20c8)=RECORD;
 MEM8(RECORD+0x1c8)=1;MEM32(RECORD+0x1c0)=0x1000000;
 MEMF(RECORD+0x144)=7.f;MEMF(RECORD+0x150)=-9.f;
 g_esp=STACK;MEM32(STACK)=0;MEM32(STACK+4)=0x90001;sub_001677E0();
 assert(g_esp==STACK+4 && MEM8(RECORD+0x1c8)==255 && !MEM32(0x7a20c8) && sleeps==1);
 assert(MEMF(RECORD+0x144)==7.f && MEMF(RECORD+0x150)==-9.f && !memcmp(matrix(RECORD),rotation,64));
 g_esp=STACK;sub_001677E0();assert(sleeps==1); /* Already paused, pose preserved. */
 free(ram);puts("PASS: original rigid orientation setter refreshes quaternion/world inertia while retaining captured local inertia");return 0;
}
