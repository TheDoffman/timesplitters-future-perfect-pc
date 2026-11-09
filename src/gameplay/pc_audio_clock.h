#ifndef TSFP_PC_AUDIO_CLOCK_H
#define TSFP_PC_AUDIO_CLOCK_H
#include <stdint.h>
/* A media clock for one host clock domain. Speed changes preserve position;
 * the kernel/ISR and source PCM sample rates remain unchanged. */
typedef struct PcAudioClock {uint64_t previous;double milliseconds;int initialized;} PcAudioClock;
static inline uint64_t pc_audio_clock_step(PcAudioClock *clock,uint64_t now,float speed)
{
    if(!(speed>=.05f&&speed<=4.f))speed=1.f;
    if(!clock->initialized) {clock->initialized=1;clock->previous=now;clock->milliseconds=(double)now;}
    else if(now>=clock->previous) {
        clock->milliseconds+=(double)(now-clock->previous)*speed;
        clock->previous=now;
    }
    /* A worker can submit a timestamp sampled before another thread's tick.
     * Ignore stale samples instead of rebasing the scaled clock to real time. */
    return (uint64_t)clock->milliseconds;
}
#endif
