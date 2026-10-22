/* Execute the complete US match dispatcher, mode dispatcher and Escort
 * resource recovery routine. World/network/rendering services are controlled. */
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
#define RECOMP_ITAIL(target) assert(!"Unexpected match switch target")
static unsigned started, announcements, sounds, paused;
#define UNUSED(name) void name(void) { fprintf(stderr,"Unexpected: %s\n",#name); abort(); }
UNUSED(sub_00042830)
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
UNUSED(sub_000687A0)
UNUSED(sub_00098DB0)
UNUSED(sub_0009DA10)
UNUSED(sub_000C6390)
UNUSED(sub_000C63F0)
UNUSED(sub_000CBCF0)
UNUSED(sub_000D0CE0)
UNUSED(sub_000D50E0)
UNUSED(sub_00125F50)
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
void sub_00355EC0(void) { g_eax=0;g_esp+=4; }
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
void sub_0003D7B0(void) { g_eax=0x1000; g_esp+=4; }
void sub_003C85D4(void) { g_eax=0; g_esp+=4; }
void sub_0012D350(void) { announcements++; g_esp+=4; }
void sub_00164A30(void) { assert(MEM32(g_esp+4)==4+sounds); sounds++; g_esp+=4; }
void sub_002F2D30(void) { started++; g_esp+=4; }
uint32_t pc_native_visual_store(uint32_t site,uint32_t value) { (void)site;return value; }
void sub_000667F0(void) { g_esp+=4; }
void sub_00153AF0(void) { g_eax=paused;g_esp+=4; }
void sub_003533F0(void) { g_eax=0;g_esp+=4; }
UNUSED(sub_00045390)
UNUSED(sub_00045820)
UNUSED(sub_00061F30)
void sub_00064810(void) { g_esp+=4; }
UNUSED(sub_00065590)
UNUSED(sub_00066FC0)
UNUSED(sub_000674C0)
UNUSED(sub_00067CD0)
UNUSED(sub_00067E60)
UNUSED(sub_00067F00)
UNUSED(sub_00225C70)
UNUSED(sub_00227C30)
void sub_00228870(void) { g_esp+=4; }
UNUSED(sub_00231760)
UNUSED(sub_00231A20)
void sub_002382A0(void) { g_esp+=4; }
/* Mutation changes only the recovery site, retaining all other policies. */
static int legacy_recovery;
float pc_native_rocking_add(uint32_t site,float left,float right) {
    (void)site;(void)left;(void)right;assert(!"unexpected rocking policy");return 0;
}
static float observed_add(uint32_t site,float left,float right) {
    if(legacy_recovery && site==0x6552Du)return left+right;
    return RECOMP_F32_ADD_AT(site,left,right);
}
#undef RECOMP_F32_ADD_AT
#define RECOMP_F32_ADD_AT(site,left,right) observed_add(site,left,right)
/* GENERATED_ROUTINES */
enum { RECORD=0x10000,ENTITY=0x20000,STACK=0x50000 };
static void setup(unsigned char *memory) {
    memset(memory,0,0x800000);
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;MEM32(0x7de30c)=7;
    MEM32(0x7356d8)=RECORD;MEM32(RECORD)=ENTITY;
    MEMF(ENTITY+0x1d8)=10;MEMF(ENTITY+0x1d0)=0;
    MEM32(ENTITY+0x1d4)=0;MEM8(0x7de498)=0;
    for(unsigned i=0;i<10;i++)if(MEM32(0x685e4+i*4)==0x6859f)MEM8(0x7de455)=i+1;
    assert(MEM8(0x7de455));paused=0;
}
static void frame(unsigned whole,float raw,float game) {
    MEM32(0x4e7940)=MEM32(0x4e7944)=whole;
    MEMF(0x4e7948)=raw;MEMF(0x4e794c)=game;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    sub_00068830();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
    assert(MEM32(0x7de30c)==7);
    assert(MEMF(RECORD+MEM8(0x7de498)*0xea0+0x78)==MEMF(ENTITY+0x1d0));
}
int main(int argc,char **argv) {
    (void)argv;legacy_recovery=argc>1;
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    const unsigned rates[]={60,60,120,144};
    for(unsigned mode=0;mode<4;mode++) {
        unsigned fps=rates[mode];setup(memory);g_tsfp_native_timing_active=mode!=0;
        MEM32(0x7de318)=301;
        for(unsigned f=0;f<fps;f++)frame((f+1)*60/fps-f*60/fps,60.f/fps,60.f/fps);
        printf("recovery native=%u fps=%u charge=%.7f ticks=%u\n",mode!=0,fps,MEMF(ENTITY+0x1d0),MEM32(0x7de318));fflush(stdout);
        assert(fabsf(MEMF(ENTITY+0x1d0)-60*MEMF(0x478864))<.0001f);
        assert(MEM32(0x7de318)==361);
    }
    /* The real match clock delays recovery for five game seconds. Slow and
       fast time affect this delay, but not the recharge rate after it. */
    for(unsigned mode=0;mode<4;mode++) for(unsigned speed=0;speed<3;speed++) {
        unsigned fps=rates[mode],previous=0;
        float scale=speed==0?.25f:speed==1?1.f:3.f;
        setup(memory);g_tsfp_native_timing_active=mode!=0;
        for(unsigned f=0;f<fps;f++) {
            unsigned total=(unsigned)((f+1)*60.0/fps*scale+1e-6);
            frame(total-previous,60.f/fps,scale*60.f/fps);previous=total;
        }
        assert(MEMF(ENTITY+0x1d0)==0); /* At most three game seconds. */
        MEM32(0x7de318)=301;
        for(unsigned f=0;f<fps;f++) {
            unsigned total=(unsigned)((f+1)*60.0/fps*scale+1e-6);
            unsigned before=(unsigned)(f*60.0/fps*scale+1e-6);
            frame(total-before,60.f/fps,scale*60.f/fps);
        }
        assert(fabsf(MEMF(ENTITY+0x1d0)-.18f)<.0001f);
        float before=MEMF(ENTITY+0x1d0);unsigned ticks=MEM32(0x7de318);
        paused=1;
        for(unsigned f=0;f<fps;f++)frame(0,60.f/fps,0);
        assert(MEMF(ENTITY+0x1d0)==before && MEM32(0x7de318)==ticks);
        paused=0;frame(3,3,3);
        assert(fabsf(MEMF(ENTITY+0x1d0)-before-MEMF(0x478864))<.000001f);
        /* Restore exactly the boundary; recovery begins only after tick 300. */
        MEM32(0x7de318)=300;MEMF(ENTITY+0x1d0)=0;MEMF(RECORD+0x78)=0;
        frame(0,60.f/fps,0);assert(MEMF(ENTITY+0x1d0)==0);
        frame(1,60.f/fps,1);assert(MEMF(ENTITY+0x1d0)>0);
        /* Existing threshold overshoot and health-dependent clamp are kept. */
        MEMF(ENTITY+0x1d0)=9.9999f;
        frame(1,60.f/fps,1);before=MEMF(ENTITY+0x1d0);
        assert(before>=10 && before<=10+MEMF(0x478864));
        frame(1,60.f/fps,1);assert(MEMF(ENTITY+0x1d0)==before);
        MEMF(ENTITY+0x1d0)=0;MEMF(ENTITY+0x1cc)=11;
        frame(1,60.f/fps,1);assert(MEMF(ENTITY+0x1d0)==10);
    }
    /* Only the selected Escort actor and its mirrored resource are updated. */
    setup(memory);g_tsfp_native_timing_active=1;MEM32(0x7de318)=301;
    MEM8(0x7de498)=1;MEM32(RECORD+0xea0)=ENTITY;MEM32(RECORD)=0x30000;
    MEMF(0x30000+0x1d0)=5;MEMF(0x30000+0x1d8)=10;
    frame(0,.5f,.5f);
    assert(MEMF(0x30000+0x1d0)==5 && MEMF(RECORD+0x78)==0);
    assert(fabsf(MEMF(ENTITY+0x1d0)-.0015f)<.000001f);
    free(memory);puts("PASS: original Escort recovery dispatch at native rates");return 0;
}
