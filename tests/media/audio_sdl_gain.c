/* Compile the complete SDL adapter against a deterministic, silent device. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
typedef int pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static void pthread_mutex_lock(pthread_mutex_t *p) {(void)p;}
static void pthread_mutex_unlock(pthread_mutex_t *p) {(void)p;}
typedef unsigned SDL_AudioDeviceID;
typedef struct {int format,channels,freq;} SDL_AudioSpec;
typedef struct {float gain;} SDL_AudioStream;
enum {SDL_INIT_AUDIO=1,SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,SDL_AUDIO_U8,SDL_AUDIO_S16LE};
static uint64_t sdl_now;
static uint64_t SDL_GetTicks(void) { return sdl_now; }
static int fail_init, fail_open, fail_bind, fail_put, fail_gain, queued_bytes;
static unsigned destroyed, closed, inits;
static int opened,created;
static SDL_AudioStream streams[32];
static int SDL_InitSubSystem(unsigned flags) {(void)flags;++inits;return !fail_init;}
static SDL_AudioDeviceID SDL_OpenAudioDevice(unsigned id,void *spec) {(void)id;(void)spec;++opened;return !fail_open;}
static void SDL_CloseAudioDevice(SDL_AudioDeviceID id) {(void)id;++closed;}
static void SDL_DestroyAudioStream(SDL_AudioStream *s) {if(s)++destroyed;}
static SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *spec,void *out)
{(void)spec;(void)out;assert(created<32);return &streams[created++];}
static int SDL_BindAudioStream(SDL_AudioDeviceID id,SDL_AudioStream *s) {(void)id;(void)s;return !fail_bind;}
static int SDL_GetAudioStreamQueued(SDL_AudioStream *s) {(void)s;return queued_bytes;}
static int SDL_SetAudioStreamGain(SDL_AudioStream *s,float gain) {s->gain=gain;return !fail_gain;}
static int SDL_PutAudioStreamData(SDL_AudioStream *s,const void *data,int bytes)
{(void)s;(void)data;(void)bytes;return !fail_put;}
#include "../src/native/audio_sdl.c"
int main(int argc,char **argv)
{
    uint8_t pcm[64]={0};
    assert(argc==2);
    float expected=strtof(argv[1],NULL);
    recomp_audio_output_initialize();assert(opened==(expected>0));
    if(expected>0) {
        /* Sound buffer, music stream and movie slot use the same output level. */
        const unsigned slots[]={0,256,RECOMP_AUDIO_SLOT_MOVIE};
        for(unsigned i=0;i<3;i++) {
            assert(recomp_audio_output_submit(slots[i],pcm,sizeof pcm,48000,2,16,0));
            assert(fabsf(streams[i].gain-expected)<0.00001f);
            assert(recomp_audio_output_set_volume(slots[i],-600));
            assert(fabsf(streams[i].gain-expected*0.5011872f)<0.00001f);
            assert(recomp_audio_output_set_volume(slots[i],-2000));
            assert(fabsf(streams[i].gain-expected*0.1f)<0.00001f);
            assert(recomp_audio_output_set_volume(slots[i],0));
            assert(fabsf(streams[i].gain-expected)<0.00001f); /* no cumulative reduction */
        }
        assert(opened==1);
    }
    recomp_audio_output_shutdown();
    puts("PASS: actual SDL buffer/music/movie output gain and live volume changes");
    return 0;
}
