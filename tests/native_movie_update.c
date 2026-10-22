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
#include <stdlib.h>

#define TSFP_PC_CONTROLS 1
#define MAX_MOVIES 4
#define MAX_CATCH_UP 8u
#define AUDIO_CHUNK 8192u
#define RECOMP_AUDIO_SLOT_MOVIE 1
#define PICTURES 90u
static int unpaced;
static void poll_wait(PcNativeMoviePacing *p) { if(!unpaced)pc_native_movie_pacing_wait(p); }
#define pc_native_movie_pacing_wait poll_wait
/* The real host-player structure/functions are inserted from the patched
 * runtime. Demux, decoding, audio output and rendering below are spies. */
typedef struct { unsigned unused; } xmv_demux;
typedef struct { unsigned unused; } xmv_video_decoder;
/* MOVIE_STRUCT */
static movie g_movies[MAX_MOVIES];
static LARGE_INTEGER g_qpf;
static unsigned packets,decoded,shown,submitted,rejected,audio_sum,submits,resets;
static unsigned pts_log[PICTURES];
static int read_next(movie *m) {
    if(m->have_next || m->video_done)return m->have_next;
    if(packets==PICTURES) { m->video_done=1;return 0; }
    m->next_pts=packets*50;m->next_size=1;m->have_next=1;
    memset(m->pcm+m->pcm_len,(int)packets,32);m->pcm_len+=32;packets++;
    return 1;
}
static int recomp_audio_output_submit(int slot,const uint8_t *bytes,uint32_t n,
    uint32_t rate,uint32_t channels,uint32_t bits,int flags) {
    assert(slot==1 && rate==8000 && channels==2 && bits==16 && !flags && n%4==0);
    if(submits++%3==0) { rejected++;return 0; }
    submitted+=n;for(unsigned i=0;i<n;i++)audio_sum+=bytes[i];return 1;
}
static void recomp_audio_output_reset_voice(int slot) { assert(slot==1);resets++; }
static int recomp_audio_output_position(int slot,uint64_t *played,uint32_t *queued)
{ assert(slot==1);*played=submitted;*queued=0;return 1; }
static int xmv_decoder_decode(xmv_video_decoder *d,const uint8_t *p,uint32_t size,
    uint32_t pts,uint8_t *bgra) {
    assert(size==1 && decoded<PICTURES && pts==decoded*50);
    pts_log[decoded++]=pts;return 1;
}
static void write_surface(movie *m,uint32_t surface) { assert(surface==0x1000); }
static void d3d8_movie_set_frame(uint8_t *bgra,uint32_t w,uint32_t h,int fill,unsigned l,unsigned r)
{ assert(w==720 && h==480 && !fill && !l && !r);shown++; }
static void hle_d3d8_movie_surface(uint32_t data,uint32_t fmt) { assert(!data && !fmt); }
static void d3d8_movie_clear(void) {}
static void xmv_decoder_destroy(xmv_video_decoder *d) {}
static void xmv_close(xmv_demux *d) {}
/* MOVIE_FUNCTIONS */
int main(void) {
    setting="144";host_frequency=10000000;g_qpf.QuadPart=host_frequency;
    unsigned counts[2];
    for(unpaced=1;unpaced>=0;unpaced--) {
        movie *m=&g_movies[0];memset(m,0,sizeof *m);
        packets=decoded=shown=submitted=rejected=audio_sum=submits=resets=0;
        host_counter=0;m->used=1;m->w=720;m->h=480;
        m->audio=1;m->channels=2;m->rate=8000;
        m->pcm=calloc(PICTURES,32);assert(m->pcm);
        unsigned polls=0,status;uint32_t pts=0;
        do {
            status=xmv_play_update(1,0x1000,&pts);polls++;
            if(status==1)assert(pts==pts_log[decoded-1]);
            assert(status<=2 && polls<100000);
            host_counter+=1000; /* 100 microseconds of guest work per poll. */
        } while(status!=2);
        assert(decoded==PICTURES && shown==PICTURES && m->shown==PICTURES);
        assert(submitted==PICTURES*32 && m->pcm_submitted==submitted && rejected);
        assert(audio_sum==(PICTURES-1)*PICTURES/2*32);
        assert(resets==1 && xmv_play_time(1)>=4450 && xmv_play_time(1)<4470);
        counts[unpaced]=polls;
        printf("movie unpaced=%d polls=%u pictures=%u bytes=%u elapsed_ms=%u\n",
               unpaced,polls,decoded,submitted,xmv_play_time(1));
        if(!unpaced)assert(polls<=144*4450/1000+5);
        xmv_play_close(1);assert(!m->used && resets==2 && creates==closes);
        int64_t stopped=host_counter;
        assert(xmv_play_update(1,0x1000,&pts)==2 && host_counter==stopped);
        xmv_play_close(1);assert(resets==2);
    }
    assert(counts[1]>counts[0]*10);
    puts("PASS: actual movie updater retains decoded timestamps/audio/EOF while bounding native polling; close releases timer");
    return 0;
}
