/* Rational native update clock; independent of the 60 Hz console interrupts.
 * Keep one schedule across update boundaries; retain fractional deadlines.
 * Dropped deadlines advance to the next slot instead of creating a burst. */
#ifndef TSFP_PC_NATIVE_CLOCK_H
#define TSFP_PC_NATIVE_CLOCK_H
#include <stdint.h>
typedef struct {
    int64_t epoch, frequency, slot;
    int rate;
} PcNativeClock;
static int64_t pc_native_clock_due(const PcNativeClock *c)
{
    return c->epoch + c->slot * c->frequency / c->rate;
}
static void pc_native_clock_reset(PcNativeClock *c, int64_t now, int64_t frequency, int rate)
{
    c->epoch = now; c->frequency = frequency; c->rate = rate; c->slot = 1;
}
static void pc_native_clock_catch_up(PcNativeClock *c, int64_t now)
{
    if (now > pc_native_clock_due(c)) {
        int64_t slot = (now - c->epoch) * c->rate / c->frequency;
        if (c->epoch + slot * c->frequency / c->rate < now) slot++;
        if (slot > c->slot) c->slot = slot;
    }
}
#endif
