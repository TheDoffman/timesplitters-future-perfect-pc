/* Original character-effect dispatcher and attached-effect timer. Other
 * effect systems are spies; original matrix helpers execute unchanged. */
#include <assert.h>
#include <math.h>
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
enum { RECORD=0x1000, ACTOR=0x2000, ENTITY=0x3000, STACK=0x10000 };
#define STUB(name) void name(void) { g_eax=0;g_esp+=4; }
STUB(sub_001213F0) STUB(sub_0011D570) STUB(sub_00125B00)
STUB(sub_0011E920) STUB(sub_0011EE50) STUB(sub_00120660)
STUB(sub_0011FD20) STUB(sub_0011F7E0) STUB(sub_00120110)
STUB(sub_00120270) STUB(sub_00125FE0) STUB(sub_001232D0)
STUB(sub_00124D90) STUB(sub_00120C50) STUB(sub_0011FB00)
STUB(sub_0011FBE0) STUB(sub_00121D00) STUB(sub_00124E20) STUB(sub_00124F60)
#define UNUSED(name) void name(void) { assert(!"unexpected " #name); }
UNUSED(sub_000FF4B0) UNUSED(sub_000FFF70) UNUSED(sub_000FF420) UNUSED(sub_000FF490)
static int legacy;
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    (legacy && (site)==0x1260c0 ? (value) : pc_native_global_load(site,address,value))
/* GENERATED_ROUTINES */
static void step(unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=RECORD;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    sub_001260C0();
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);MEM32(0x4e8cec)=60;
    const unsigned rates[]={60,120,144,180,240};
    for(legacy=1;legacy>=0;legacy--)
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned phase=0;phase<4;phase++) {
        memset((void *)(g_xbox_mem_offset+RECORD),0,0x4000);
        MEM32(RECORD)=ACTOR;MEM32(ACTOR)=ENTITY;MEM32(RECORD+4)=0x100;
        g_tsfp_native_timing_active=1;
        unsigned count=rates[r]*slow,previous=phase/4;
        for(unsigned frame=1;frame<=count;frame++) {
            unsigned tick=(frame*240+phase*count)/(count*4);
            float before=MEMF(RECORD+0x10);
            step(tick-previous,60.f/count);previous=tick;
            if(!legacy) {
                assert(MEMF(RECORD+0x10)>before);
                assert(fabsf(MEMF(RECORD+0x10)-(float)frame/count)<.00002f);
            }
        }
        float expected=legacy?60.f/count:1.f;
        printf("character_effect legacy=%d fps=%u slow=%u phase=%u elapsed=%f expected=%f\n",
               legacy,rates[r],slow,phase,MEMF(RECORD+0x10),expected);
        assert(fabsf(MEMF(RECORD+0x10)-expected)<.00002f);
        float before=MEMF(RECORD+0x10);step(0,0);step(1,0);
        assert(MEMF(RECORD+0x10)==before);
    }
    legacy=0;g_tsfp_native_timing_active=0;
    float before=MEMF(RECORD+0x10);step(0,.25f);
    assert(MEMF(RECORD+0x10)==before); /* Original gate remains unchanged. */
    step(1,1);assert(fabsf(MEMF(RECORD+0x10)-before-1.f/60)<.000001f);
    puts("PASS: original character-effect timer advances through dispatcher at 60/120/144/180/240, slow time, pause and carried phases");
    free(memory);return 0;
}
