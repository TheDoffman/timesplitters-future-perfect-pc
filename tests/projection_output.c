/* Validate the output extent and arithmetic of the checksum-verified US
 * perspective builder. The lifted routine and constants stay local. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while(0)
/* ORIGINAL_ROUTINE */
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    CHECK(memory);
    g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    const float aspects[]={.8f,4.f/3.f,16.f/9.f,3440.f/1440.f};
    const float fovs[]={25.f,55.f,73.f,100.f};
    const float near_planes[]={.01f,.1f,1.f};
    for(size_t a=0;a<4;a++)for(size_t f=0;f<4;f++)for(size_t n=0;n<3;n++) {
        const uint32_t dest=0x6000+(uint32_t)(n%2)*0x100;
        const float near_z=near_planes[n],far_z=1000.f;
        float expected[16]={0};
        double cot=1.0/tan((double)fovs[f]*MEMF(0x475CD4));
        expected[0]=(float)(cot/aspects[a]);expected[5]=(float)cot;
        expected[10]=-far_z/(far_z-near_z);expected[11]=-1.f;
        expected[14]=-(far_z/(far_z-near_z))*near_z;
        /* NaNs reveal missing writes even for zero-valued matrix entries. */
        for(size_t i=0;i<16;i++)MEM32(dest+4*(uint32_t)i)=0x7FC00001u;
        MEM32(dest-4)=0x12345678;MEM32(dest+64)=0x87654321;
        g_esp=0x10000;g_fp_top=0;
        MEM32(g_esp)=0x112F7F;MEM32(g_esp+4)=dest;
        MEMF(g_esp+8)=aspects[a];MEMF(g_esp+12)=fovs[f];
        MEMF(g_esp+16)=near_z;MEMF(g_esp+20)=far_z;
        sub_00151CF0_gen();
        CHECK(g_esp==0x10004 && g_eax==dest && g_fp_top==0);
        CHECK(MEM32(dest-4)==0x12345678 && MEM32(dest+64)==0x87654321);
        for(size_t i=0;i<16;i++) {
            float actual=MEMF(dest+4*(uint32_t)i);
            CHECK(isfinite(actual) && fabsf(actual-expected[i])<1e-5f);
        }
    }
    free(memory);
    puts("PASS: original projection writes all 16 floats, preserves bounds and balances stack");
    return 0;
}
