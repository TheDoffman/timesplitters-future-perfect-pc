/* Verify the original Story position reset against isolated guest memory. */
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


void sub_0009BCF0(void){g_eax=0x1234;g_esp+=4;}
void sub_000C3C50(void){g_eax=7;g_esp+=4;}
/* ORIGINAL_POSITION_RESET */
int main(void){
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* ORIGINAL_CONSTANTS */
    const uint32_t context=0x60000,body=0x70000,actor=0x71000;
    MEM32(0x79094c)=2;MEM32(context+0x14)=body;MEM32(body+0x7c)=actor;
    MEMF(context+0xa8)=-82.58f;MEMF(context+0xac)=-12.38f;MEMF(context+0xb0)=-126.59f;
    MEMF(context+0x90)=1.6f;MEMF(context+0x100)=90.f;
    MEMF(context+0xf4)=MEMF(context+0xf8)=MEMF(context+0xfc)=12.f;
    g_esp=0x30000;MEM32(g_esp+4)=context;sub_000D2DE0();assert(g_esp==0x30004);
    for(unsigned axis=0;axis<3;axis++){
        assert(MEM32(body+0x64+axis*4)==MEM32(context+0xa8+axis*4));
        assert(MEM32(body+0xec+axis*4)==MEM32(context+0xa8+axis*4));
        assert(MEM32(context+0x1010+axis*4)==MEM32(context+0xa8+axis*4));
        assert(MEMF(context+0xf4+axis*4)==0.f);
    }
    assert(MEMF(body+0x70)==90.f && MEM32(body+0x34)==7);
    assert(MEM32(context+0xf54)==7 && MEM32(context+0xf58)==7 && MEM32(actor+0x678)==0x1234);
    free(ram);puts("PASS: original Story reposition updates body, prior position, camera coordinates, room and zero velocity");return 0;
}
