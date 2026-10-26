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
#include "@TIMING@"
PcNativeStep g_tsfp_native_step;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
#include "@VISUAL@"
#define RECOMP_MANUAL_TLS RECOMP_TLS
static int deny_demo_read;
static int demo_read(uint32_t address,void *out,unsigned bytes) {
    if(deny_demo_read || address<0x10000 || address>0x800000 || bytes>0x800000-address)return 0;
    memcpy(out,(const void *)((uintptr_t)address+g_xbox_mem_offset),bytes);return 1;
}
#define PC_NATIVE_DEMO_READ demo_read
#include "@DEMO@"
static unsigned polls, seeds, released, restored;
static int cancel_requested;
static unsigned device_held,device_axis;
static int recording_device;
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
    MEM32(record+12)=recording_device ? device_held<<slot : 0;
    MEM32(record+16)=MEM32(record+20)=0;
    if(recording_device)for(unsigned axis=0;axis<6;axis++)
        MEM32(record+28+axis*4)=device_axis+slot+axis;
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
    cancel_requested=recording_device=deny_demo_read=0;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x6f1e10)=0x10000;
    MEM32(0x1000c)=1000*channels*14; MEM8(0x10010)=channels;
    for(unsigned n=0;n<1000;n++) for(unsigned c=0;c<channels;c++) {
        unsigned a=0x10140+(n*channels+c)*14;
        MEM8(a)=raw; MEM8(a+1)=game;
        MEM16(a+2)=n*game; MEM16(a+4)=(n+1)*raw;
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
/* Execute the original writer, then the original initializer/reader on those
 * bytes. A synthetic pre-filled stream alone misses zero-tick records emitted
 * by the native writer. Half the recording is paused, while raw time advances.
 * File IO and the full level simulation are outside this fixture. */
static void record_roundtrips(unsigned char *memory)
{
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) for(unsigned p=0;p<sizeof rates/sizeof rates[0];p++) {
        unsigned rate=rates[r], playback=rates[p];
        setup(memory,channels,1,1);
        g_tsfp_native_timing_active=rate!=60;
        MEM32(0x6b98a4)=3;
        PcNativeTiming clock={0};
        pc_native_timing_step(&clock,0,72000000,rate,0);
        unsigned level=0,raw_clock=0;
        for(unsigned n=0;n<rate;n++) {
            PcNativeStep step=pc_native_timing_step(&clock,
                (n+1)*72000000LL/rate,72000000,rate,n>=rate/2);
            g_tsfp_native_step=step;
            MEM32(0x4e7940)=step.raw_ticks; MEM32(0x4e7944)=step.game_ticks;
            MEMF(0x4e7948)=step.raw_delta; MEMF(0x4e794c)=step.game_delta;
            MEM32(0x7de318)=level; raw_clock+=step.raw_ticks;
            MEM32(0x7497b0)=raw_clock;
            call(sub_000438B0);
            level+=step.game_ticks;
            /* Recording must not change live simulation time. */
            assert(MEM32(0x4e7940)==step.raw_ticks && MEM32(0x4e7944)==step.game_ticks);
            assert(MEMF(0x4e7948)==step.raw_delta && MEMF(0x4e794c)==step.game_delta);
        }
        unsigned bytes=MEM32(0x6b94c0)-0x10140, groups=bytes/(14*channels);
        assert(bytes && bytes%(14*channels)==0 && level==30 && raw_clock==60);
        assert(groups==60); /* Recording follows authored ticks, not render FPS. */
        for(unsigned c=0;c<channels;c++) {
            unsigned raw=0,game=0;
            for(unsigned n=0;n<groups;n++) {
                unsigned packet=0x10140+(n*channels+c)*14;
                assert(MEM8(packet)==1);
                raw+=MEM8(packet); game+=MEM8(packet+1);
            }
            assert(raw==60 && game==30); /* Check actual original writer bytes. */
        }
        MEM32(0x1000c)=bytes;
        g_tsfp_native_timing_active=playback!=60;
        call(sub_00069E80);
        clock=(PcNativeTiming){0}; pc_native_timing_step(&clock,0,72000000,playback,0);
        double raw_time=0,game_time=0,finish_time=0;
        for(unsigned n=0;n<=playback;n++) {
            PcNativeStep step=pc_native_timing_step(&clock,
                (n+1)*72000000LL/playback,72000000,playback,0);
            g_tsfp_native_step=step;
            MEM32(0x4e7940)=step.raw_ticks; MEM32(0x4e7944)=step.game_ticks;
            MEMF(0x4e7948)=step.raw_delta; MEMF(0x4e794c)=step.game_delta;
            call(sub_000438B0);
            /* Exclude the following exit UI's raw timer from stream time. */
            if(MEM32(0x6b98a4)==1) {
                raw_time+=MEMF(0x4e7948)/60.; game_time+=MEMF(0x4e794c)/60.;
            }
            /* The decoder cursor reaches the buffer end when it selects the
             * final packet, before that packet's fractional interval finishes.
             * Observe EOF on the following update, excluding the exit UI time. */
            if(!finish_time && MEM32(0x6b98a4)==2)
                finish_time=(double)n/playback;
        }
        printf("ROUNDTRIP %u %u %u %.9f %.9f %.9f\n",
            rate,playback,channels,raw_time,game_time,finish_time);
    }
}
static void record_phase_cases(unsigned char *memory)
{
    const int64_t f=72000000;
    const unsigned rates[]={120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++)
    for(unsigned channels=1;channels<=4;channels+=3) {
        unsigned rate=rates[r],resume=44;
        /* Keep raw/game phases offset after pause at every tested rate. */
        while(((resume-17)*60)%rate==0)++resume;
        setup(memory,channels,1,1); MEM32(0x6b98a4)=3;
        g_tsfp_native_timing_active=1; recording_device=1;
        PcNativeTiming clock={0}; pc_native_timing_step(&clock,0,f,rate,0);
        int64_t now=0;unsigned raw_total=0,game_total=0,recorded_game=0;
        unsigned groups=0,partial_game_frames=0;
        call(sub_000438B0); /* Startup's zero-length update writes no packet. */
        assert(MEM32(0x6b94c0)==0x10140 && MEM32(0x6b98a4)==3);
        for(unsigned n=0;n<=rate;n++) {
            /* Pause away from an authored boundary; include a three-tick stall
             * and finish on a raw boundary to flush the last pending count. */
            int64_t units=n==rate?(f-clock.raw_remainder)%f:(n==80?3*f:60*f/rate);
            now+=units/60;
            PcNativeStep step=pc_native_timing_step(&clock,now,f,rate,n>=17 && n<resume);
            MEM32(0x4e7940)=step.raw_ticks; MEM32(0x4e7944)=step.game_ticks;
            MEMF(0x4e7948)=step.raw_delta; MEMF(0x4e794c)=step.game_delta;
            MEM32(0x7de318)=game_total;
            raw_total+=step.raw_ticks;game_total+=step.game_ticks;
            MEM32(0x7497b0)=raw_total;
            if(!step.raw_ticks && step.game_ticks)partial_game_frames++;
            device_held=1+(groups&1);device_axis=32+n%64;
            unsigned before=MEM32(0x6b94c0);
            call(sub_000438B0);
            assert(polls==(n+2)*4);
            assert(MEM32(0x4e7944)==step.game_ticks && MEMF(0x4e794c)==step.game_delta);
            if(!step.raw_ticks)assert(MEM32(0x6b94c0)==before);
            else {
                assert(MEM32(0x6b94c0)==before+14*channels);
                for(unsigned c=0;c<channels;c++) {
                    unsigned packet=before+c*14;
                    assert(MEM8(packet)==step.raw_ticks);
                    assert(MEM8(packet+1)==game_total-recorded_game);
                    assert(MEM16(packet+2)==recorded_game);
                    assert(MEM16(packet+4)==raw_total);
                    assert(MEM16(packet+6)==device_held<<c);
                    for(unsigned axis=0;axis<6;axis++)
                        assert(MEM8(packet+8+axis)==device_axis+c+axis);
                }
                recorded_game=game_total;groups++;
            }
        }
        assert(partial_game_frames>0 && recorded_game==game_total);
        printf("RECORD_PHASE %u %u %u\n",rate,channels,partial_game_frames);
        /* Pending game ticks survive a live return to Original60. */
        MEM32(0x7de318)=game_total;
        MEM32(0x4e7940)=0; MEM32(0x4e7944)=1;
        MEMF(0x4e7948)=MEMF(0x4e794c)=0.4f;
        unsigned before=MEM32(0x6b94c0);call(sub_000438B0);
        assert(MEM32(0x6b94c0)==before);
        MEM32(0x7de318)=game_total+1;
        MEM32(0x4e7940)=MEM32(0x4e7944)=1;
        MEMF(0x4e7948)=MEMF(0x4e794c)=1.f;
        g_tsfp_native_timing_active=0;call(sub_000438B0);
        for(unsigned c=0;c<channels;c++) {
            assert(MEM8(before+c*14+1)==2);
            assert(MEM16(before+c*14+2)==game_total);
        }
        /* A same-buffer restart discards pending ticks even if no record was
         * written since initialization (cursor identity alone cannot tell). */
        setup(memory,channels,1,1);g_tsfp_native_timing_active=1;
        MEM32(0x6b98a4)=3;MEM32(0x4e7940)=0;MEM32(0x4e7944)=1;
        MEMF(0x4e7948)=MEMF(0x4e794c)=0.4f;
        call(sub_000438B0);assert(MEM32(0x6b94c0)==0x10140);
        call(sub_00069E80);MEM32(0x6b98a4)=3;
        MEM32(0x7de318)=1;MEM32(0x4e7940)=1;MEM32(0x4e7944)=0;
        call(sub_000438B0);
        for(unsigned c=0;c<channels;c++) {
            assert(MEM8(0x10140+c*14+1)==0);
            assert(MEM16(0x10140+c*14+2)==1);
        }
    }
    setup(memory,4,1,1);g_tsfp_native_timing_active=1;
    MEM32(0x6b98a4)=3;MEM32(0x6b98a0)=42;
    MEM32(0x4e7940)=0;MEM32(0x4e7944)=1;
    MEMF(0x4e7948)=MEMF(0x4e794c)=0.4f;
    call(sub_000438B0);
    MEM32(0x7de318)=1;MEM32(0x4e7940)=1;MEM32(0x4e7944)=0;
    memset(memory+0x10140+28,0xcd,28);
    call(sub_000438B0);
    assert(MEM32(0x6b98a4)==4 && MEM32(0x6b94c0)==0x10140+28);
    for(unsigned c=0;c<2;c++) {
        assert(MEM8(0x10140+c*14+1)==1);
        assert(MEM16(0x10140+c*14+2)==0);
    }
    for(unsigned n=0;n<28;n++)assert(MEM8(0x10140+28+n)==0xcd);
}
static void publish(PcNativeStep step)
{
    g_tsfp_native_step=step;
    MEM32(0x4e7940)=step.raw_ticks;MEM32(0x4e7944)=step.game_ticks;
    MEMF(0x4e7948)=step.raw_delta;MEMF(0x4e794c)=step.game_delta;
}
/* Target refresh is not delivered cadence: exercise the reported 110-update
 * load and a fluctuating 60..240 cadence without skipping recorded edges. */
