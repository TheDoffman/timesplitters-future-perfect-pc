/* Execute the original US player dispatcher on inactive and active branches.
 * Its elapsed time, hit setter, displacement block and decay run; collision,
 * world/input services are mocked.
 * This does not replace a collision or full movement comparison. */
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
RECOMP_TLS int g_fp_top;
#define STUB(name) void name(void) { g_eax=0; g_esp+=4; }
/* Variable-site scope checks must never dispatch another numeric policy. */
float pc_native_rocking_add(uint32_t site,float left,float right) {
    (void)site;(void)left;(void)right;assert(!"unexpected rocking policy");return 0;
}
float pc_native_weapon_sway(float original) {
    (void)original;assert(!"unexpected weapon sway policy");return 0;
}
static int active_case;
static unsigned movement_calls;
#define UNUSED(name) void name(void) { assert(active_case); g_eax=0; g_esp+=4; }
UNUSED(sub_00042870)
UNUSED(sub_00058C70)
UNUSED(sub_00061CA0)
UNUSED(sub_00065FE0)
UNUSED(sub_00099280)
UNUSED(sub_000A5A60)
UNUSED(sub_000BA7D0)
UNUSED(sub_000BD090)
UNUSED(sub_000C45F0)
UNUSED(sub_000CB3B0)
UNUSED(sub_000CBD10)
UNUSED(sub_000D0CA0)
UNUSED(sub_000D19F0)
UNUSED(sub_000D1B40)
UNUSED(sub_000D1CB0)
UNUSED(sub_000D2190)
UNUSED(sub_000D2280)
UNUSED(sub_000D2330)
UNUSED(sub_000D2B80)
UNUSED(sub_000D3FE0)
void sub_00259344(void);
void sub_000D5C70(void) {
    assert(active_case);movement_calls++;
    uint32_t saved_esi=g_esi;
    g_esi=0x9000;g_xmm1.f[0]=MEMF(0x9008);
    /* The original integration block returns through this service's frame. */
    sub_00259344();g_esi=saved_esi;g_eax=0;
}
STUB(sub_001213D0)
STUB(sub_001260C0)
UNUSED(sub_00238510)
UNUSED(sub_0023F1E0)
UNUSED(sub_00259E50)
UNUSED(sub_0025A080)
UNUSED(sub_0025A0C0)
UNUSED(sub_0025A3B0)
UNUSED(sub_0025A7F0)
UNUSED(sub_0025B720)
STUB(sub_0025C120)
UNUSED(sub_0025C650)
UNUSED(sub_0025CE80)
UNUSED(sub_0025D520)
UNUSED(sub_0025E650)
STUB(sub_0025F0C0)
UNUSED(sub_0033E530)
STUB(sub_003533F0)
UNUSED(sub_00356940)
/* Hit setup keeps original direction/gain arithmetic; HUD/rumble and the
 * optional camera-kick service are boundaries, not simulated physics. */
