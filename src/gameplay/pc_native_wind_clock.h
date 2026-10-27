#ifndef TSFP_PC_NATIVE_WIND_CLOCK_H
#define TSFP_PC_NATIVE_WIND_CLOCK_H
#include <stdint.h>
#include <math.h>
#include "pc_native_timing.h"
static inline int pc_native_wind_clock_matches(const PcNativeStep *s,float h,uint32_t whole)
{
    if(!s || !isfinite(h) || !(h>0.f&&h<1.f) ||
       s->units_per_tick<=0 || s->game_phase<0 || s->game_phase>=s->units_per_tick ||
       s->game_units<=0 || s->game_units>=s->units_per_tick ||
       s->game_units>INT64_MAX-s->game_phase)return 0;
    return (float)((double)s->game_units/s->units_per_tick)==h &&
        (uint32_t)((s->game_phase+s->game_units)/s->units_per_tick)==whole;
}
#endif
