/* Run original US25EC30 against controlled marker/room services. */
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
/* ORIGINAL_AMMO_CONSUMPTION */
static void consume(unsigned amount,unsigned source)
{
    g_esp=0x30000;MEM32(g_esp+4)=0x50000;MEM32(g_esp+8)=0x60000;
    MEM32(g_esp+12)=1;MEM32(g_esp+16)=amount;MEM32(g_esp+20)=source;
    g_esi=0xabcdef;sub_001B66E0();assert(g_esp==0x30004&&g_esi==0xabcdef);
}
int main(void)
{
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM16(0x6003a)=12;MEM16(0x50620)=30;
    consume(3,2);assert(MEM16(0x6003a)==9&&MEM32(0x6010c)==1);
    consume(0,2);assert(MEM16(0x6003a)==9&&MEM32(0x6010c)==2);
    consume(3,1);assert(MEM16(0x50620)==27&&MEM32(0x6010c)==3);
    consume(0,1);assert(MEM16(0x50620)==27&&MEM32(0x6010c)==4);
    consume(100,2);assert(MEM16(0x6003a)==0);
    free(ram);puts("PASS: original ammo consumption retains shot statistics while zero amount preserves clip/reserve");return 0;
}
