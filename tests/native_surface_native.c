#include <assert.h>
#include <stdio.h>
#include "pc_surface_native.h"
#include "pc_native_timing.h"
int main(void) {
    assert(pc_surface_native_policy(1,NULL,NULL,NULL,NULL));
    assert(pc_surface_native_policy(1,"",NULL,NULL,NULL));
    assert(pc_surface_native_policy(1,"1",NULL,NULL,NULL));
    assert(!pc_surface_native_policy(1,"0",NULL,NULL,NULL));
    assert(!pc_surface_native_policy(1,"invalid",NULL,NULL,NULL));
    assert(!pc_surface_native_policy(0,"1",NULL,NULL,NULL));
    assert(!pc_surface_native_policy(1,NULL,"1",NULL,NULL));
    assert(!pc_surface_native_policy(1,NULL,NULL,"1",NULL));
    assert(!pc_surface_native_policy(1,NULL,NULL,NULL,"1"));
    for(unsigned rate=120;rate<=144;rate+=24) {
        PcNativeTiming producer={0};PcSurfaceNativeClock clock={0},next;
        unsigned begins=0,continues=0;double delta,total=0;
        pc_native_timing_step(&producer,0,72000000,rate,0);
        uint32_t ticks=0;
        for(unsigned frame=1;frame<=rate*3;frame++) {
            int paused=frame>rate && frame<=rate+rate/2;
            PcNativeStep step=pc_native_timing_step(&producer,(int64_t)frame*72000000/rate,72000000,rate,paused);
            ticks+=step.game_ticks;
            PcSurfaceNativeSample s={0};s.feedback.frame=frame;s.feedback.level_ticks=ticks;
            s.feedback.scene=4;s.feedback.level=7;s.feedback.whole=step.game_ticks;
            s.frequency=step.units_per_tick;s.start_phase=step.game_phase;s.units=step.game_units;
            unsigned action=pc_surface_native_plan(&clock,&s,&next,&delta);
            if(paused){assert(!action);continue;}
            if(!action){assert(!clock.ready && !ticks);continue;}
            assert(action==(step.game_ticks?1u:2u));
            begins+=action==1;continues+=action==2;
            assert(delta>=0 && delta<1);total+=delta;
            clock=next;
            assert(pc_surface_native_plan(&clock,&s,&next,&delta)==3 && delta==0);
        }
        assert(begins==150 && continues>100 && total>50);
        /* A skipped resource interval must not use stale state. A whole update
         * resynchronizes once, including the original three-tick catch-up. */
        PcSurfaceNativeSample s={0};s.feedback.frame=10000;s.feedback.level_ticks=ticks+1;
        s.feedback.scene=4;s.feedback.level=7;s.frequency=72000000;s.units=30000000;
        assert(!pc_surface_native_plan(&clock,&s,&next,&delta));
        s.feedback.whole=3;s.units=216000000;s.start_phase=12000000;
        assert(pc_surface_native_plan(&clock,&s,&next,&delta)==1 && delta==1./6.);
        clock=next;s.feedback.frame++;s.feedback.whole=0;s.start_phase=next.phase;s.units=30000000;
        s.feedback.scene=5;assert(!pc_surface_native_plan(&clock,&s,&next,&delta));
        s.feedback.scene=4;s.frequency=36000000;assert(!pc_surface_native_plan(&clock,&s,&next,&delta));
    }
    puts("PASS: native water follows authored events, exact phase, pause, duplicate and gap rules");
    return 0;
}
