/* Complete current-recompiler constant upload; captures actual packed values. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE

#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
void sub_003D6B20(void) { assert(!"unexpected command-buffer refill"); }
/* GENERATED_UPLOAD */
int main(void) {
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x3e3f58)=0x10000;MEM32(0x10000+0x784)=0x20000;MEM32(0x20008)=0x30000;
    MEM32(0x300e4)=MEM32(0x300e8)=MEM32(0x300ec)=0xffffffff;
    const unsigned rates[]={60,120,144};
    for(unsigned r=0;r<3;r++) {
        float delta=60.f/rates[r];
        MEM32(0x10000)=0x40000;MEM32(0x10004)=0x41000;
        MEMF(0x50004)=.105f*delta;
        MEMF(0x50018)=MEMF(0x5001c)=.65f*delta;
        MEMF(0x50028)=MEMF(0x5002c)=powf(.98f,delta);
        g_esp=0x60000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0;
        MEM32(g_esp+8)=0x50000;MEM32(g_esp+12)=3;
        g_ebp=0xabc;g_ebx=0xdef;g_esi=0x123;g_edi=0x456;
        g_seh_ebp=g_ebp;sub_003D9520();
        assert(g_esp==0x60010 && MEM32(0x5fffc)==0xabc && g_ebx==0xdef && g_esi==0x123 && g_edi==0x456);
        printf("upload fps=%u a=%u b=%u d=%u\n",rates[r],
            (MEM32(0x108e4)>>8)&255,MEM32(0x108e8)&255,MEM32(0x108ec)&255);
    }
    free(memory);puts("PASS: complete current-recompiler constant upload");return 0;
}
