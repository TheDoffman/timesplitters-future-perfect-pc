/* US 45410066 sub_001538C0, from the verified original disassembly. The
 * original path remains available verbatim when native_fps is absent/zero.
 * Float deltas run every native frame; whole ticks retain console units.
 * The ISR-owned counter at 0x563918 and kernel/audio clocks are untouched. */
#include "platform/xbox_winnt.h"
#include "pc_native_rate.h"
#include "pc_native_timing.h"
#include "pc_native_clock.h"
extern uint64_t xbox_ReadTimeStampCounter(void);
extern void sub_000455B0(void), sub_000455C0(void), sub_00150190(void);
int g_tsfp_native_timing_active;
#include "pc_native_character_guest.h"
#include "pc_native_character_trace_guest.inc"
#include "pc_native_walk_trace_guest.inc"
/* Shared host-side interval for future procedural consumers. Original mode
 * publishes zero; the existing guest timing words retain their exact layout. */
PcNativeStep g_tsfp_native_step;
#define NT_WORD(va) (*(volatile uint32_t *)((uintptr_t)(va) + g_xbox_mem_offset))
#define NT_FLOAT(va) (*(float *)((uintptr_t)(va) + g_xbox_mem_offset))
#include "pc_native_pose_trace.h"
static uint32_t pc_native_pose_word(uint32_t address) { return NT_WORD(address); }
static void pc_native_pose_trace(void)
{
    static PcNativePoseTrace trace;
    PcNativePose pose;
    if(!pc_native_pose_enabled(&trace) ||
       !pc_native_pose_collect(pc_native_pose_word,NT_WORD(g_esp),&pose))return;
    LARGE_INTEGER now,frequency;
    QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
    if(frequency.QuadPart>0)pc_native_pose_record(&trace,&pose,(double)now.QuadPart/(double)frequency.QuadPart);
}
static void pc_native_input_guest(void (*original)(void))
{
    if (!g_tsfp_native_timing_active || NT_WORD(0x4E7940u)) {
        original();
        return;
    }
    /* Refresh axes/connection state each native update. The US frontend
     * consumes digital edges on whole ticks, so retain its published held
     * state between ticks: the next full poll still sees the right prior state
     * and emits each press/release once. The original decoder owns dead zones,
     * axis ranges and absolute rumble output; no elapsed-time counter runs here. */
    uint32_t record = NT_WORD(g_esp + 4u);
    if (record) {
        uint32_t held = NT_WORD(record + 12u);
        original();
        NT_WORD(record + 8u) = held;
        NT_WORD(record + 12u) = held;
        NT_WORD(record + 16u) = 0;
        NT_WORD(record + 20u) = 0;
        return;
    }
    g_esp += 4u;
}
static float pc_native_save_animation_delta(void)
{
    return g_tsfp_native_timing_active ? NT_FLOAT(0x4E794Cu) : 0;
}
static void pc_native_restore_animation_delta(float delta)
{
    if (g_tsfp_native_timing_active) NT_FLOAT(0x4E794Cu) = delta;
}
static void pc_native_trace(int64_t now, int64_t frequency, PcNativeStep step)
{
    static FILE *file;
    static unsigned rows;
    static int opened;
    static int64_t epoch;
    if (!opened) {
        const char *path = getenv("TSFP_NATIVE_TRACE");
        opened = 1;
        epoch = now;
        if (path && *path) {
            file = fopen(path, "wb");
            if (file) fputs("frame,seconds,raw_delta,game_delta,raw_ticks,game_ticks,scene,flags\n", file);
        }
    }
    if (!file) return;
    fprintf(file, "%u,%.9f,%.9g,%.9g,%u,%u,%u,%u\n",
        NT_WORD(0x7497B8u), (double)(now-epoch)/(double)frequency,
        step.raw_delta, step.game_delta, step.raw_ticks, step.game_ticks,
        NT_WORD(0x79094Cu), NT_WORD(0x7DE458u));
    if (++rows % 144u == 0) fflush(file);
    if (rows >= 20000u) { fclose(file); file = NULL; }
}
static void pc_native_call(void (*function)(void))
{
    /* The wrapper already owns the caller's return slot. A zero-argument
     * nested cdecl call needs a distinct return slot, consumed by its ret. */
    uint32_t stack = g_esp;
    g_esp -= 4u;
    NT_WORD(g_esp) = 0;
    function();
    g_esp = stack;
}
static void pc_native_timing_guest(void (*original)(void))
{
    static PcNativeTiming clock;
    static PcNativeClock pacing;
    static HANDLE timer;
    static int announced;
    static int physics_history_pending;
    LARGE_INTEGER now, frequency;
    pc_native_pose_trace();
    recomp_config_commit_native_rate();
    int rate = pc_native_rate();
    if (!rate) {
        g_tsfp_native_step=(PcNativeStep){0};
        if (g_tsfp_native_timing_active) {
            /* The original waits relative to 7A58B0. Rebase to the real ISR
             * counter so a stale native/menu deadline cannot cause catch-up. */
            uint32_t vblank=NT_WORD(0x563918u);
            NT_WORD(0x7497A8u)=NT_WORD(0x7497ACu)=NT_WORD(0x7A58B0u)=vblank;
            clock=(PcNativeTiming){0}; pacing=(PcNativeClock){0};
            announced=0;
            g_tsfp_native_timing_active=0;
            physics_history_pending=1;
            fprintf(stderr,"[NATIVE-TIMING] 60 FPS original clock applied live\n");
        }
        original();
        /* Native physics consumes live elapsed time, rather than this old
         * fifteen-frame average. On return to Original 60, seed the history
         * with its first advancing interval so native-sized samples cannot
         * temporarily slow physics. Defer across a paused transition. */
        if (physics_history_pending && NT_FLOAT(0x4E794Cu)>0.f) {
            float delta=NT_FLOAT(0x4E794Cu);
            for(unsigned i=0;i<15;i++) NT_FLOAT(0x74C414u+4u*i)=delta;
            physics_history_pending=0;
        }
        return;
    }
    g_tsfp_native_timing_active = 1;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    if (pacing.rate && pacing.rate != rate) {
        /* Preserve elapsed time and both whole-tick remainders when changing
         * native targets. Only the future pacing schedule changes. */
        pc_native_clock_reset(&pacing, now.QuadPart, frequency.QuadPart, rate);
        announced=0;
    }
    if (!pacing.rate) {
        pc_native_clock_reset(&pacing, now.QuadPart, frequency.QuadPart, rate);
    } else {
        int64_t due = pc_native_clock_due(&pacing);
        if (!timer) timer = CreateWaitableTimerExW(NULL, NULL, 2, TIMER_ALL_ACCESS);
        if (!timer) timer = CreateWaitableTimerW(NULL, FALSE, NULL);
        while (now.QuadPart < due) {
            LARGE_INTEGER wait;
            wait.QuadPart = -(due - now.QuadPart) * 10000000 / frequency.QuadPart;
            if (!wait.QuadPart) wait.QuadPart = -1;
            if (timer && SetWaitableTimer(timer, &wait, 0, NULL, NULL, FALSE))
                WaitForSingleObject(timer, 100);
            else Sleep(1);
            QueryPerformanceCounter(&now);
        }
        pacing.slot++;
        pc_native_clock_catch_up(&pacing, now.QuadPart);
    }
    int32_t pause = (int32_t)NT_WORD(0x7497B4u);
    PcNativeStep step = pc_native_timing_step(&clock, now.QuadPart,
        frequency.QuadPart, rate, pause > 0);
    g_tsfp_native_step=step;
    if (!announced) {
        announced = 1;
        fprintf(stderr,
            "[NATIVE-TIMING] %d fresh updates/s target; fractional 60-Hz deltas; interpolation off; console ISR unchanged\n", rate);
    }
    /* Same three profiling histories as the original routine. */
    for (unsigned i = 0; i < 4; i++) {
        NT_WORD(0x7A58A0u + i*4u) = NT_WORD(0x7A58A4u + i*4u);
        NT_FLOAT(0x7A58C0u + i*4u) = NT_FLOAT(0x7A58C4u + i*4u);
        NT_FLOAT(0x7A58E0u + i*4u) = NT_FLOAT(0x7A58E4u + i*4u);
    }
    uint32_t vblank = NT_WORD(0x563918u);
    NT_WORD(0x7497A8u) = NT_WORD(0x7497ACu) = NT_WORD(0x7A58B0u) = vblank;
    uint32_t refresh = NT_WORD(0x4E8CECu);
    if (refresh) NT_FLOAT(0x7A58F0u) = (float)(uint32_t)xbox_ReadTimeStampCounter() /
        (float)(733333333u / refresh);
    NT_WORD(0x4E7940u) = step.raw_ticks;
    NT_FLOAT(0x4E7948u) = step.raw_delta;
    NT_WORD(0x7497B8u)++;
    if (step.raw_ticks > 1) NT_WORD(0x7497BCu)++;
    if (pause > 0 && NT_FLOAT(0x4E794Cu) > 0) pc_native_call(sub_000455B0);
    NT_WORD(0x4E7944u) = step.game_ticks;
    NT_FLOAT(0x4E794Cu) = step.game_delta;
    if (pause < 0) {
        NT_WORD(0x7497B4u) = 0;
        pc_native_call(sub_000455C0);
    }
    NT_WORD(0x7497B0u) += step.raw_ticks;
    pc_native_call(sub_00150190);
    pc_native_trace(now.QuadPart, frequency.QuadPart, step);
    g_esp += 4u;
}
#undef NT_WORD
#undef NT_FLOAT
