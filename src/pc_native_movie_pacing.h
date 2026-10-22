#ifndef TSFP_PC_NATIVE_MOVIE_PACING_H
#define TSFP_PC_NATIVE_MOVIE_PACING_H
#include "platform/xbox_winnt.h"
#include "pc_native_clock.h"
#include "pc_native_rate.h"
/* Movies poll outside the level-update pacer. Keep their host polling bounded
 * without changing decoded-picture timestamps, the audio clock or guest time.
 * One schedule/timer belongs to each playback, and is released on close. */
typedef struct {
    PcNativeClock clock;
    HANDLE timer;
} PcNativeMoviePacing;
static void pc_native_movie_pacing_close(PcNativeMoviePacing *p)
{
    if(p->timer)CloseHandle(p->timer);
    p->timer=NULL;p->clock.rate=0;
}
static void pc_native_movie_pacing_wait(PcNativeMoviePacing *p)
{
    LARGE_INTEGER now,frequency;
    int rate=pc_native_rate();
    if(!rate) { p->clock.rate=0;return; }
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<rate ||
       !QueryPerformanceCounter(&now)) { p->clock.rate=0;return; }
    if(p->clock.rate!=rate || p->clock.frequency!=frequency.QuadPart ||
       now.QuadPart<p->clock.epoch) {
        pc_native_clock_reset(&p->clock,now.QuadPart,frequency.QuadPart,rate);
        return; /* First picture is available immediately. */
    }
    int64_t due=pc_native_clock_due(&p->clock);
    while(now.QuadPart<due) {
        LARGE_INTEGER wait;
        if(!p->timer)p->timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_ALL_ACCESS);
        if(!p->timer)p->timer=CreateWaitableTimerW(NULL,FALSE,NULL);
        wait.QuadPart=-(due-now.QuadPart)*10000000/frequency.QuadPart;
        if(!wait.QuadPart)wait.QuadPart=-1;
        if(!p->timer || !SetWaitableTimer(p->timer,&wait,0,NULL,NULL,FALSE) ||
           WaitForSingleObject(p->timer,100)!=WAIT_OBJECT_0)Sleep(1);
        if(!QueryPerformanceCounter(&now)) { p->clock.rate=0;return; }
    }
    p->clock.slot++;
    pc_native_clock_catch_up(&p->clock,now.QuadPart);
    /* A late decode does not earn a burst of immediate catch-up polls. */
    if(pc_native_clock_due(&p->clock)<=now.QuadPart)p->clock.slot++;
}
#endif
