/* Execute the original material cache and double-buffered temporary allocator.
 * Matrix production is a controlled service; cache and allocation are guest code. */
#include <assert.h>
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
enum { OWNER=0x1000,PARAMS=0x4000,MATRIX=0x5000,STACK=0x10000 };
static unsigned copies;
void sub_0008E710(void) { g_eax=0;g_esp+=4; }
void sub_0008E730(void) { assert(!"unexpected second UV stage"); }
void sub_0008E750(void) { assert(!"unexpected third UV stage"); }
void sub_0008E770(void) { assert(!"unexpected fourth UV stage"); }
void sub_00094660(void)
{
    uint32_t dst=MEM32(g_esp+4),src=MEM32(g_esp+8);
    assert(src==MATRIX);memcpy((void *)(g_xbox_mem_offset+dst),(void *)(g_xbox_mem_offset+src),64);
    copies++;g_esp+=4;
}
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) do { (void)(target);assert(!"unexpected UV stage target");return; } while(0)
/* GENERATED_ROUTINES */
static unsigned run(unsigned rate,int fixed)
{
    unsigned errors=0;copies=0;
    MEM32(OWNER+0x10)=UINT32_MAX;MEM32(OWNER+0x14)=0;MEM32(OWNER+0x20)=MATRIX;
    MEM8(OWNER+0x1a)=0;MEM32(PARAMS+0xc)=0;
    MEM32(0x6b83a0)=0x20000;MEM32(0x6b83a4)=0x30000;MEM32(0x6b839c)=0x20000;
    for(unsigned f=0;f<rate*2;f++) {
        /* Real allocator reset alternates arenas; a camera allocation reuses
         * the previous matrix's bytes before material drawing on a later frame. */
        g_esp=STACK;sub_0003E930();assert(g_esp==STACK+4);
        uint32_t recycled=MEM32(0x6b839c);
        for(unsigned j=0;j<256;j++)MEMF(recycled+4*j)=-900.f-(float)j;
        g_esp=STACK;MEM32(STACK+4)=64;sub_0003E970();assert(g_esp==STACK+4);
        for(unsigned j=0;j<16;j++)MEMF(g_eax+4*j)=-900.f-(float)j;
        MEM32(0x7497ac)=fixed?f:(f*60/rate);
        for(unsigned j=0;j<16;j++)MEMF(MATRIX+4*j)=(float)(j+1);
        unsigned before=copies;
        for(unsigned pass=0;pass<3;pass++) {
            g_esp=STACK;MEM32(STACK+4)=OWNER;MEM32(STACK+8)=PARAMS;MEM32(STACK+12)=0;
            sub_000948B0();assert(g_esp==STACK+4 && g_eax);
            for(unsigned j=0;j<16;j++)if(MEMF(g_eax+4*j)!=(float)(j+1))errors++;
            if(pass)assert(copies==before+1 || !fixed);
        }
    }
    if(fixed)assert(copies==rate*2);
    return errors;
}
int main(void)
{
    void *mem=calloc(1,0x800000);assert(mem);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)mem;
    /* ORIGINAL_CONSTANTS */
    const unsigned rates[]={60,120,144,180,240};unsigned regressions=0;
    for(unsigned i=0;i<5;i++) {
        unsigned old=run(rates[i],0),fixed=run(rates[i],1);
        assert(!fixed);if(rates[i]>=144)assert(old>0);
        regressions+=old;printf("original material/arena at %u: stale-clock errors=%u, render-epoch errors=%u\n",rates[i],old,fixed);
    }
    assert(regressions>0);puts("PASS: original UV cache and temporary arena lifetime");free(mem);return 0;
}
