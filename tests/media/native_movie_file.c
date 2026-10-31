#include "platform/xbox_winnt.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const char *setting;
const char *recomp_config_lookup(const char *env,const char *key)
{ assert(!strcmp(env,"TSFP_NATIVE_FPS") && !strcmp(key,"native_fps"));return setting; }
#ifndef LIVE_HOST_CLOCK
static int64_t host_counter,host_frequency=10000000,deadline;
static unsigned creates,closes,waits,sleeps;
static int fail_create,fail_set,fail_wait,fail_clock;
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
#endif
#define TSFP_PC_CONTROLS 1
#include "hle/hle_xmv_play.c"

static char host_path[2048];
static uint8_t guest[8192];
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_total_ram=sizeof guest;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp;
static uint64_t image_hash=1469598103934665603ull,audio_hash=1469598103934665603ull,audio_bytes;
static unsigned image_count;
#ifdef LIVE_HOST_CLOCK
static double intervals[10000];
static int compare_intervals(const void *a,const void *b)
{ double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y); }
#endif
static uint64_t hash_bytes(uint64_t hash,const uint8_t *p,size_t n)
{ for(size_t i=0;i<n;i++)hash=(hash^p[i])*1099511628211ull;return hash; }
int xbox_EnvSwitch(const char *name,int fallback) { return fallback; }
BOOL xbox_translate_path(const char *path,WCHAR *out,DWORD count)
{ assert(!strcmp(path,"d:\\xmv\\eag_e.xmv"));return MultiByteToWideChar(CP_UTF8,0,host_path,-1,out,count)!=0; }
void hle_d3d8_movie_surface(uint32_t data,uint32_t format) { assert(!data && !format); }
void d3d8_movie_clear(void) {}
void d3d8_movie_set_frame(const uint8_t *pixels,uint32_t w,uint32_t h,int fill,unsigned l,unsigned r)
{ assert(w && h);image_hash=hash_bytes(image_hash,pixels,(size_t)w*h*4);image_count++; }
static int host_discontinuity, refuse_audio;
static uint8_t first_pcm;
int recomp_audio_output_take_discontinuity(uint32_t slot) { int v=host_discontinuity; host_discontinuity=0; return v; }
void recomp_audio_output_reset_voice(uint32_t slot) { assert(slot==RECOMP_AUDIO_SLOT_MOVIE); }
int recomp_audio_output_submit(uint32_t slot,const uint8_t *pcm,uint32_t n,uint32_t rate,uint32_t channels,uint32_t bits,int32_t volume)
{ assert(slot==RECOMP_AUDIO_SLOT_MOVIE && rate && channels && bits==16); if(refuse_audio)return 0; first_pcm=pcm[0]; audio_hash=hash_bytes(audio_hash,pcm,n);audio_bytes+=n;return 1; }
int recomp_audio_output_position(uint32_t slot,uint64_t *played,uint32_t *queued)
{ assert(slot==RECOMP_AUDIO_SLOT_MOVIE);*played=audio_bytes;*queued=0;return 1; }
#ifndef LIVE_HOST_CLOCK
static void test_audio_recovery(void) {
    movie m = {0}; uint8_t pcm[9600];
    m.rate=48000; m.channels=1; m.pcm=pcm; m.pcm_len=sizeof pcm;
    m.pcm_submitted=9600; g_qpf.QuadPart=host_frequency;
    memset(pcm,0x11,4800); memset(pcm+4800,0x77,4800);
    host_counter=host_frequency/8; /* 125 ms: discard 25 ms of pending sound */
    host_discontinuity=1; refuse_audio=1;
    feed_audio(&m); assert(m.pcm_off==2400 && m.pcm_submitted==12000);
    host_counter=host_frequency*3/20; /* 150 ms: recovery uses the current offset */
    refuse_audio=0; feed_audio(&m);
    assert(m.pcm_off==9600 && m.pcm_submitted==19200 && first_pcm==0x77);
    assert(!m.audio_recovering && audio_bytes==4800);
    audio_bytes=0; audio_hash=1469598103934665603ull; host_counter=0;
}
#endif
int main(int argc,char **argv) {
#ifndef LIVE_HOST_CLOCK
    test_audio_recovery();
#endif
    assert(argc==3 && strlen(argv[1])<sizeof host_path);strcpy(host_path,argv[1]);
    setting=strcmp(argv[2],"original")?argv[2]:NULL;
    g_xbox_mem_offset=(ptrdiff_t)guest;strcpy((char *)guest+0x1000,"d:\\xmv\\eag_e.xmv");
    uint32_t width,height,pts=0;int handle=xmv_play_open(0x1000,&width,&height);assert(handle>0);
    unsigned polls=0,status;uint64_t timestamp_hash=1469598103934665603ull;
#ifdef LIVE_HOST_CLOCK
    assert(setting); /* The unchanged console flip gate is outside this test. */
    LARGE_INTEGER start,last,now,freq;
    QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&start);last=start;
#endif
    do {
        status=xmv_play_update(handle,0,&pts);polls++;
        if(status==1)timestamp_hash=hash_bytes(timestamp_hash,(const uint8_t *)&pts,4);
#ifdef LIVE_HOST_CLOCK
        QueryPerformanceCounter(&now);
        assert(polls<=10000 && now.QuadPart-start.QuadPart<freq.QuadPart*10);
        if(polls>1)intervals[polls-2]=(double)(now.QuadPart-last.QuadPart)*1000/freq.QuadPart;
        last=now;
#else
        assert(polls<1000000 && host_counter<host_frequency*60);
        /* Original console path's flip wait is modeled at 60 Hz. The native
         * movie path must impose its own cap on a much faster guest loop. */
        if(status!=2)host_counter+=setting?1000:host_frequency/60;
#endif
    } while(status!=2);
    assert(image_count>30 && audio_bytes>1000);
    printf("RESULT rate=%s polls=%u pictures=%u duration_ms=%u image_hash=%016llx timestamp_hash=%016llx audio_bytes=%llu audio_hash=%016llx\n",
        argv[2],polls,image_count,xmv_play_time(handle),image_hash,timestamp_hash,audio_bytes,audio_hash);
#ifdef LIVE_HOST_CLOCK
    HANDLE timer=g_movies[handle-1].pacing.timer;assert(timer);
    unsigned n=polls-1;qsort(intervals,n,sizeof *intervals,compare_intervals);
    double total=0;for(unsigned i=0;i<n;i++)total+=intervals[i];
    printf("TIMING rate=%s intervals=%u mean_ms=%.6f p50_ms=%.6f p95_ms=%.6f p99_ms=%.6f max_ms=%.6f\n",
           argv[2],n,total/n,intervals[n/2],intervals[n*95/100],intervals[n*99/100],intervals[n-1]);
    xmv_play_close(handle);
    DWORD flags;assert(!GetHandleInformation(timer,&flags) && GetLastError()==ERROR_INVALID_HANDLE);
    assert(!g_movies[handle-1].pacing.timer && !g_movies[handle-1].used);
#else
    xmv_play_close(handle);assert(creates==closes);
#endif
    puts("PASS: real XMV decode and PCM capture completed through paced player; GPU/audio-device output mocked");
    return 0;
}
