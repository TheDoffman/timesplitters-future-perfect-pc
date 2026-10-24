#ifndef TSFP_SURFACE_INTERVAL_H
#define TSFP_SURFACE_INTERVAL_H
#include <stdint.h>

/* Split an exact advancing game interval at authored tick boundaries. The
 * producer owns pause, elapsed-time clamping and phase; no FPS choice or float
 * accumulation belongs here. A resource owner must initialize/synchronize its
 * held state before using a first piece that starts partway through a tick. */
typedef struct PcSurfaceSlice {
    int64_t units;
    double delta;
    unsigned begins_tick,ends_tick;
} PcSurfaceSlice;
typedef struct PcSurfaceInterval {
    unsigned count;
    int64_t end_phase;
    PcSurfaceSlice pieces[4];
} PcSurfaceInterval;
static inline int pc_surface_interval_split(int64_t frequency,int64_t phase,
    int64_t elapsed,PcSurfaceInterval *out)
{
    if(!out || frequency<=0 || frequency>INT64_MAX/4 || phase<0 ||
       phase>=frequency || elapsed<0 || elapsed>3*frequency)return 0;
    PcSurfaceInterval result={0};
    while(elapsed) {
        int64_t remaining=frequency-phase;
        int64_t units=elapsed<remaining?elapsed:remaining;
        PcSurfaceSlice *piece=&result.pieces[result.count++];
        piece->units=units;piece->delta=(double)units/(double)frequency;
        piece->begins_tick=phase==0;piece->ends_tick=units==remaining;
        phase=piece->ends_tick?0:phase+units;
        elapsed-=units;
    }
    result.end_phase=phase;*out=result;return 1;
}
#endif
