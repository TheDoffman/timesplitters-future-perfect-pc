/* Original US98E50: real-clock versus fractional native movie advancement. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
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
void sub_00060210(void){g_esp+=4;}
void sub_00153AF0(void){g_eax=0;g_esp+=4;}
/* ORIGINAL_CUTSCENE_CLOCK */
static void setf(uint32_t address,float value){memcpy((void *)(g_xbox_mem_offset+address),&value,4);}
static float getf(uint32_t address){float value;memcpy(&value,(void *)(g_xbox_mem_offset+address),4);return value;}
static void step(float delta){g_esp=0x30000;MEM32(g_esp+4)=0x50000;setf(0x4e794c,delta);sub_00098E50();assert(g_esp==0x30004);}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x70e164)=0x50000;MEM32(0x50000)=10000;
    MEM32(0x4e8cec)=60;MEM32(0x7497a8)=600;
    setf(0x4d1994,60.f);setf(0x475c78,1.f);setf(0x4760d0,0.f);
    setf(0x50008,2.f);step(2.f);assert(getf(0x50008)==10.f);
    MEM32(0x6d60f0)=1;setf(0x50008,2.f);step(.5f);
    float half=getf(0x50008)-2.f;step(2.f);
    float twice=getf(0x50008)-(2.f+half);assert(fabsf(twice-4.f*half)<.000001f);
    step(0.f);assert(fabsf(getf(0x50008)-(2.f+half+twice))<.000001f);
    free(ram);puts("PASS: original cutscene fractional clock honors speed and pause; wall-clock branch ignores game speed");return 0;
}
