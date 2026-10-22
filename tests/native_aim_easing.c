/* Original player aim/weapon-offset easing blocks, including their stores
 * and snap threshold. Incoming target vectors are controlled; target
 * acquisition, collisions, input delivery and rendered poses are outside scope. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
enum { PLAYER=0x1000, TARGET=0x3000, STACK=0x10000 };
/* GENERATED_ROUTINES */
static const unsigned offsets[]={0xf6c,0xf70,0xf64,0xf68};
static const float targets[]={8.f,-4.f,6.f,-3.f};
static float raw_delta;
static void step(int mode,unsigned whole,float delta)
{
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;MEMF(0x4e7948)=raw_delta;
    g_esp=STACK;g_eax=g_ecx=PLAYER;g_edi=0;
    MEM32(STACK)=0x12345678;
    if(!mode) {
        g_xmm0.f[0]=targets[0];g_xmm1.f[0]=targets[1];
        MEMF(STACK+0x18)=targets[2];MEMF(STACK+0x1c)=targets[3];
        sub_0025AB82();
    } else {
        MEM32(STACK+0x44)=TARGET;
        MEMF(TARGET)=-targets[2];MEMF(TARGET+4)=-targets[3];
        sub_0025B0F3();
    }
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    setvbuf(stdout,NULL,_IONBF,0);
    /* ORIGINAL_CONSTANTS */
    MEM32(0x7b0c7c)=PLAYER;
    const unsigned rates[]={60,60,120,144};
    for(int mode=0;mode<2;mode++)for(unsigned r=0;r<4;r++)
    for(unsigned slow=1;slow<=4;slow*=4) {
        unsigned fps=rates[r];g_tsfp_native_timing_active=r!=0;raw_delta=60.f/fps;
        for(unsigned c=0;c<4;c++)MEMF(PLAYER+offsets[c])=0;
        for(unsigned f=0;f<fps*slow/2;f++) {
            unsigned whole=(f+1)*60/(fps*slow)-f*60/(fps*slow);
            step(mode,whole,60.f/(fps*slow));
            for(unsigned c=mode?2:0;c<4;c++) {
                double ticks=(f+1)*60./fps;
                double expected=targets[c]*(1-pow(1-(double)MEMF(0x475d24)/slow,ticks));
                float actual=MEMF(PLAYER+offsets[c]);
                if(fabs(actual-expected)>.00002) {
                    printf("aim mode=%d fps=%u slow=%u frame=%u channel=%u actual=%.9f expected=%.9f\n",
                           mode,fps,slow,f,c,actual,expected);
                    assert(!"aim easing differs from original one-tick response");
                }
                assert(isfinite(actual));
            }
            float saved[4];for(unsigned c=0;c<4;c++)saved[c]=MEMF(PLAYER+offsets[c]);
            step(mode,0,0);
            for(unsigned c=0;c<4;c++)assert(MEMF(PLAYER+offsets[c])==saved[c]);
        }
        printf("aim mode=%d native=%d fps=%u slow=%u offset=%.9f\n",
               mode,g_tsfp_native_timing_active,fps,slow,MEMF(PLAYER+0xf64));
    }
    /* Uneven sub-tick intervals sum to two original updates per group.
     * Compare against the original block, including quarter-speed time. */
    const float intervals[]={.27f,.83f,.4f,.5f};
    for(int mode=0;mode<2;mode++)for(unsigned slow=1;slow<=4;slow*=4) {
        float reference[4];
        g_tsfp_native_timing_active=0;raw_delta=1.f;
        for(unsigned c=0;c<4;c++)MEMF(PLAYER+offsets[c])=0;
        for(unsigned i=0;i<20;i++)step(mode,i%slow==slow-1,1.f/slow);
        for(unsigned c=0;c<4;c++)reference[c]=MEMF(PLAYER+offsets[c]);
        g_tsfp_native_timing_active=1;
        for(unsigned c=0;c<4;c++)MEMF(PLAYER+offsets[c])=0;
        for(unsigned i=0;i<40;i++) {
            raw_delta=intervals[i%4];step(mode,0,raw_delta/slow);
        }
        for(unsigned c=0;c<4;c++)assert(fabsf(MEMF(PLAYER+offsets[c])-reference[c])<.00002f);
    }
    /* Whole and stalled catch-up updates retain the exact original result. */
    for(int mode=0;mode<2;mode++)for(unsigned ticks=1;ticks<=3;ticks++) {
        float reference[4];raw_delta=(float)ticks;
        g_tsfp_native_timing_active=0;
        for(unsigned c=0;c<4;c++)MEMF(PLAYER+offsets[c])=0;
        step(mode,ticks,(float)ticks);
        for(unsigned c=0;c<4;c++)reference[c]=MEMF(PLAYER+offsets[c]);
        g_tsfp_native_timing_active=1;
        for(unsigned c=0;c<4;c++)MEMF(PLAYER+offsets[c])=0;
        step(mode,ticks,(float)ticks);
        for(unsigned c=0;c<4;c++)assert(MEMF(PLAYER+offsets[c])==reference[c]);
    }
    /* Original snap threshold and unrelated sites must remain unchanged. */
    for(unsigned c=0;c<4;c++)MEMF(PLAYER+offsets[c])=targets[c]+MEMF(0x4785b0)*.5f;
    step(0,0,.5f);
    for(unsigned c=0;c<4;c++)assert(MEMF(PLAYER+offsets[c])==targets[c]);
    MEMF(0x4e794c)=.5f;
    MEMF(0x4e7948)=.5f;
    const unsigned sites[]={0x25abce,0x25ac1e,0x25ac80,0x25acca,0x25b13c,0x25b16d};
    for(unsigned i=0;i<6;i++) {
        g_tsfp_native_timing_active=1;
        assert(pc_native_multiply(sites[i]+1,2,.1f)==2*.1f);
        assert(pc_native_multiply(sites[i],2,0)==0);
        assert(pc_native_multiply(sites[i],2,1)==2);
        g_tsfp_native_timing_active=0;
        assert(pc_native_multiply(sites[i],2,.1f)==2*.1f);
    }
    free(memory);
    puts("PASS: original aim/weapon-offset easing, pause, snap and scope at 60/120/144");
    return 0;
}
