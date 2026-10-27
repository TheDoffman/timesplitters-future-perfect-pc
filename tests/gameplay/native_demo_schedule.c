/* Asset-free timing/read-boundary contracts; original decoding/ABI is covered
 * separately by native_demo_playback.c with the local supported executable. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _MSC_VER
#define RECOMP_MANUAL_TLS __declspec(thread)
#else
#define RECOMP_MANUAL_TLS _Thread_local
#endif
static unsigned char *memory;
static ptrdiff_t g_xbox_mem_offset;
static int g_tsfp_native_timing_active,denied;
static unsigned reads,calls;
static double visual;
static int read_packet(uint32_t address,void *out,unsigned bytes)
{
    reads++;
    if(denied || address<0x10000 || address>0x800000 || bytes>0x800000-address)return 0;
    memcpy(out,memory+address,bytes);return 1;
}
#define PC_NATIVE_DEMO_READ read_packet
void pc_native_visual_rebase(uint32_t value,double fraction) { visual=value+fraction; }
#include "../src/gameplay/pc_native_demo_guest.inc"
PcNativeStep g_tsfp_native_step;
#define WORD(a) (*(uint32_t *)(memory+(a)))
#define FLOAT(a) (*(float *)(memory+(a)))
static void decoder(void)
{
    calls++;
    if(pc_native_demo_dispatch_mode(WORD(0x6b98a4))!=1)return;
    uint32_t cursor=WORD(0x6b94c0);
    if(cursor>=WORD(0x6b989c)+WORD(0x6b98a0)) {
        WORD(0x6b98a4)=2;
        WORD(0x4e7940)=1;FLOAT(0x4e7948)=1;
        WORD(0x4e7944)=0;FLOAT(0x4e794c)=0;return;
    }
    /* Stub only the original decoder boundary; the real shared adapter must
     * derive all fractional output from the stream and exact incoming clock. */
    WORD(0x6b94c0)=cursor+14*WORD(0x6b98a8);
    WORD(0x4e7940)=memory[cursor];FLOAT(0x4e7948)=memory[cursor];
    WORD(0x4e7944)=memory[cursor+1];FLOAT(0x4e794c)=memory[cursor+1];
}
static void setup(unsigned channels)
{
    memset(memory,0,0x800000);reads=calls=0;denied=0;
    WORD(0x6b989c)=0x10000;WORD(0x6b98a0)=60*channels*14;
    WORD(0x6b98a8)=channels;WORD(0x6b98a4)=1;
    WORD(0x6b94c0)=pc_native_demo_cursor_reset(0x10000);
    for(unsigned c=0;c<4;c++)WORD(0x6b93c0+c*4)=c;
    for(unsigned n=0;n<60;n++)for(unsigned c=0;c<channels;c++) {
        unsigned char *p=memory+0x10000+(n*channels+c)*14;
        p[0]=1;p[1]=!(n&1);p[2]=(unsigned char)((n+1)/2);p[4]=(unsigned char)(n+1);
    }
}
static void publish(PcNativeStep step)
{
    g_tsfp_native_step=step;
    WORD(0x4e7940)=step.raw_ticks;WORD(0x4e7944)=step.game_ticks;
    FLOAT(0x4e7948)=step.raw_delta;FLOAT(0x4e794c)=step.game_delta;
}
/* Integral of an independent one-tick-on/one-tick-off game clock. */
static double expected(unsigned frame,unsigned rate)
{
    unsigned tick=frame*60/rate,part=frame*60%rate;
    return (tick+1)/2+((tick&1)?0:(double)part/rate);
}
int main(void)
{
    memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    const int64_t f=72000000;
    const unsigned rates[]={120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++)for(unsigned channels=1;channels<=4;channels+=3) {
        unsigned rate=rates[r];
        setup(channels);g_tsfp_native_timing_active=1;
        PcNativeTiming clock={0};publish(pc_native_timing_step(&clock,0,f,rate,0));
        pc_native_demo_guest(decoder);
        assert(WORD(0x6b94c0)==0x10000 && !FLOAT(0x4e794c));
        double game=0,raw=0;
        for(unsigned n=0;n<rate;n++) {
            publish(pc_native_timing_step(&clock,(n+1)*f/rate,f,rate,0));
            pc_native_demo_guest(decoder);
            assert(fabs(visual-expected(n,rate))<0.00001);
            game+=FLOAT(0x4e794c);raw+=FLOAT(0x4e7948);
            assert(fabs(game-expected(n+1,rate))<0.00001);
            assert(WORD(0x7497b0)==(n+1)*60/rate);
            assert(g_tsfp_native_step.game_ticks==WORD(0x4e7944));
            assert(g_tsfp_native_step.game_delta==FLOAT(0x4e794c));
        }
        assert(fabs(game-30)<0.00001 && fabs(raw-60)<0.00001 && calls==rate+1);
        publish(pc_native_timing_step(&clock,f+f/rate,f,rate,0));
        float delta=FLOAT(0x4e7948);pc_native_demo_guest(decoder);
        assert(WORD(0x6b98a4)==2 && FLOAT(0x4e794c)==0 && FLOAT(0x4e7948)==delta);
        assert(g_tsfp_native_step.game_delta==0 && g_tsfp_native_step.game_ticks==0 &&
               g_tsfp_native_step.game_units==0);
        assert(g_tsfp_native_step.raw_delta==delta &&
               g_tsfp_native_step.raw_ticks==WORD(0x4e7940));
        /* A permission change must be checked after successful prior reads. */
        setup(channels);clock=(PcNativeTiming){0};pc_native_timing_step(&clock,0,f,rate,0);
        publish(pc_native_timing_step(&clock,f/rate,f,rate,0));pc_native_demo_guest(decoder);
        unsigned before=reads;denied=1;
        publish(pc_native_timing_step(&clock,2*f/rate,f,rate,0));pc_native_demo_guest(decoder);
        assert(reads>before && WORD(0x6b98a4)==2);
    }
    setup(1);memset(memory+0x10000,0,60*14);
    PcNativeTiming clock={0};pc_native_timing_step(&clock,0,f,144,0);
    publish(pc_native_timing_step(&clock,f/144,f,144,0));pc_native_demo_guest(decoder);
    assert(WORD(0x6b98a4)==2 && reads<=128);
    free(memory);puts("PASS: shared playback clocks, zero startup, EOF and fresh bounded reads");return 0;
}