static void playback_variable_cadence(unsigned char *memory)
{
    const int64_t f=79200000; /* Exact intervals for every rate below. */
    const unsigned cadence[]={240,110,60,144,180,120};
    for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned pattern=0;pattern<2;pattern++) {
        setup(memory,channels,1,1);g_tsfp_native_timing_active=1;
        PcNativeTiming clock={0};int64_t now=0;
        pc_native_timing_step(&clock,0,f,240,0);
        double elapsed=0;unsigned previous=0;
        for(unsigned n=0;n<360;n++) {
            unsigned rate=pattern?cadence[n%6]:110;
            now+=f/rate;
            publish(pc_native_timing_step(&clock,now,f,240,0));
            call(sub_000438B0);
            unsigned selected=(MEM32(0x6b94c0)-0x10140)/(14*channels);
            assert(selected>=previous && selected<=previous+1);
            for(unsigned c=0;c<channels;c++) {
                unsigned record=0x6b93d0+c*0x3c;
                unsigned held=(((selected-1)&1)?1u:2u)<<c;
                unsigned prior=previous?(((previous-1)&1)?1u:2u)<<c:0;
                assert(MEM32(record+12)==held);
                assert(MEM32(record+16)==(held&~prior));
                assert(MEM32(record+20)==(prior&~held));
            }
            elapsed+=MEMF(0x4e794c);
            assert(fabs(elapsed-(double)now*60/f)<0.0001);
            assert(!pc_native_demo_playback.debt);
            previous=selected;
        }
        printf("VARIABLE_CADENCE %u %u %.9f %.9f\n",channels,pattern,
               (double)now/f,elapsed/60);
    }
}

