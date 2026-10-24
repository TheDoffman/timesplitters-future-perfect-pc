#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pc_native_timing.h"
#include "graphics/surface_interval.h"

static void sequence(int64_t frequency,int jitter)
{
    PcNativeTiming clock={0};int64_t now=0,total=0,raw_total=0;unsigned ended=0,started=0;
    pc_native_timing_step(&clock,now,frequency,144,0);
    const unsigned rates[]={144,120,60,144};
    for(unsigned frame=0;frame<1440;frame++) {
        int rate=rates[(frame/37)%4];
        int64_t elapsed=frequency/rate;
        if(jitter)elapsed+=((int)(frame%7)-3)*(frequency/4000);
        if(frame==913)elapsed=frequency; /* Bounded catch-up, not a 60-tick burst. */
        int paused=frame>=103 && frame<160;
        now+=elapsed;
        int64_t raw_units=(elapsed>frequency/20?frequency/20:elapsed)*60;
        int64_t expected_units=paused?0:raw_units;
        PcNativeStep step=pc_native_timing_step(&clock,now,frequency,rate,paused);
        assert(step.units_per_tick==frequency && step.game_phase==total%frequency);
        assert(step.game_units==expected_units);
        assert(step.raw_units==raw_units && step.raw_phase==raw_total%frequency);
        PcSurfaceInterval interval;
        assert(pc_surface_interval_split(step.units_per_tick,step.game_phase,step.game_units,&interval));
        int64_t consumed=0,phase=total%frequency;unsigned frame_ended=0;
        for(unsigned i=0;i<interval.count;i++) {
            const PcSurfaceSlice *p=&interval.pieces[i];
            assert(p->units>0 && p->units<=frequency-phase);
            assert(p->begins_tick==(unsigned)(phase==0));
            assert(p->ends_tick==(unsigned)(phase+p->units==frequency));
            assert(p->delta>0 && p->delta<=1);
            assert(fabs(p->delta-(double)p->units/frequency)<1e-15);
            consumed+=p->units;phase=(phase+p->units)%frequency;
            started+=p->begins_tick;ended+=p->ends_tick;frame_ended+=p->ends_tick;
        }
        total+=expected_units;
        assert(consumed==expected_units && interval.end_phase==total%frequency);
        assert(frame_ended==step.game_ticks && ended==(unsigned)(total/frequency));
        assert(started==ended+(interval.end_phase!=0));
        if(paused)assert(interval.count==0);
        PcSurfaceInterval raw_interval;
        assert(pc_surface_interval_split(frequency,step.raw_phase,step.raw_units,&raw_interval));
        int64_t raw_consumed=0; unsigned raw_ticks=0;
        for(unsigned i=0;i<raw_interval.count;i++) {
            raw_consumed+=raw_interval.pieces[i].units;
            raw_ticks+=raw_interval.pieces[i].ends_tick;
        }
        raw_total+=raw_units;
        assert(raw_consumed==raw_units && raw_ticks==step.raw_ticks);
        assert(raw_interval.end_phase==raw_total%frequency);
    }
}
int main(void)
{
    const int64_t frequencies[]={72000000,10000003,1000000000};
    for(unsigned i=0;i<3;i++)for(int jitter=0;jitter<2;jitter++)sequence(frequencies[i],jitter);
    /* A double accumulator of the published FLOAT deltas still misses a tick.
     * Verify an actual 144-Hz producer, not a hardcoded fractional constant. */
    PcNativeTiming clock={0};const int64_t frequency=72000000;
    pc_native_timing_step(&clock,0,frequency,144,0);
    double naive=0;unsigned ticks=0;
    for(unsigned frame=1;frame<=144;frame++) {
        PcNativeStep step=pc_native_timing_step(&clock,frame*(frequency/144),frequency,144,0);
        PcSurfaceInterval interval;
        assert(pc_surface_interval_split(step.units_per_tick,step.game_phase,step.game_units,&interval));
        naive+=step.game_delta;
        for(unsigned i=0;i<interval.count;i++)ticks+=interval.pieces[i].ends_tick;
    }
    assert((unsigned)naive==59 && ticks==60);
    /* A recorded-input sampler cannot substitute game units: they stop on
     * pause while raw time continues. Float raw_delta still loses a boundary. */
    clock=(PcNativeTiming){0};
    pc_native_timing_step(&clock,0,frequency,144,1);
    double raw_naive=0; unsigned raw_ticks=0;
    for(unsigned frame=1;frame<=144;frame++) {
        PcNativeStep step=pc_native_timing_step(&clock,frame*(frequency/144),frequency,144,1);
        PcSurfaceInterval interval;
        assert(!step.game_units && !step.game_ticks);
        assert(pc_surface_interval_split(frequency,step.raw_phase,step.raw_units,&interval));
        raw_naive+=step.raw_delta;
        for(unsigned i=0;i<interval.count;i++)raw_ticks+=interval.pieces[i].ends_tick;
    }
    assert((unsigned)raw_naive==59 && raw_ticks==60);
    PcSurfaceInterval result;
    assert(pc_surface_interval_split(100,1,300,&result));
    assert(result.count==4 && result.end_phase==1);
    assert(result.pieces[0].units==99 && result.pieces[3].units==1);
    assert(pc_surface_interval_split(100,75,0,&result) && !result.count && result.end_phase==75);
    PcSurfaceInterval before;memset(&result,0x5a,sizeof result);before=result;
    assert(!pc_surface_interval_split(0,0,0,&result));
    assert(!pc_surface_interval_split(INT64_MAX,0,0,&result));
    assert(!pc_surface_interval_split(100,-1,1,&result));
    assert(!pc_surface_interval_split(100,100,1,&result));
    assert(!pc_surface_interval_split(100,0,-1,&result));
    assert(!pc_surface_interval_split(100,0,301,&result));
    assert(!memcmp(&before,&result,sizeof result));
    assert(!pc_surface_interval_split(100,0,1,NULL));
    PcNativeTiming unchanged=clock;
    assert(!pc_native_timing_step(&clock,INT64_MAX,INT64_MAX,144,0).units_per_tick);
    assert(!memcmp(&clock,&unchanged,sizeof clock));
    assert(!pc_native_timing_step(NULL,0,100,144,0).units_per_tick);
    PcNativeStep reset=pc_native_timing_step(&clock,99,100,144,0);
    assert(reset.units_per_tick==100 && !reset.game_units && !reset.game_phase &&
           !reset.raw_units && !reset.raw_phase);
    printf("PASS: exact surface intervals across rates, jitter, pause and stalls; float accumulation %.12f vs %u ticks\n",naive,ticks);
    return 0;
}
