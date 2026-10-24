/* Original initialization + input poll + demo dispatcher. Synthetic recordings
 * and controlled device/lifecycle services; no game assets or generated code.
 * Timing acceptance is asserted in Python so only known mismatches xfail. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#define RECOMP_NUMERIC_POLICY_HEADER "@NUMERIC@"
#include "recomp_types.h"
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(address) assert(!"Unexpected demo switch target")
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
#include "@VISUAL@"
#define RECOMP_MANUAL_TLS RECOMP_TLS
#include "@DEMO@"
static unsigned polls, seeds, released, restored;
static int cancel_requested;
static void inert(void) { g_eax=0; g_esp+=4; }
void sub_00190340(void) { inert(); }
void sub_00190520(void) { g_eax=cancel_requested; g_esp+=4; }
void sub_0035E960(void) { inert(); }
void sub_001CCE70(void) { inert(); }
void sub_001CCF80(void) { inert(); }
void sub_00153B80(void) { assert(MEM32(g_esp+4)==1); ++seeds; inert(); }
void sub_000389B0(void) { assert(MEM32(g_esp+4)<4); ++released; inert(); }
/* The real service reaches level/frontend initialization via 65470/668F0.
 * Model a mode-changing callback boundary so restoration cannot rely on mode2
 * still being published afterward; this is not live cancellation coverage. */
void sub_00069ED0(void) { ++restored; MEM32(0x6b98a4)=0; inert(); }
void sub_00150230(void) { g_eax=1; g_esp+=4; }
void sub_0018FF70(void) {
    unsigned record=MEM32(g_esp+4), slot=MEM32(g_esp+8);
    assert(slot<4 && record==0x6b93d0+slot*0x3c);
    MEM32(record+8)=MEM32(record+12);
    MEM32(record+12)=0; /* Neutral physical device, retaining prior playback held. */
    MEM32(record+16)=MEM32(record+20)=0;
    ++polls; inert();
}
#define UNEXPECTED(address) void sub_##address(void) { assert(!"Unexpected service " #address); }
UNEXPECTED(00042640)
UNEXPECTED(00069620)
UNEXPECTED(00069E60)
UNEXPECTED(001CDAE0)
UNEXPECTED(00356800)
void sub_00042510(void); void sub_000431F0(void);
void sub_00043220(void); void sub_000438B0(void);
void sub_00069E70(void); void sub_00069E80(void);
void sub_000425A0(void); void sub_00069EA0(void);
void sub_00043220_gen(void);
void sub_00043220(void) { pc_native_demo_guest(sub_00043220_gen); }
/* ORIGINAL_CODE */