static unsigned check_playback_edges(unsigned channels,unsigned previous)
{
    unsigned selected=(MEM32(0x6b94c0)-0x10140)/(14*channels);
    assert(selected>=previous && selected<=previous+1);
    for(unsigned c=0;c<channels;c++) {
        unsigned record=0x6b93d0+c*0x3c;
        unsigned held=selected?(((selected-1)&1)?1u:2u)<<c:0;
        unsigned prior=previous?(((previous-1)&1)?1u:2u)<<c:0;
        assert(MEM32(record+12)==held);
        assert(MEM32(record+16)==(held&~prior));
        assert(MEM32(record+20)==(prior&~held));
    }
    return selected;
}

static void playback_lifecycle(unsigned char *memory)
{
    const int64_t f=72000000;
    const unsigned rates[]={120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++)
    for(unsigned p=0;p<sizeof rates/sizeof rates[0];p++)
    for(unsigned channels=1;channels<=4;channels+=3) {
        unsigned rate=rates[r],playback=rates[p];
        setup(memory,channels,1,1);g_tsfp_native_timing_active=1;
        PcNativeTiming clock={0};int64_t now=0;
        publish(pc_native_timing_step(&clock,now,f,rate,0));call(sub_000438B0);
        assert(MEM32(0x6b94c0)==0x10140 && MEM32(0x6b98a4)==1);
        double elapsed=0;unsigned previous=0;
        for(unsigned n=0;n<rate;n++) {
            /* One bounded stall, then normal pacing repays it without dropping
             * any intervening packet. Include repeated zero-time polling. */
            now+=n==7?f/20:f/rate;
            publish(pc_native_timing_step(&clock,now,f,rate,0));
            call(sub_000438B0);
            unsigned selected=(MEM32(0x6b94c0)-0x10140)/(14*channels);
            assert(selected>=previous && selected<=previous+1);
            for(unsigned c=0;c<channels;c++) {
                unsigned record=0x6b93d0+c*0x3c;
                unsigned held=(((selected-1)&1)?1:2)<<c;
                unsigned prior=previous?(((previous-1)&1)?1:2)<<c:0;
                assert(MEM32(record+12)==held);
                assert(MEM32(record+16)==(held&~prior));
                assert(MEM32(record+20)==(prior&~held));
            }
            assert(fabs(pc_native_visual_sample(MEM32(0x7de318))-elapsed)<0.0001);
            elapsed+=MEMF(0x4e794c);
            assert(MEM32(0x7497b0)==(uint32_t)floor(elapsed+0.00001));
            MEM32(0x7de318)=pc_native_visual_store(0x688bb,MEM32(0x7de318)+MEM32(0x4e7944));
            assert(fabs(pc_native_visual_sample(MEM32(0x7de318))-elapsed)<0.0001);
            previous=selected;
            publish(pc_native_timing_step(&clock,now,f,rate,0));call(sub_000438B0);
            assert(MEM32(0x6b94c0)==0x10140+selected*14*channels);
            assert(MEMF(0x4e7948)==0 && MEMF(0x4e794c)==0);
            for(unsigned c=0;c<channels;c++) {
                unsigned record=0x6b93d0+c*0x3c;
                assert(MEM32(record+12)==((((selected-1)&1)?1u:2u)<<c));
                assert(!MEM32(record+16) && !MEM32(record+20));
            }
        }
        assert(fabs(elapsed-(double)now*60/f)<0.0001);
        assert(!pc_native_demo_playback.debt);
        /* Preserve the fractional stream position through native->60->native,
         * including the zero-length first update of the new native clock. */
        for(unsigned n=0;n<5;n++) {
            g_tsfp_native_timing_active=0;g_tsfp_native_step=(PcNativeStep){0};
            MEM32(0x4e7940)=MEM32(0x4e7944)=1;
            MEMF(0x4e7948)=MEMF(0x4e794c)=1.f;call(sub_000438B0);
            previous=check_playback_edges(channels,previous);
            elapsed+=MEMF(0x4e794c);assert(MEMF(0x4e794c)==1.f);
        }
        g_tsfp_native_timing_active=1;clock=(PcNativeTiming){0};
        publish(pc_native_timing_step(&clock,now,f,playback,0));call(sub_000438B0);
        previous=check_playback_edges(channels,previous);
        assert(MEMF(0x4e794c)==0);
        for(unsigned n=0;n<playback;n++) {
            now+=f/playback;publish(pc_native_timing_step(&clock,now,f,playback,0));
            call(sub_000438B0);
            previous=check_playback_edges(channels,previous);
            assert(fabs(pc_native_visual_sample(MEM32(0x7de318))-elapsed)<0.0001);
            elapsed+=MEMF(0x4e794c);
        }
        assert(fabs(elapsed-(128.0-60.0/rate))<0.0001);
        /* Cancellation after the scheduler is active still restores native
         * raw time, and the callback may reset demo mode to normal play. */
        cancel_requested=1;now+=f/playback;
        PcNativeStep input=pc_native_timing_step(&clock,now,f,playback,0);publish(input);
        call(sub_000438B0);
        assert(MEM32(0x6b98a4)==0 && !pc_native_demo_playback.valid);
        assert(MEMF(0x4e7948)==input.raw_delta && MEMF(0x4e794c)==0);
        assert(g_tsfp_native_step.game_delta==0 && g_tsfp_native_step.game_ticks==0 &&
               g_tsfp_native_step.game_units==0);
        assert(g_tsfp_native_step.raw_units==input.raw_units &&
               g_tsfp_native_step.raw_phase==input.raw_phase &&
               g_tsfp_native_step.game_phase==input.game_phase);
        printf("LIFECYCLE %u %u %u %.9f\n",rate,playback,channels,elapsed);
    }
    /* An inaccessible or truncated controller group reaches original EOF,
     * never a speculative decode/read beyond the validated buffer. */
    for(unsigned denied=0;denied<2;denied++) {
        setup(memory,4,1,1);g_tsfp_native_timing_active=1;
        PcNativeTiming clock={0};pc_native_timing_step(&clock,0,f,144,0);
        publish(pc_native_timing_step(&clock,f/144,f,144,0));
        if(denied)deny_demo_read=1;else MEM32(0x6b98a0)=14;
        call(sub_000438B0);
        assert(MEM32(0x6b98a4)==2 && MEMF(0x4e794c)==0);
        assert(g_tsfp_native_step.game_delta==0 && g_tsfp_native_step.game_ticks==0 &&
               g_tsfp_native_step.game_units==0);
    }
}
static void playback_old_native(unsigned char *memory)
{
    const unsigned rates[]={60,120,144,180,240};const int64_t f=72000000;
    for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned recorded=120;recorded<=144;recorded+=24)
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) for(unsigned offset=0;offset<2;offset++) {
        unsigned rate=rates[r];setup(memory,channels,1,1);
        unsigned raw_total=0,game_total=0;
        for(unsigned n=0;n<recorded;n++) {
            unsigned raw=(n+1)*60/recorded-n*60/recorded;
            unsigned game=offset?!raw:(n<recorded/2?raw:0);
            unsigned level=game_total;raw_total+=raw;game_total+=game;
            for(unsigned c=0;c<channels;c++) {
                unsigned packet=0x10140+(n*channels+c)*14;
                MEM8(packet)=raw;MEM8(packet+1)=game;
                MEM16(packet+2)=level;MEM16(packet+4)=raw_total;
                MEM16(packet+6)=(raw_total?(raw_total&1?1:2):0)<<c;
            }
        }
        MEM32(0x1000c)=recorded*channels*14;call(sub_00069E80);
        g_tsfp_native_timing_active=rate!=60;
        PcNativeTiming clock={0};pc_native_timing_step(&clock,0,f,rate,0);
        double raw_time=0,game_time=0;
        for(unsigned n=0;n<rate;n++) {
            publish(pc_native_timing_step(&clock,(n+1)*f/rate,f,rate,0));
            call(sub_000438B0);
            if(rate!=60)assert(fabs(pc_native_visual_sample(MEM32(0x7de318))-game_time)<0.0001);
            raw_time+=MEMF(0x4e7948);game_time+=MEMF(0x4e794c);
            assert(MEM32(0x7497b0)==(uint32_t)floor(raw_time+0.0001));
            unsigned held=((n*60/rate)&1)?2:1;
            for(unsigned c=0;c<channels;c++)assert(MEM32(0x6b93dc+c*0x3c)==held<<c);
        }
        assert(fabs(raw_time-60)<0.0001 && fabs(game_time-game_total)<0.0001);
        assert(MEM32(0x6b94c0)==0x10140+recorded*channels*14);
    }
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000); assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)memory;
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned k=0;k<sizeof rates/sizeof rates[0];k++) for(unsigned channels=1;channels<=4;channels+=3)
    for(unsigned raw=1;raw<=2;raw++) for(unsigned game=0;game<=1;game++) {
        unsigned rate=rates[k], previous=0; double raw_time=0,game_time=0;
        setup(memory,channels,raw,game); g_tsfp_native_timing_active=k!=0;
        PcNativeTiming clock={0};pc_native_timing_step(&clock,0,72000000,rate,0);
        for(unsigned n=0;n<rate;n++) {
            g_tsfp_native_step=pc_native_timing_step(&clock,(n+1)*72000000LL/rate,72000000,rate,0);
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
    for(unsigned k=0;k<sizeof rates/sizeof rates[0];k++) for(unsigned channels=1;channels<=4;channels+=3)
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
    for(unsigned k=0;k<sizeof rates/sizeof rates[0];k++) for(unsigned channels=1;channels<=4;channels+=3)
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
    record_roundtrips(memory);
    record_phase_cases(memory);
    playback_lifecycle(memory);
    playback_variable_cadence(memory);
    playback_old_native(memory);
    free(memory);
    puts("PASS: original initializer, input dispatcher, controller bytes/edges, ABI and EOF; timing checked separately");
    return 0;
}
