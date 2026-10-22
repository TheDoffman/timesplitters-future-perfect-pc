#include "platform/xbox_winnt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int64_t host_counter,host_frequency=10000000,deadline;
static unsigned creates,closes,waits,sleeps;
static int fail_create,fail_set,fail_wait,fail_clock;
static const char *setting;
const char *recomp_config_lookup(const char *env,const char *key)
{ assert(!strcmp(env,"TSFP_NATIVE_FPS") && !strcmp(key,"native_fps"));return setting; }
static BOOL qpf(LARGE_INTEGER *n) { n->QuadPart=host_frequency;return !fail_clock; }
static BOOL qpc(LARGE_INTEGER *n) { n->QuadPart=host_counter;return !fail_clock; }
static HANDLE create(void) { if(fail_create)return NULL;creates++;return (HANDLE)(uintptr_t)creates; }
static BOOL close_timer(HANDLE h) { assert(h);closes++;return TRUE; }
static BOOL set_timer(HANDLE h,const LARGE_INTEGER *n,LONG period,void *cb,void *arg,BOOL resume)
{ assert(h && n->QuadPart<0 && !period && !cb && !arg && !resume);deadline=host_counter+(-n->QuadPart)*host_frequency/10000000;return !fail_set; }
static DWORD wait_timer(HANDLE h,DWORD timeout)
{ assert(h && timeout==100);waits++;if(fail_wait)return WAIT_FAILED;host_counter=deadline>host_counter?deadline:host_counter+1;return WAIT_OBJECT_0; }
static void sleep_ms(DWORD ms) { assert(ms==1);sleeps++;host_counter+=host_frequency/1000; }
#define QueryPerformanceFrequency qpf
#define QueryPerformanceCounter qpc
#define CreateWaitableTimerExW(a,b,c,d) create()
#define CreateWaitableTimerW(a,b,c) create()
#define CloseHandle close_timer
#define SetWaitableTimer set_timer
#define WaitForSingleObject wait_timer
#define Sleep sleep_ms
#include "../src/pc_native_movie_pacing.h"
int main(void) {
    PcNativeMoviePacing p={0};host_counter=123;
    setting=NULL;pc_native_movie_pacing_wait(&p);assert(host_counter==123 && !creates);
    setting="144x";pc_native_movie_pacing_wait(&p);assert(host_counter==123 && !creates);
    const char *rates[]={"60","120","144"};
    for(unsigned r=0;r<3;r++)for(unsigned f=0;f<2;f++) {
        host_frequency=f?1000000003:10000000;setting=rates[r];host_counter=123;
        pc_native_movie_pacing_wait(&p);assert(host_counter==123);
        for(unsigned i=1;i<=p.clock.rate*10u;i++) {
            pc_native_movie_pacing_wait(&p);
            int64_t expected=123+(int64_t)i*host_frequency/p.clock.rate;
            assert(host_counter>=expected && host_counter-expected<host_frequency/1000000);
        }
        /* Already-paced callers incur no second wait. Late decode/audio work
         * skips missed slots, then the next poll must wait for a future one. */
        host_counter=pc_native_clock_due(&p.clock);unsigned before=waits;
        pc_native_movie_pacing_wait(&p);assert(waits==before);
        host_counter+=host_frequency*3;int64_t late=host_counter;
        pc_native_movie_pacing_wait(&p);assert(host_counter==late);
        pc_native_movie_pacing_wait(&p);
        printf("movie rate=%d host_frequency=%lld late_gap=%lld limit=%lld\n",p.clock.rate,host_frequency,host_counter-late,host_frequency/p.clock.rate+1);fflush(stdout);
        assert(host_counter>late && host_counter-late<=host_frequency/p.clock.rate+host_frequency/10000000+1);
        pc_native_movie_pacing_close(&p);assert(!p.timer && !p.clock.rate);
    }
    assert(creates==closes);
    /* Independent playback schedules, reset/disable, failed host waits. */
    host_frequency=10000000;setting="144";host_counter=0;
    pc_native_movie_pacing_wait(&p);pc_native_movie_pacing_wait(&p);
    PcNativeMoviePacing other={0};int64_t start=host_counter;
    pc_native_movie_pacing_wait(&other);assert(host_counter==start && !other.timer);
    setting="60";pc_native_movie_pacing_wait(&p);assert(host_counter==start);
    setting=NULL;pc_native_movie_pacing_wait(&p);assert(host_counter==start && !p.clock.rate);
    setting="144";pc_native_movie_pacing_wait(&p);assert(host_counter==start);
    host_counter=0;pc_native_movie_pacing_wait(&p);assert(host_counter==0);
    for(unsigned mode=0;mode<3;mode++) {
        pc_native_movie_pacing_close(&p);host_counter=0;
        fail_create=mode==0;fail_set=mode==1;fail_wait=mode==2;
        pc_native_movie_pacing_wait(&p);pc_native_movie_pacing_wait(&p);
        assert(host_counter>=host_frequency/144 && host_counter<host_frequency/144+host_frequency/1000);
    }
    fail_create=fail_set=fail_wait=0;fail_clock=1;
    start=host_counter;pc_native_movie_pacing_wait(&p);assert(host_counter==start && !p.clock.rate);
    pc_native_movie_pacing_close(&p);pc_native_movie_pacing_close(&other);
    assert(creates==closes && sleeps);
    puts("PASS: native movie polling cadence, clock independence, late/disabled/reset paths and timer lifetime/failure");
    return 0;
}
