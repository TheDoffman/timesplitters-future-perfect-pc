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
static uint32_t expected_index,expected_slot;
void sub_0006BB90(void) {
    assert(MEM32(g_esp+4)==expected_index&&MEM32(g_esp+8)==expected_slot);
    g_eax=0x60000;g_esp+=4;
}
void sub_0009CC40(void) {assert(MEM32(g_esp+4)==123);g_eax=456;g_esp+=4;}
/* ORIGINAL_CHECKPOINT_LOOKUP */
int main(void) {
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    MEM32(0x75f5d0)=0x50000;MEM32(0x60004)=123;
    for(expected_slot=0;expected_slot<2;expected_slot++)for(expected_index=0;expected_index<16;expected_index++) {
        const uint32_t bookmark=0x50000+expected_slot*0x238;
        MEM32(bookmark)=expected_index;MEM32(bookmark+4)=(1u<<(expected_index+1))-1u;
        g_esp=0x30000;MEM32(g_esp+4)=expected_slot;g_ebx=0xabcdef;
        sub_0025EC30();assert(g_eax==456&&g_esp==0x30004&&g_ebx==0xabcdef);
        MEM32(bookmark+4)=0;
        g_esp=0x30000;sub_0025EC30();assert(g_eax==0xffffffff&&g_esp==0x30004);
    }
    free(ram);puts("PASS: original respawn resolves the authored checkpoint index independently of current player position");return 0;
}
