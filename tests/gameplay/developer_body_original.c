/* Execute the supported title's view inclusion and alternate-header branches. */
#include <assert.h>
#include <stdio.h>
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
void sub_000389B0(void){g_esp+=4;}
/* ORIGINAL_VISIBILITY_BRANCHES */
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x7b0c48)=0x70000;MEM32(0x70004)=0;MEM32(0x70008)=0x52000;
    MEM32(0x52214)=0;MEM32(0x5000c)=0;g_eax=0;g_esi=0x50000;g_esp=0x30000;
    sub_000A02F9();assert(MEM32(0x5000c)==1); /* Own-view bit INCLUDES the body. */
    MEM32(0x7b0cc0)=0x52000;MEM32(0x51000)=0x50000;MEM32(0x50020)=8;
    MEM32(0x51004)=0x60000;MEM32(0x51008)=0x61000;MEM32(0x51014)=3;MEM32(0x500a0)=1;
    g_ebp=0x51000;g_esp=0x30000;sub_000A1BF1();assert(MEM32(0x51004)==0x61000);
    MEM32(0x51004)=0x60000;MEM32(0x500a0)=0;
    g_ebp=0x51000;g_esp=0x30000;sub_000A1BF1();assert(MEM32(0x51004)==0x60000);
    MEM32(0x51004)=0x60000;MEM32(0x500a0)=1;MEM32(0x52214)=1;
    g_ebp=0x51000;g_esp=0x30000;sub_000A1BF1();assert(MEM32(0x51004)==0x60000); /* Other view unaffected. */
    free(ram);puts("PASS: native inclusion bit and per-view alternate model header selection");return 0;
}