static void call(void (*function)(void)) {
    g_esp=0x7000; MEM32(g_esp)=0xdeadbeef;
    g_ebx=11; g_esi=22; g_edi=33; g_ebp=g_seh_ebp=44;
    function();
    if(g_esp!=0x7004 || g_ebx!=11 || g_esi!=22 || g_edi!=33 || g_ebp!=44)
        fprintf(stderr,"ABI init=%d stack=%x saved=%u/%u/%u/%u\n",
            function==sub_00069E80,g_esp,g_ebx,g_esi,g_edi,g_ebp);
    assert(g_esp==0x7004 && g_ebx==11 && g_esi==22 && g_edi==33 && g_ebp==44);
}
static void setup(unsigned char *memory,unsigned channels,unsigned raw,unsigned game) {
    memset(memory,0,0x800000); polls=seeds=released=restored=0;
    cancel_requested=0;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x6f1e10)=0x10000;
    MEM32(0x1000c)=1000*channels*14; MEM8(0x10010)=channels;
    for(unsigned n=0;n<1000;n++) for(unsigned c=0;c<channels;c++) {
        unsigned a=0x10140+(n*channels+c)*14;
        MEM8(a)=raw; MEM8(a+1)=game;
        MEM16(a+2)=n*game; MEM16(a+4)=n*raw;
        MEM16(a+6)=((n&1)?1:2)<<c;
        for(unsigned axis=0;axis<6;axis++) MEM8(a+8+axis)=32+c*7+axis;
    }
    for(unsigned c=0;c<4;c++) MEM32(0x6b93c0+c*4)=c;
    MEM32(0x6b9888)=1; /* Suppress human cancellation for this controlled stream. */
    MEM32(0x79094c)=39;
    MEM32(0x4e8cec)=60;
    call(sub_00069E80);
    assert(seeds==1 && MEM32(0x6b98a4)==1 && MEM32(0x6b98a8)==channels);
    assert(MEM32(0x6b94c0)==0x10140 && MEM32(0x6b98ac)==0);
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000); assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)memory;
    const unsigned rates[]={60,120,144};
    for(unsigned k=0;k<3;k++) for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned raw=1;raw<=2;raw++) for(unsigned game=0;game<=1;game++) {
        unsigned rate=rates[k], previous=0; double raw_time=0,game_time=0;
        setup(memory,channels,raw,game); g_tsfp_native_timing_active=k!=0;
        for(unsigned n=0;n<rate;n++) {
            unsigned ticks=(n+1)*60/rate-n*60/rate;
            MEM32(0x4e7940)=MEM32(0x4e7944)=ticks;
            MEMF(0x4e7948)=MEMF(0x4e794c)=60.f/rate;
            call(sub_000438B0);
            assert(MEM32(0x6b98a4)==1);
            unsigned bytes=MEM32(0x6b94c0)-0x10140, consumed=bytes/(14*channels);
            assert(bytes%(14*channels)==0 && consumed>=previous && consumed<=previous+1);
            for(unsigned c=0;c<channels;c++) {
                unsigned record=0x6b93d0+c*0x3c;
                unsigned held=consumed?(((consumed-1)&1)?1:2)<<c:0;
                unsigned prior=previous?(((previous-1)&1)?1:2)<<c:0;
                assert(MEM32(record+12)==held);
                assert(MEM32(record+16)==(held&~prior));
                assert(MEM32(record+20)==(prior&~held));
                for(unsigned axis=0;consumed && axis<6;axis++)
                    assert(MEM32(record+28+axis*4)==32+c*7+axis);
            }
            previous=consumed;
            raw_time+=MEMF(0x4e7948)/60.; game_time+=MEMF(0x4e794c)/60.;
        }
        assert(polls==rate*4 && seeds==1);
        printf("RESULT %u %u %u %u %.9f %.9f\n",rate,channels,raw,game,raw_time,game_time);
    }
    setup(memory,1,1,1);
    MEM32(0x79094c)=101; call(sub_000438B0);
    assert(MEM32(0x6b94c0)==0x10140); /* Menu scene gate remains original. */
    MEM32(0x79094c)=39; MEM32(0x6b98a0)=14;
    call(sub_000438B0); assert(MEM32(0x6b98a4)==1);
    MEM32(0x4e7940)=0; MEMF(0x4e7948)=60.f/144;
    call(sub_000438B0); assert(MEM32(0x6b98a4)==2 && MEMF(0x4e794c)==0);
    assert(MEM32(0x4e7940)==0 && MEMF(0x4e7948)==60.f/144);
    for(unsigned k=0;k<3;k++) for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned mode=2;mode<=5;mode+=3) for(unsigned scale=1;scale<=2;scale++) {
        unsigned rate=rates[k];
        setup(memory,channels,1,1); g_tsfp_native_timing_active=k!=0;
        MEM32(0x6b98a4)=mode; MEMF(0x6b98b0)=10.f;
        for(unsigned n=0;n<rate;n++) {
            unsigned ticks=(n+1)*60*scale/rate-n*60*scale/rate;
            float delta=60.f*scale/rate;
            MEM32(0x4e7940)=MEM32(0x4e7944)=ticks;
            MEMF(0x4e7948)=MEMF(0x4e794c)=delta;
            call(sub_000438B0);
            assert(MEM32(0x6b98a4)==mode);
            assert(MEMF(0x4e794c)==(mode==2?0.f:delta));
            assert(MEMF(0x4e7948)==(mode==2 && !k?1.f:delta));
            assert(MEM32(0x4e7940)==(mode==2 && !k?1u:ticks));
            assert(pc_native_demo_timer_multiply(0x43473u,3.f,4.f)==12.f);
        }
        /* Original state2 changes raw delta to one after controller 0, so
         * preserve that legacy multi-controller/multi-tick behavior at60. */
        float expected=10.f-(mode==2 && !k ? (float)(scale+channels-1) : (float)(scale*channels));
        assert(fabsf(MEMF(0x6b98b0)-expected)<0.0002f);
    }
    for(unsigned mode=2;mode<=5;mode+=3) {
        setup(memory,4,1,1); g_tsfp_native_timing_active=1;
        MEM32(0x6b98a4)=mode; MEMF(0x6b98b0)=10.f;
        MEM32(0x4e7940)=0; MEMF(0x4e7948)=0.f;
        call(sub_000438B0);
        assert(MEMF(0x6b98b0)==10.f && MEMF(0x4e7948)==0.f);
    }
    setup(memory,1,1,1); g_tsfp_native_timing_active=1; cancel_requested=1;
    MEM32(0x4e7940)=0; MEMF(0x4e7948)=60.f/144;
    call(sub_000438B0);
    assert(restored==1 && MEM32(0x6b98a4)==0);
    assert(MEM32(0x4e7940)==0 && MEMF(0x4e7948)==60.f/144);
    assert(MEMF(0x4e794c)==0.f);
    for(unsigned k=0;k<3;k++) for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned mode=2;mode<=5;mode+=3) {
        unsigned rate=rates[k], frames=0;
        setup(memory,channels,1,1); g_tsfp_native_timing_active=k!=0;
        MEM32(0x6b98a4)=mode; MEMF(0x6b98b0)=0.5f;
        while((mode==2?MEM32(0x6b98ac)==0:MEM32(0x6b98a4)==5) && frames<rate) {
            MEM32(0x4e7940)=(frames+1)*60/rate-frames*60/rate;
            MEMF(0x4e7948)=60.f/rate;
            call(sub_000438B0); ++frames;
        }
        assert(fabs((double)frames/rate-0.5/channels)<1.01/rate);
        if(mode==2) assert(restored==1 && released==channels && seeds==2);
        else assert(MEM32(0x6b98a4)==4 && MEM32(0x1000c)==0);
    }
    free(memory);
    puts("PASS: original initializer, input dispatcher, controller bytes/edges, ABI and EOF; timing checked separately");
    return 0;
}
