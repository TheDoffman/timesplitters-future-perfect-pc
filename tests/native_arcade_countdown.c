/* Execute the complete US match-state dispatcher and its original input-mask
 * writer. Rendering, messages and sound are spies; unrelated state paths
 * abort. The countdown must reach gameplay with the original cue cadence. */
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

#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected countdown switch target")
static unsigned started, announcements, sounds;
#define UNUSED(name) void name(void) { fprintf(stderr,"Unexpected: %s\n",#name); abort(); }
UNUSED(sub_000344E0)
UNUSED(sub_00042870)
UNUSED(sub_000459E0)
UNUSED(sub_00055D80)
UNUSED(sub_00055D90)
UNUSED(sub_00056050)
UNUSED(sub_00061E90)
UNUSED(sub_00061EE0)
UNUSED(sub_00064D10)
UNUSED(sub_00065230)
UNUSED(sub_00065250)
UNUSED(sub_00065270)
UNUSED(sub_00065470)
UNUSED(sub_000658D0)
UNUSED(sub_00065940)
UNUSED(sub_00065FE0)
UNUSED(sub_000667B0)
UNUSED(sub_000667F0)
UNUSED(sub_000687A0)
UNUSED(sub_00098DB0)
UNUSED(sub_0009DA10)
UNUSED(sub_000C6390)
UNUSED(sub_000C63F0)
UNUSED(sub_000CBCF0)
UNUSED(sub_000D0CE0)
UNUSED(sub_000D50E0)
UNUSED(sub_00125F50)
UNUSED(sub_00153AF0)
UNUSED(sub_001553E0)
UNUSED(sub_00164B60)
UNUSED(sub_00212490)
UNUSED(sub_0022DEC0)
UNUSED(sub_002321C0)
UNUSED(sub_0025A570)
UNUSED(sub_002E44A0)
UNUSED(sub_002E4C10)
UNUSED(sub_002E68E0)
UNUSED(sub_002EB030)
UNUSED(sub_002EB1F0)
UNUSED(sub_002EC250)
UNUSED(sub_002EDD90)
UNUSED(sub_002F43D0)
UNUSED(sub_003526D0)
UNUSED(sub_00352BB0)
UNUSED(sub_003533F0)
UNUSED(sub_00355EC0)
UNUSED(sub_00355F10)
UNUSED(sub_00356800)
UNUSED(sub_00357690)
UNUSED(sub_00357BD0)
UNUSED(sub_00359AA0)
UNUSED(sub_00359E70)
UNUSED(sub_0035DB10)
UNUSED(sub_0035E960)
void sub_001CDAE0(void) { g_eax=1; g_esp+=4; }
void sub_002E68C0(void) { g_eax=0; g_esp+=4; }
void sub_00358E40(void) { g_eax=0; g_esp+=4; }
void sub_000681C0(void) { g_esp+=4; }
void sub_0003D7B0(void) { g_eax=0x1000; g_esp+=4; }
void sub_003C85D4(void) { g_eax=0; g_esp+=4; }
void sub_0012D350(void) { announcements++; g_esp+=4; }
void sub_00164A30(void) { assert(MEM32(g_esp+4)==4+sounds); sounds++; g_esp+=4; }
void sub_002F2D30(void) { started++; g_esp+=4; }
uint32_t pc_native_visual_store(uint32_t site,uint32_t value) { (void)site;return value; }
/* GENERATED_ROUTINES */
int main(void)
{
    unsigned char *memory=calloc(1,0x800000); assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    const unsigned rates[]={60,120,144};
    for(unsigned test=0;test<4;test++) {
        unsigned fps=test?rates[test-1]:60;
        g_tsfp_native_timing_active=test!=0;
        memset(memory,0,0x800000); started=announcements=sounds=0;
        /* ORIGINAL_CONSTANTS */
        MEMF(0x475c78)=1;MEM32(0x4e8cec)=60;
        MEM32(0x7de30c)=5;MEM32(0x4d2878)=3;MEM32(0x4d287c)=0xffffffff;
        for(unsigned port=0;port<4;port++)MEM32(0x6b93c0+4*port)=port;
        unsigned ticks=0,frame=0;
        for(;frame<fps*5 && !started;frame++) {
            unsigned whole=(frame+1)*60/fps-frame*60/fps;ticks+=whole;
            MEM32(0x4e7940)=MEM32(0x4e7944)=whole;
            MEMF(0x4e7948)=MEMF(0x4e794c)=60.f/fps;
            MEM32(0x6b9408)=0;g_esp=0x7000;MEM32(g_esp)=0x12345678;
            sub_00068830();assert(g_esp==0x7004 && MEM32(0x7000)==0x12345678);
            for(unsigned port=0;port<4;port++)
                assert(MEM32(0x6b9408+port*0x3c)==0xfffffdff);
        }
        printf("countdown native=%d fps=%u frames=%u ticks=%u state=%u sounds=%u\n",
               g_tsfp_native_timing_active,fps,frame,ticks,MEM32(0x7de30c),sounds);fflush(stdout);
        assert(started==1 && MEM32(0x7de30c)==7 && ticks==184);
        assert(sounds==4 && announcements==4);
    }
    /* Only the integer countdown addition changes; paused raw time and
     * adjacent instruction sites retain their own operands. */
    g_tsfp_native_timing_active=1;MEM32(0x4e7940)=3;
    assert(RECOMP_F32_ADD_AT(0x69043,20.f,0.f)==20.f);
    assert(RECOMP_F32_ADD_AT(0x69043,20.f,.4f)==23.f);
    assert(RECOMP_F32_ADD_AT(0x69044,20.f,.5f)==20.5f);
    g_tsfp_native_timing_active=0;
    assert(RECOMP_F32_ADD_AT(0x69043,20.f,.5f)==20.5f);
    free(memory);puts("PASS: original Arcade countdown releases input at equal console time");return 0;
}