UNUSED(sub_0013AB10)
UNUSED(sub_0013A1D0)
UNUSED(sub_0036CF00)
UNUSED(sub_000D1EC0)
void sub_00153B40(void) { g_eax=1; g_esp+=4; }
/* GENERATED_ROUTINES */
static void dispatch(unsigned whole, float delta)
{
    MEM32(0x4e7944)=whole; MEMF(0x4e794c)=delta;
    g_esp=0x7000; MEM32(g_esp)=0x12345678;
    g_esi=0xdef;
    sub_000D5EB0();
    assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678 && g_esi==0xdef);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
}
static void hit(void) {
    MEM32(0x3150)=0x1000;MEM32(0x8000)=0x3000;
    MEM32(0x8018)=0xffffffff;MEMF(0x8020)=5;
    MEM32(0x8024)=9;MEM32(0x8030)=0x8100;
    MEMF(0x8100)=1;MEMF(0x8104)=-.5f;MEMF(0x8108)=.25f;
    g_esp=0x7000;MEM32(g_esp)=0x12345678;MEM32(g_esp+4)=0x8000;
    g_esi=0xdef;g_edi=0xabc;sub_000B9510();
    assert(g_esp==0x7004 && g_esi==0xdef && g_edi==0xabc && g_fp_top==0);
    float gain=MEMF(0x4dad80+9*60)*MEMF(0x4785e0);
    assert(gain>0 && MEM32(0x4dad48+9*60)!=1);
    assert(MEMF(0x10f4)==gain && MEMF(0x10f8)==-.5f*gain && MEMF(0x10fc)==.25f*gain);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);
    const unsigned rates[]={60,120,144};
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x7b0c7c)=0x1000; MEM32(0x1014)=0x3000; MEM32(0x307c)=0x4000;
    MEM32(0x1004)=0xffffffff; MEM32(0x4e8cec)=60;
    MEMF(0x475c78)=1; MEMF(0x475cd4)=.5f;
    for(unsigned r=0;r<3;r++) for(int native=0;native<2;native++) {
        unsigned fps=rates[r];
        MEMF(0x4b30)=0; MEMF(0x2008)=1;
        g_tsfp_native_timing_active=native;
        for(unsigned frame=0;frame<fps;frame++)
            dispatch((frame+1)*60/fps-frame*60/fps,60.0f/fps);
        float expected=native ? 1.0f : 60.0f/fps;
        assert(fabsf(MEMF(0x4b30)-expected)<.00001f);
        assert(fabsf(MEMF(0x2008)/ldexpf(1,-60)-1)<.00002f);
        float before=MEMF(0x4b30); MEMF(0x2008)=1;
        dispatch(0,0); assert(MEMF(0x4b30)==before && MEMF(0x2008)==1);
        if(native) {
            dispatch(0,.25f);
            assert(fabsf(MEMF(0x4b30)-before-.25f/60)<.00001f);
            assert(fabsf(MEMF(0x2008)-powf(.5f,.25f))<.000001f);
            dispatch(3,3);
            assert(fabsf(MEMF(0x4b30)-before-3.25f/60)<.00001f);
            assert(fabsf(MEMF(0x2008)-powf(.5f,3.25f))<.000001f);
        }
    }
    assert(RECOMP_LOAD_GLOBAL_U32(0xd5ec0,0x4e7944,7)==7);
    assert(RECOMP_LOAD_GLOBAL_U32(0xd6251,0x4e7944,0)==0);
    assert(RECOMP_F32_MUL_AT(0xd5f61,8,.5f)==4);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_LOAD_GLOBAL_U32(0xd5ebf,0x4e7944,3)==3);
    assert(RECOMP_F32_MUL_AT(0xd5f60,8,.5f)==4);
    /* Active-player branch: real hit-vector stores after the world step.
     * All three channels must retain the original decay over equal game time.
     * Stubbed input/world helpers do not touch these fields. */
    active_case=1;MEM32(0x1004)=0;MEM32(0x1114)=2;
    MEM32(0x4c00)=0x6000;MEMF(0x4b08)=MEMF(0x4b0c)=1;
    for(unsigned mode=0;mode<4;mode++) for(unsigned slow=0;slow<2;slow++) {
        if(mode==0 && slow)continue; /* Preserve the original disabled path. */
        unsigned fps=mode<2?60:rates[mode-1],steps=fps/6,whole_total=0;
        float scale=slow?.25f:1.f,elapsed=0;
        g_tsfp_native_timing_active=(mode!=0);movement_calls=0;
        hit();float initial=MEMF(0x10f4);
        MEMF(0x9000)=MEMF(0x9004)=MEMF(0x9008)=0;
        for(unsigned frame=0;frame<steps;frame++) {
            elapsed=(frame+1)*60.0f/fps*scale;
            unsigned next=(unsigned)(elapsed+.00001f);
            dispatch(next-whole_total,60.0f/fps*scale);whole_total=next;
        }
        float expected=powf(MEMF(0x476040),10*scale);
        printf("player_hit native=%u fps=%u scale=%.2f calls=%u vector=%.9f,%.9f,%.9f expected=%.9f\n",
               mode!=0,fps,scale,movement_calls,MEMF(0x10f4),MEMF(0x10f8),MEMF(0x10fc),initial*expected);
        float distance=initial*(1-expected)/(1-MEMF(0x476040));
        printf("player_hit_distance fps=%u scale=%.2f actual=%.9f expected=%.9f\n",
               fps,scale,MEMF(0x9000),distance);
        fflush(stdout);
        assert(movement_calls==steps);
        assert(fabsf(MEMF(0x9000)-distance)<.00001f);
        assert(fabsf(MEMF(0x9004)+.5f*distance)<.00001f);
        assert(fabsf(MEMF(0x9008)-.25f*distance)<.00001f);
        assert(fabsf(MEMF(0x10f4)/initial-expected)<.00001f);
        assert(fabsf(MEMF(0x10f8)/(-.5f*initial)-expected)<.00001f);
        assert(fabsf(MEMF(0x10fc)/(.25f*initial)-expected)<.00001f);
        float held[3]={MEMF(0x10f4),MEMF(0x10f8),MEMF(0x10fc)};
        float position[3]={MEMF(0x9000),MEMF(0x9004),MEMF(0x9008)};
        unsigned calls=movement_calls;
        dispatch(0,0);
        if(mode)dispatch(3,0); /* A stale integer count cannot advance pause. */
        assert(movement_calls==calls);
        assert(MEMF(0x9000)==position[0] && MEMF(0x9004)==position[1] && MEMF(0x9008)==position[2]);
        assert(MEMF(0x10f4)==held[0] && MEMF(0x10f8)==held[1] && MEMF(0x10fc)==held[2]);
    }
    /* Uneven intervals and a new hit after earlier decay. The setter replaces
     * the three channels; no per-player cached decay state may leak through. */
    g_tsfp_native_timing_active=1;hit();float initial=MEMF(0x10f4);
    MEMF(0x9000)=MEMF(0x9004)=MEMF(0x9008)=0;
    const float intervals[]={.25f,.75f,2.f,.5f,1.5f,5.f};
    float elapsed=0;unsigned whole_total=0;
    for(unsigned i=0;i<sizeof intervals/sizeof intervals[0];i++) {
        elapsed+=intervals[i];unsigned next=(unsigned)elapsed;
        dispatch(next-whole_total,intervals[i]);whole_total=next;
    }
    assert(fabsf(MEMF(0x10f4)/initial-powf(MEMF(0x476040),10))<.00001f);
    float distance=initial*(1-powf(MEMF(0x476040),10))/(1-MEMF(0x476040));
    assert(fabsf(MEMF(0x9000)-distance)<.00001f);
    hit();assert(MEMF(0x10f4)==initial);
    dispatch(0,.5f);
    assert(fabsf(MEMF(0x10f4)/initial-sqrtf(MEMF(0x476040)))<.00001f);
    float extra=initial*(1-sqrtf(MEMF(0x476040)))/(1-MEMF(0x476040));
    assert(fabsf(MEMF(0x9000)-distance-extra)<.00001f);
    const unsigned sites[]={0xd64a4,0xd64bd,0xd64d6};
    const unsigned additions[]={0x259352,0x259368,0x25937f};
    for(unsigned i=0;i<3;i++) {
        assert(RECOMP_F32_MUL_AT(sites[i]+1,8,.9f)==8*.9f);
        assert(RECOMP_F32_ADD_AT(additions[i]+1,8,2)==10);
        MEMF(0x4e794c)=0;
        assert(RECOMP_F32_ADD_AT(additions[i],8,2)==2);
        MEMF(0x4e794c)=1;
        assert(RECOMP_F32_ADD_AT(additions[i],8,2)==10);
        assert(RECOMP_F32_MUL_AT(sites[i],8,.9f)==8*.9f);
        MEMF(0x4e794c)=.5f;g_tsfp_native_timing_active=0;
        assert(RECOMP_F32_ADD_AT(additions[i],8,2)==10);
        assert(RECOMP_F32_MUL_AT(sites[i],8,.9f)==8*.9f);
        g_tsfp_native_timing_active=1;
    }
    free(memory);
    puts("PASS: original player elapsed time, hit displacement and decay at 60/120/144");
    return 0;
}
