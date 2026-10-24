/* Original remote-player correction, with prediction/transport services
 * controlled. This measures the correction stage, not packet delivery. */
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
enum { PLAYER=0x1000, PEER=0x3000, ENTITY=0x4000, STACK=0x7000, RECORD=0x777240 };
static unsigned predictions;
static int connected=1;
static float target[3];
void sub_003533F0(void) {g_eax=connected;g_esp+=4;}
void sub_003568C0(void) {g_eax=1;g_esp+=4;}
void sub_001CE850(void)
{
    predictions++;
    for(unsigned i=0;i<3;i++) MEMF(PLAYER+0xa8+4*i)=MEMF(ENTITY+0x64+4*i)=target[i];
    g_esp+=4;
}
#define UNUSED(name) void name(void) { fprintf(stderr,"Unexpected: %s\n",#name);abort(); }
UNUSED(sub_000CCAB0)
UNUSED(sub_000D0CE0)
UNUSED(sub_002EC850)
/* GENERATED_ROUTINES */
static void step(unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xabcd;g_ebp=0xfeed;
    sub_00356E20();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xabcd && g_ebp==0xfeed);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void origin(void)
{
    for(unsigned i=0;i<3;i++) MEMF(PLAYER+0xa8+4*i)=MEMF(ENTITY+0x64+4*i)=0;
    target[0]=10;target[1]=target[2]=0;predictions=0;
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER)=0;MEM32(PLAYER+4)=UINT32_MAX;
    MEM32(PLAYER+0x14)=ENTITY;MEM32(RECORD+4)=PEER;MEM32(PEER)=1;
    MEM32(RECORD+0x120)=1;MEM32(0x4e8cec)=60;
    const unsigned rates[]={60,60,120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned fps=rates[r],changed=0;float delta=60.f/(fps*slow);
        g_tsfp_native_timing_active=r!=0;origin();
        for(unsigned f=0;f<fps/2;f++) {
            float before=MEMF(ENTITY+0x64);
            step((f+1)*60/(fps*slow)-f*60/(fps*slow),delta);
            assert(fabsf(MEMF(ENTITY+0x64)-before-.2f*delta)<.00001f);
            changed+=MEMF(ENTITY+0x64)>before;
        }
        assert(changed==fps/2 && predictions==fps/2);
        assert(fabsf(MEMF(ENTITY+0x64)-6.f/slow)<.0001f);
        printf("remote native=%d fps=%u slow=%u changed=%u displacement=%.7f\n",
               g_tsfp_native_timing_active,fps,slow,changed,MEMF(ENTITY+0x64));
    }
    g_tsfp_native_timing_active=1;
    origin();step(1,0);assert(MEMF(ENTITY+0x64)==0); /* Pause beats carried tick. */
    origin();step(0,.25f);assert(fabsf(MEMF(ENTITY+0x64)-.05f)<.000001f);
    origin();step(3,3);assert(fabsf(MEMF(ENTITY+0x64)-.6f)<.000001f);
    origin();target[0]=3;target[1]=4;step(0,.5f);
    assert(fabsf(MEMF(ENTITY+0x64)-.06f)<.000001f);
    assert(fabsf(MEMF(ENTITY+0x68)-.08f)<.000001f);
    origin();target[0]=.01f;step(0,.5f);assert(MEMF(ENTITY+0x64)==.01f);
    origin();target[0]=20;step(0,.5f);assert(MEMF(ENTITY+0x64)==20);
    origin();target[0]=19.99f;step(0,.5f);assert(fabsf(MEMF(ENTITY+0x64)-.1f)<.000001f);
    /* An expired peer skips prediction and retains its recorded position. */
    origin();MEMF(PLAYER+0xa8)=7;MEM32(0x7de318)=160;MEM32(RECORD+0x108)=100;
    step(0,.5f);assert(predictions==0 && MEMF(ENTITY+0x64)==7);
    MEM32(0x7de318)=0;MEM32(RECORD+0x108)=0;
    /* Local and offline callbacks continue to use their original update. */
    origin();MEM32(PLAYER+4)=0;step(0,.5f);assert(predictions==1 && MEMF(ENTITY+0x64)==10);
    origin();connected=0;MEM32(PLAYER+4)=UINT32_MAX;step(0,.5f);
    assert(predictions==1 && MEMF(ENTITY+0x64)==10);
    free(memory);puts("PASS: original remote correction advances fractionally and preserves pause/teleport/stale/local paths");
    return 0;
}
