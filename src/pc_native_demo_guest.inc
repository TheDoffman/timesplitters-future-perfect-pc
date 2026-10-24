#include <math.h>
/* US 43220: preserve the raw interval across its per-controller exit/wait
 * loop. State 2 overwrites the guest raw float with one while visiting the
 * first controller; subsequent controllers must still use this frame's time.
 * Playback packet cadence and recording remain original here. */
static RECOMP_MANUAL_TLS float pc_native_demo_raw_interval;
static RECOMP_MANUAL_TLS int pc_native_demo_interval_active;
static RECOMP_MANUAL_TLS int pc_native_demo_exit_seen;

float pc_native_demo_timer_multiply(uint32_t site, float left, float right)
{
    if(pc_native_demo_interval_active && site==0x43473u) pc_native_demo_exit_seen=1;
    return left * (pc_native_demo_interval_active ? pc_native_demo_raw_interval : right);
}

static void pc_native_demo_guest(void (*original)(void))
{
    uint32_t *raw_ticks=(uint32_t *)((uintptr_t)0x4E7940u+g_xbox_mem_offset);
    float *raw_delta=(float *)((uintptr_t)0x4E7948u+g_xbox_mem_offset);
    uint32_t *mode=(uint32_t *)((uintptr_t)0x6B98A4u+g_xbox_mem_offset);
    float previous_interval=pc_native_demo_raw_interval, interval=*raw_delta;
    int previous_active=pc_native_demo_interval_active;
    int previous_exit=pc_native_demo_exit_seen;
    uint32_t ticks=*raw_ticks, entry_mode=*mode;
    int active=g_tsfp_native_timing_active && isfinite(interval) && interval>=0.f;
    pc_native_demo_interval_active=active;
    pc_native_demo_raw_interval=interval;
    pc_native_demo_exit_seen=0;
    original();
    /* Exit cleanup can reinitialize the level and replace the demo mode.
     * Remember the executed exit site as well as the entry/EOF state. Keep
     * its game pause while UI/audio raw time stays at native pace. */
    if(active && (entry_mode==2u || pc_native_demo_exit_seen || (entry_mode==1u && *mode==2u))) {
        *raw_ticks=ticks;
        *raw_delta=interval;
    }
    pc_native_demo_raw_interval=previous_interval;
    pc_native_demo_interval_active=previous_active;
    pc_native_demo_exit_seen=previous_exit;
}
