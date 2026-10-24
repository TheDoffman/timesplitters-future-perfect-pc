#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/pc_native_timing.h"
#include "../src/pc_native_clock.h"

static void pacing(int rate, int64_t frequency)
{
    PcNativeClock clock;
    pc_native_clock_reset(&clock,12345,frequency,rate);
    int64_t previous=12345;
    for(int i=1;i<=rate*10;i++) {
        int64_t due=pc_native_clock_due(&clock);
        assert(due>previous && due==12345+(int64_t)i*frequency/rate);
        previous=due; clock.slot++;
    }
    assert(previous==12345+10*frequency);
    int64_t late=pc_native_clock_due(&clock)+frequency/10;
    pc_native_clock_catch_up(&clock,late);
    assert(pc_native_clock_due(&clock)>=late);
    assert(pc_native_clock_due(&clock)-late<=frequency/rate+1);
}

static void compare_rates(int rate, int64_t frequency)
{
    PcNativeTiming clock = {0};
    uint64_t ticks = 0;
    double distance = 0;
    pc_native_timing_step(&clock, 0, frequency, rate, 0);
    /* Ten seconds of constant velocity, with counter frequencies that are
     * deliberately not multiples of 60/120/144. Each frame is a new step. */
    for (int frame = 1; frame <= rate * 10; frame++) {
        int64_t now = (int64_t)frame * frequency / rate;
        PcNativeStep step = pc_native_timing_step(&clock, now, frequency, rate, 0);
        assert(step.game_delta > 0 && step.game_delta < 1.01f);
        assert(step.game_ticks == step.raw_ticks);
        assert(step.raw_units==step.game_units && step.raw_phase==step.game_phase);
        assert(step.raw_ticks==(step.raw_phase+step.raw_units)/frequency);
        distance += step.game_delta * 2.5;
        ticks += step.game_ticks;
    }
    assert(ticks == 600);
    assert(fabs(distance - 1500.0) < .001);
    assert(clock.game_remainder == 0);
}

int main(void)
{
    for (int rate = 60; rate <= 144; rate += rate == 60 ? 60 : 24) {
        compare_rates(rate, 10000000);
        compare_rates(rate, 1000000003);
        pacing(rate,10000000);
        pacing(rate,1000000003);
    }
    PcNativeTiming clock = {0};
    pc_native_timing_step(&clock, 0, 144000, 144, 0);
    unsigned raw = 0, game = 0;
    for (int i = 1; i <= 432; i++) {
        int paused = i > 144 && i <= 288;
        PcNativeStep step = pc_native_timing_step(&clock, i * 1000, 144000, 144, paused);
        assert(step.raw_units==60000);
        assert(step.raw_phase==((int64_t)(i-1)*60000)%144000);
        raw += step.raw_ticks; game += step.game_ticks;
        if (paused) assert(step.game_ticks == 0 && step.game_delta == 0);
        else assert(step.game_delta > .416f && step.game_delta < .417f);
    }
    assert(raw == 180 && game == 120);
    PcNativeStep stalled = pc_native_timing_step(&clock, 144000 * 100LL, 144000, 144, 0);
    assert(stalled.raw_ticks == 3 && stalled.raw_delta == 3);
    assert(stalled.raw_units==432000);
    PcNativeStep resumed = pc_native_timing_step(&clock, 144000 * 100LL + 1000, 144000, 144, 0);
    assert(resumed.raw_delta < .417f);
    PcNativeStep reversed = pc_native_timing_step(&clock, 1, 144000, 144, 0);
    assert(reversed.raw_ticks == 0 && reversed.raw_delta == 0);
    assert(reversed.raw_units==0 && reversed.raw_phase==clock.raw_remainder);
    assert(pc_native_timing_step(&clock, 100, 0, 144, 0).raw_delta == 0);
    puts("Native fractional timing: movement, timers, pause, stalls and counter precision passed");
    return 0;
}
