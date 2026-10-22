/* Compose the production native clock and movement adapters. Host QPC/ISR,
 * packet encoder and sockets are controlled boundaries, not live peers. */
static void observe_network_counter(void);
#define TIMING_COUNTER_OBSERVER() observe_network_counter()
#define main native_timing_baseline_main
#include "native_timing_guest.c"
#undef main
#include "../src/pc_network.h"
static uint32_t g_eax;
static int network_enabled=1;
int pc_network_enabled(void) { return network_enabled; }
unsigned pc_network_arguments(uint32_t address) { (void)address;return 0; }
uint32_t pc_network_call(uint32_t address,const uint32_t *args,ptrdiff_t offset)
{ (void)address;(void)args;(void)offset;return 0; }
int pc_network_latency(double *ms,unsigned *peers) { *ms=-1;*peers=0;return 1; }
void pc_debug_latency(int mode,double ms,unsigned peers)
{ (void)mode;(void)ms;(void)peers; }
/* Exercise the existing Windows adapter on either host compiler; this does
 * not supply a Mac transport. Its fallback has its own dispatch fixture. */
#define TSFP_PC_DEBUG_H
#ifndef _WIN32
#define TSFP_TEST_FORCED_WINDOWS
#define _WIN32 1
#endif
#include "../src/pc_network_guest.inc"
#ifdef TSFP_TEST_FORCED_WINDOWS
#undef _WIN32
#endif

static unsigned sends,refreshes[4],pump_calls,sample_calls;
static uint32_t isr_base;
static void observe_network_counter(void)
{ WORD(0x563918)=isr_base+(uint32_t)(counter/2400000); }
static void pump(void)
{
    uint32_t epoch=WORD(0x76b0f0);
    if((int32_t)epoch>(int32_t)WORD(0x76b0ec)) {
        sends++;WORD(0x76b0ec)=epoch;
    }
    pump_calls++;g_eax=0x1234;g_esp+=4;
}
static void sampler(void) { sample_calls++;g_eax=0x5678;g_esp+=4; }
static void clock_frame(int64_t now)
{
    counter=now;
    g_esp=0x1000;WORD(g_esp)=0xabcdef12;
    pc_native_timing_guest(original);
    assert(g_esp==0x1004 && WORD(0x1000)==0xabcdef12);
    uint32_t isr=isr_base+(uint32_t)(counter/2400000);
    assert(WORD(0x563918)==isr && WORD(0x7497a8)==isr);
    WORD(0x7de318)+=WORD(0x4e7944);
}
static void network_frame(void)
{
    uint32_t isr=WORD(0x7497a8),ticks=WORD(0x7de318);
    unsigned before=sends;
    g_esp=0x1000;pc_network_guest_tick(pump);
    assert(g_esp==0x1004 && g_eax==0x1234 && sends<=before+1);
    before=sends;
    g_esp=0x1000;pc_network_guest_tick(pump);
    assert(g_esp==0x1004 && sends==before);
    g_esp=0x1000;WORD(g_esp+4)=0x2000;pc_network_guest_sample(sampler);
    assert(g_esp==0x1004 && g_eax==0x5678);
    for(unsigned port=0;port<4;port++) {
        uint32_t record=0x3000+port*0x200;
        if(WORD(record+0x100)) {
            refreshes[port]++;
            /* Successful original encoding commits its full-state timestamp. */
            WORD(record+0x10c)=ticks;WORD(record+0x100)=0;
        }
    }
    assert(WORD(0x7497a8)==isr && WORD(0x7de318)==ticks);
}
static void reset(int64_t start,const char *rate,uint32_t origin)
{
    setting="0";g_esp=0x1000;pc_native_timing_guest(original);
    memset(memory,0,sizeof memory);WORD(0x4e8cec)=60;
    network_enabled=0;g_esp=0x1000;pc_network_guest_tick(pump);
    network_enabled=1;WORD(0x76b1a4)=15;WORD(0x76b0f0)=WORD(0x76b0ec)=20;
    WORD(0x2020)=4;
    for(unsigned port=0;port<4;port++) {
        WORD(0x200c+port*4)=0x3000+port*0x200;
        WORD(0x310c+port*0x200)=origin;
    }
    WORD(0x7de318)=origin;setting=rate;
    isr_base=origin-(uint32_t)(start/2400000);clock_frame(start);
    assert(WORD(0x7497a8)==origin);
    network_frame(); /* Initial movement send is independent of render rate. */
    sends=pump_calls=sample_calls=0;memset(refreshes,0,sizeof refreshes);
}
static void regular_rates(void)
{
    const char *rates[]={"60","120","144"};
    for(unsigned wrap=0;wrap<2;wrap++) for(unsigned r=0;r<3;r++) {
        int rate=atoi(rates[r]);int64_t start=counter+144000000;
        uint32_t origin=wrap?UINT32_MAX-30u:0;
        reset(start,rates[r],origin);
        for(int f=1;f<=rate*2;f++) {
            clock_frame(counter);
            network_frame();
        }
        assert(sends==120 && pump_calls==(unsigned)rate*4 && sample_calls==(unsigned)rate*2);
        for(unsigned port=0;port<4;port++) assert(refreshes[port]==40);
        assert((uint32_t)(WORD(0x7de318)-origin)==120);
        assert((uint32_t)(WORD(0x7497a8)-origin)==120);
        printf("network native=%d wrap=%u: %u movement gates, %u full states/port in 2s\n",
               rate,wrap,sends,refreshes[0]);
    }
}
static void transition_pause_stall(void)
{
    int64_t start=counter+144000000;
    reset(start,"144",0);
    const char *rates[]={"144","120","144","60"};
    for(unsigned phase=0;phase<4;phase++) {
        setting=rates[phase];int rate=atoi(setting);
        for(int f=1;f<=rate;f++) {
            clock_frame(counter);network_frame();
        }
    }
    assert(sends==240 && WORD(0x7de318)==240);
    assert(fabs((double)(counter-start)/144000000-4)<.00001);
    for(unsigned port=0;port<4;port++) assert(refreshes[port]==80);
    unsigned before=sends,full=refreshes[0];uint32_t game=WORD(0x7de318);
    setting="144";WORD(0x7497b4)=1;
    for(int f=1;f<=144;f++) {
        clock_frame(counter);network_frame();
        assert(WORD(0x7de318)==game && FLOAT(0x4e794c)==0);
    }
    /* Transport sampling stays responsive to the raw ISR during game pause;
     * unchanged movement full-state refresh ages only on the match clock. */
    assert(sends-before==60 && refreshes[0]==full);
    WORD(0x7497b4)=UINT32_MAX;
    for(int f=1;f<=144;f++) {
        clock_frame(counter);network_frame();
    }
    assert(WORD(0x7de318)==game+60 && refreshes[0]==full+20);
    before=sends;full=refreshes[0];game=WORD(0x7de318);
    clock_frame(counter+14400000);network_frame(); /* 100ms host stall */
    assert(sends==before+1 && refreshes[0]==full+1);
    assert(WORD(0x4e7944)==3 && WORD(0x7de318)==game+3);
    for(unsigned i=0;i<8;i++) network_frame();
    assert(sends==before+1 && refreshes[0]==full+1);
    puts("PASS: native rate switches, pause/resume and bounded stall preserve network clock units");
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    regular_rates();transition_pause_stall();
    puts("PASS: production native clock plus movement/full-state adapters; no live transport claim");
    return 0;
}
