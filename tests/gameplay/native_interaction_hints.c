/* Complete original HUD message update and queue eligibility routines.
 * Sound playback is observed, not performed. Test elapsed game time, delayed
 * messages, queue limits and pause across original/native update rates. */
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
RECOMP_TLS int g_fp_top,g_df;
/* GENERATED_ROUTINES */
#define HINT(n) (0x746ed0u+(n)*0x10u)
static void call_update(unsigned raw,float game,float raw_delta)
{
    MEM32(0x4e7940)=raw;MEMF(0x4e7948)=raw_delta;MEMF(0x4e794c)=game;
    g_esp=0x10000;MEM32(g_esp)=0x12345678;sub_00140630();
    assert(g_esp==0x10004 && MEM32(0x10000)==0x12345678);
    assert(MEMF(0x4e794c)==game && MEMF(0x4e7948)==raw_delta);
}
static void refresh(unsigned port,unsigned slot)
{
    g_esp=0x10000;MEM32(g_esp)=0x12345678;
    MEM32(g_esp+4)=port;MEM32(g_esp+8)=MEM32(0x4e73a8+4*slot);MEM32(g_esp+12)=0x17d0;
    sub_001405B0();assert(g_esp==0x10004);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    const unsigned rates[]={60,60,120,144,180,240};
    for(unsigned mode=0;mode<6;mode++)for(unsigned phase=0;phase<8;phase++)
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned late=0;late<2;late++) {
        memset(memory,0,0x800000);
        /* ORIGINAL_CONSTANTS */
        MEM32(0x4e8cec)=60;MEM32(0x7b0c7c)=0x1000;MEM32(0x1004)=0;
        for(unsigned n=0;n<18;n++)MEM32(0x4e73a8+4*n)=n+1;
        unsigned fps=rates[mode];g_tsfp_native_timing_active=mode!=0;
        for(unsigned frame=0;frame<fps*slow;frame++) {
            unsigned raw=(frame+phase+1)*60/fps-(frame+phase)*60/fps;
            /* Level scripts publish the eligible hint at the raw 60-Hz boundary.
             * Its fade update is visited at each render update. */
            if(raw && !late)refresh(0,7);
            call_update(raw,60.f/fps/slow,60.f/fps);
            if(raw && late)refresh(0,7);
            if(frame+1==fps*slow/4)
                assert(MEMF(HINT(7))>=.99f && "gameplay hint remains faded");
        }
        if(MEMF(HINT(7))<.99f)fprintf(stderr,"faded native=%d fps=%u phase=%u slow=%u alpha=%g\n",g_tsfp_native_timing_active,fps,phase,slow,MEMF(HINT(7)));
        assert(MEMF(HINT(7))>=.99f && "gameplay hint remains faded");
        float before=MEMF(HINT(7));
        for(unsigned f=0;f<fps;f++)call_update(1,0,1);
        assert(MEMF(HINT(7))==before && "paused hint changed");
        for(unsigned f=0;f<fps*slow/2;f++) {
            unsigned raw=(f+1)*60/fps-f*60/fps;
            call_update(raw,60.f/fps/slow,60.f/fps);
        }
        assert(MEMF(HINT(7))==0.f && "unavailable hint did not fade");
        /* Another split-screen player's hint is not advanced by this viewport. */
        refresh(1,7);before=MEMF(HINT(18+7));
        call_update(1,1,1);assert(MEMF(HINT(18+7))==before);
    }
    puts("PASS: gameplay hint opacity and fade at60/120/144/180/240, pause, slow motion and player isolation");
    free(memory);return 0;
}
