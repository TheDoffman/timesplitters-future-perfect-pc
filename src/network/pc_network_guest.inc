/* Each explicit wrapper lives in recomp_manual.c so the lifter can find it. */
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
#include "pc_network.h"
#include "pc_debug.h"
static void pc_network_guest(uint32_t address, void (*original)(void))
{
    uint32_t args[12]; unsigned n,i,count; double latency; int mode;
    if(!pc_network_enabled()) { pc_debug_latency(0,-1,0); original(); return; }
    n=pc_network_arguments(address);
    for(i=0;i<n;i++) args[i]=*(uint32_t *)((uintptr_t)(g_esp+4+i*4)+g_xbox_mem_offset);
    g_eax=pc_network_call(address,args,g_xbox_mem_offset);
    mode=pc_network_latency(&latency,&count);
    pc_debug_latency(mode,latency,count);
    g_esp+=4+n*4;
}
#else
/* Builds without a desktop transport retain the original guest path. */
static void pc_network_guest(uint32_t address, void (*original)(void))
{
    (void)address;
    original();
}
#endif

/* The Xbox sampler treats unchanged controller bytes as unchanged movement.
 * Host mouse look bypasses those bytes, and independent frame stalls also
 * invalidate dead reckoning without changing a held key. Bound full-state
 * age to three 60-Hz match ticks (20 Hz), using the original packet encoder.
 * The sampler's argument is the original peer: up to four local Xbox players. */
static void pc_network_guest_sample(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    uint32_t peer = *(uint32_t *)((uintptr_t)(g_esp + 4u) + g_xbox_mem_offset);
#endif
    original();
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if (pc_network_enabled() &&
        *(uint32_t *)((uintptr_t)0x0076B1A4u + g_xbox_mem_offset) == 15u) {
        uint32_t frame = *(uint32_t *)((uintptr_t)0x007DE318u + g_xbox_mem_offset);
        uint32_t count = *(uint32_t *)((uintptr_t)(peer + 0x20u) + g_xbox_mem_offset);
        uint32_t i;
        if (count > 4u) return;
        for (i = 0; i < count; ++i) {
            uint32_t record = *(uint32_t *)((uintptr_t)(peer + 0x0Cu + i * 4u) + g_xbox_mem_offset);
            if (record) {
                uint32_t last = *(uint32_t *)((uintptr_t)(record + 0x10Cu) + g_xbox_mem_offset);
                if (frame - last >= 3u)
                    *(uint32_t *)((uintptr_t)(record + 0x100u) + g_xbox_mem_offset) = 1u;
            }
        }
    }
#endif
}

/* The US network pump samples movement only when its eight-Hz epoch changes
 * (60 / 8 truncates to seven guest ticks). Instant keyboard reversals can be
 * missed or reach the other player 117 ms late, followed by a position snap.
 * Open just the movement-send gate once per guest tick in an active PC match.
 * Keep the original epoch, game clock, heartbeat and timeout calculations. */
static void pc_network_guest_tick(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    static uint32_t last_frame;
    static int have_frame;
    if (pc_network_enabled()) {
        uint32_t *state = (uint32_t *)((uintptr_t)0x0076B1A4u + g_xbox_mem_offset);
        uint32_t frame = *(uint32_t *)((uintptr_t)0x007497A8u + g_xbox_mem_offset);
        if (*state == 15) {
            if (!have_frame || frame != last_frame) {
                uint32_t epoch = *(uint32_t *)((uintptr_t)0x0076B0F0u + g_xbox_mem_offset);
                *(uint32_t *)((uintptr_t)0x0076B0ECu + g_xbox_mem_offset) = epoch - 1u;
                last_frame = frame;
                have_frame = 1;
            }
        } else have_frame = 0;
    } else have_frame = 0;
#endif
    original();
}
