#ifndef TSFP_PC_NATIVE_TIMING_H
#define TSFP_PC_NATIVE_TIMING_H
#include <stdint.h>
/* The engine exposes two forms of delta: fractional ticks for continuous
 * motion and whole 60-Hz ticks for counters. Carry the remainder separately
 * for paused and unpaused time; rounding each 144-Hz frame to one speeds up
 * integer timers by 2.4x. Host counter units never enter guest memory. */
typedef struct PcNativeTiming {
    int64_t last, frequency, raw_remainder, game_remainder;
    int initialized;
} PcNativeTiming;
typedef struct PcNativeStep {
    float raw_delta, game_delta;
    uint32_t raw_ticks, game_ticks;
    /* Exact host-side game interval, in scaled counter units. These never enter
     * guest memory. A tick is units_per_tick; game_phase is the START phase.
     * Consumers needing authored boundaries must not sum rounded float deltas. */
    int64_t game_units, game_phase, units_per_tick;
} PcNativeStep;
/* Published by the shared guest timing hook before guest-local time overrides.
 * Consumers must still honor their resource lifecycle and guest time scopes. */
extern PcNativeStep g_tsfp_native_step;
static inline PcNativeStep pc_native_timing_step(PcNativeTiming *clock,
    int64_t now, int64_t frequency, int rate, int paused)
{
    PcNativeStep step = {0};
    int64_t elapsed, scaled;
    if (!clock || frequency <= 0 || frequency > INT64_MAX/4 || rate <= 0) return step;
    step.units_per_tick = frequency;
    if (!clock->initialized || clock->frequency != frequency) {
        clock->initialized = 1;
        clock->frequency = frequency;
        clock->last = now;
        clock->raw_remainder = clock->game_remainder = 0;
        return step;
    }
    step.game_phase = clock->game_remainder;
    elapsed = now > clock->last ? now - clock->last : 0;
    if (now > clock->last) clock->last = now;
    /* Match the original engine's maximum three-tick catch-up. Loading or a
     * debugger stop must not produce an unbounded physics step or a burst. */
    if (elapsed > frequency / 20) elapsed = frequency / 20;
    scaled = elapsed * 60;
    step.raw_delta = (float)((double)scaled / (double)frequency);
    clock->raw_remainder += scaled;
    step.raw_ticks = (uint32_t)(clock->raw_remainder / frequency);
    clock->raw_remainder %= frequency;
    if (!paused) {
        step.game_units = scaled;
        step.game_delta = step.raw_delta;
        clock->game_remainder += scaled;
        step.game_ticks = (uint32_t)(clock->game_remainder / frequency);
        clock->game_remainder %= frequency;
    }
    return step;
}
#endif
