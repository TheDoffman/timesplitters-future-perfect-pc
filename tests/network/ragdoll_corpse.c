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


static unsigned generated,bound;
void sub_003515A0(void),sub_00351570(void);
void sub_00352200(void){assert(!"must not construct an independently falling corpse");}
void sub_003513C0(void){assert(MEM32(g_esp+4)==0x20000 && MEM32(g_esp+8)==0x30000);assert(MEM32(0x30000)==235);generated++;g_esp+=4;}
void sub_0005F460(void){assert(generated==1 && MEM32(g_esp+4)==0x20000 && MEM32(g_esp+8)==235);assert(MEM32(g_esp+12)==0x3dcccccd && MEM32(g_esp+16)==0x3f800000);bound++;MEM32(0x24000)=MEM32(g_esp+8);g_esp+=4;}
/* ORIGINAL_RAGDOLL_CORPSE */
int main(void){
 void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
 uint32_t context=0x732a18+3*96;
 MEM32(0x2007c)=context;MEM32(0x30000)=230;MEM32(0x30004)=4;
 MEM32(context+0x3c)=0;MEM32(context+0x48)=0x3f800000;
 g_esp=0x18000;MEM32(g_esp)=0;MEM32(g_esp+4)=0x20000;MEM32(g_esp+8)=0x30000;MEM32(g_esp+12)=0;
 sub_000B2E50();assert(g_esp==0x18004 && generated==1 && bound==1);
 assert(MEM32(0x76a9a0)==1 && MEM32(0x76aaac)==230); /* NPC slot returned. */
 assert(MEM32(0x30000)==235 && MEM32(0x24000)==235);
 assert(!MEM32(context+0x3c) && !MEM32(context+0x48)); /* No live corpse physics. */
 MEM32(0x30000)=229;assert(MEM32(0x24000)==235); /* Subsequent actor cannot animate this corpse. */
 free(ram);puts("PASS: original ragdoll corpse bake returns temporary slot, selects corpse-owned clip and avoids live group transfer");return 0;
}
