/* Execute the original US entity post-update and its list dispatcher.
 * Collision/sector services are stubbed; positions are supplied by the fixture.
 * This covers velocity derivation, not collision response or motion integration. */
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
RECOMP_TLS int g_fp_top,g_fp_cmp;
enum { ENTITY=0x1000, SECOND=0x2000, STACK=0x7000 };
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target, stack, site) assert(!"Unexpected entity callback")
#define STUB(name) void name(void) { g_esp+=4; }
STUB(sub_00057040)
STUB(sub_0008C690)
STUB(sub_0008C8E0)
STUB(sub_0009FBE0)
STUB(sub_0009FD60)
STUB(sub_000A0010)
STUB(sub_000A5F40)
STUB(sub_000A6290)
STUB(sub_000A64D0)
STUB(sub_000A6530)
STUB(sub_00129920)
STUB(sub_0012C1F0)
STUB(sub_0013A2C0)
STUB(sub_00171FB0)
STUB(sub_00238510)
STUB(sub_003515D0)
/* GENERATED_ROUTINES */
static const float velocity[3]={.125f,-.0625f,.03125f};
static void step(unsigned whole,float delta,int advancing)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    for(unsigned entity=ENTITY;entity<=SECOND;entity+=0x1000) {
        for(unsigned axis=0;axis<3;axis++) {
            if(advancing)
                MEMF(entity+0x64+axis*4)+=velocity[axis]*delta;
        }
    }
    g_esp=STACK;MEM32(STACK)=0x12345678;g_esi=0xdead;
    g_ebx=0xbeef;g_ebp=0xfeed;g_edi=0xabcd;
    sub_000A7870();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_ebp==0xfeed && g_edi==0xabcd);
    assert(g_fp_top==0);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    for(unsigned entity=ENTITY;entity<=SECOND;entity+=0x1000)
        for(unsigned axis=0;axis<3;axis++) {
            float expected=advancing && delta>0 &&
                (g_tsfp_native_timing_active || whole>0) ? velocity[axis] : 0;
            float actual=MEMF(entity+0xf8+axis*4);
            if(!isfinite(actual) || fabsf(actual-expected)>.0001f) {
                fprintf(stderr,"FAIL: entity velocity native=%d whole=%u delta=%g axis=%u got=%g expected=%g\n",
                        g_tsfp_native_timing_active,whole,delta,axis,actual,expected);
                abort();
            }
            assert(MEMF(entity+0xec+axis*4)==MEMF(entity+0x64+axis*4));
        }
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144,180,240};
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEMF(0x475c78)=1;
    MEM32(0x7b4cc0)=ENTITY;MEM32(ENTITY)=SECOND;
    MEM32(ENTITY+0x20)=MEM32(SECOND+0x20)=1;
    for(int native=0;native<=1;native++)
        for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++)
            for(unsigned slow=1;slow<=4;slow*=4) {
                unsigned fps=rates[rate];
                g_tsfp_native_timing_active=native;
                for(unsigned f=0;f<fps;f++)
                    step((f+1)*60/(fps*slow)-f*60/(fps*slow),60.f/(fps*slow),1);
                step(0,0,0);
                step(0,60.f/fps,0); /* Stationary objects report zero velocity. */
            }
    g_tsfp_native_timing_active=1;
    step(3,3,1);step(2,2.5f,1);step(2,1.8f,1);step(0,.25f,1);
    MEMF(ENTITY+0x64)+=1;MEMF(SECOND+0x68)-=1;
    step(1,0,0); /* A scoped pause wins over a carried integer tick. */
    assert(RECOMP_LOAD_GLOBAL_U32(0xa75e8,0x4e7944,7)==7);
    assert(RECOMP_LOAD_GLOBAL_U32(0xa75e7,0x4e7940,7)==7);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0xa75e7,0x4e7944,3)==3);
    free(memory);
    puts("PASS: original entity velocity and list dispatch at 60/120/144/180/240, slow time, pause and catch-up");
    return 0;
}
