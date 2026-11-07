#ifndef TSFP_PC_MOVIE_SPEED_H
#define TSFP_PC_MOVIE_SPEED_H
#include <stdint.h>
#include <math.h>
typedef struct PcMovieSpeed {int64_t previous;double milliseconds;} PcMovieSpeed;
static inline void pc_movie_speed_reset(PcMovieSpeed *clock,int64_t now)
{clock->previous=now;clock->milliseconds=0.;}
/* Integrate each interval; changing rate never jumps or rewinds the movie. */
static inline uint32_t pc_movie_speed_time(PcMovieSpeed *clock,int64_t now,int64_t frequency,double speed)
{
    if(frequency<=0 || !isfinite(speed) || speed<.05 || speed>4.)return (uint32_t)clock->milliseconds;
    if(now>clock->previous)clock->milliseconds+=(double)(now-clock->previous)*1000./frequency*speed;
    if(now>clock->previous)clock->previous=now;
    if(clock->milliseconds>UINT32_MAX)clock->milliseconds=UINT32_MAX;
    return (uint32_t)clock->milliseconds;
}
#endif
