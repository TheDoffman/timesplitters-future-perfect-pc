/* Compile the complete SDL adapter against a deterministic, silent device. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
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
static int refuse;
static uint8_t heard_pcm[320000];static int heard_bytes;
static SDL_AudioStream streams[4096];
static int SDL_InitSubSystem(unsigned flags) {(void)flags;++inits;return !fail_init;}
static SDL_AudioDeviceID SDL_OpenAudioDevice(unsigned id,void *spec) {(void)id;(void)spec;++opened;return !fail_open;}
static void SDL_CloseAudioDevice(SDL_AudioDeviceID id) {(void)id;++closed;}
static void SDL_DestroyAudioStream(SDL_AudioStream *s) {if(s)++destroyed;}
static SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *spec,void *out)
{(void)spec;(void)out;assert(created<4096);return &streams[created++];}
static int SDL_BindAudioStream(SDL_AudioDeviceID id,SDL_AudioStream *s) {(void)id;(void)s;return !fail_bind;}
static int SDL_GetAudioStreamQueued(SDL_AudioStream *s) {(void)s;return queued_bytes;}
static int SDL_SetAudioStreamGain(SDL_AudioStream *s,float gain) {s->gain=gain;return !fail_gain;}
static int SDL_PutAudioStreamData(SDL_AudioStream *s,const void *data,int bytes)
{(void)s;if(refuse || fail_put)return 0;heard_bytes=bytes;assert(bytes<=sizeof heard_pcm);memcpy(heard_pcm,data,bytes);return 1;}
