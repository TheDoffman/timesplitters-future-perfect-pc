/* SDL3 output for HLE PCM on macOS. Guest timing retains its wall-clock fallback:
 * SDL's queued-byte count is not a hardware playback cursor. */
#include "audio_output.h"
#include "gameplay/pc_spatial_audio.h"
#include "gameplay/pc_acoustic_dsp.h"
#include <SDL3/SDL.h>
#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SLOT_COUNT (RECOMP_AUDIO_SLOT_MOVIE + 1)
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_AudioDeviceID device;
static int initialized, stopped, audio_initialized;
static uint64_t retry_ms;
static int discontinuity[SLOT_COUNT];
static RecompAudioEngineStats health; /* protected by lock; lifetime counters */
static float master_gain = RECOMP_AUDIO_DEFAULT_GAIN;
static struct { SDL_AudioStream *stream; SDL_AudioSpec spec; uint64_t retry_ms; } voices[SLOT_COUNT];
static struct {float pan;int enabled;} spatial[SLOT_COUNT];
static PcAcousticDsp acoustics[SLOT_COUNT];
static uint8_t acoustic_pcm[320000];
static PcAcousticDsp acoustic_backup; /* serialized by lock; refusal must not consume DSP history */
/* SDL copies these bytes before returning. Serialize scratch and routing with
 * the existing output mutex. Mono always uses stereo input, so enabling 3D
 * never recreates a stream or discards sound already queued. */
static uint8_t stereo_pcm[320000];

void recomp_audio_output_set_spatial(uint32_t slot,float pan,int enabled)
{
    if(slot>=SLOT_COUNT)return;
    pthread_mutex_lock(&lock);
    spatial[slot].pan=isfinite(pan)?fmaxf(-1,fminf(1,pan)):0;
    spatial[slot].enabled=!!enabled;
    pthread_mutex_unlock(&lock);
}

void recomp_audio_output_set_acoustics(uint32_t slot,float obstruction,float room)
{
    if(slot>=SLOT_COUNT)return;
    pthread_mutex_lock(&lock);
    pc_acoustic_target(&acoustics[slot],obstruction,room);
    pthread_mutex_unlock(&lock);
}
void recomp_audio_output_set_ambience(uint32_t slot,int enabled)
{
    if(slot>=SLOT_COUNT)return;
    pthread_mutex_lock(&lock);
    pc_acoustic_ambience_target(&acoustics[slot],enabled);
    pthread_mutex_unlock(&lock);
}

void recomp_audio_output_initialize(void)
{
    pthread_mutex_lock(&lock);
    if (!initialized) {
        initialized = 1;
        const char *gain = getenv("RECOMP_AUDIO_GAIN");
        if (gain) { float v = strtof(gain, NULL); if (isfinite(v)) master_gain = fmaxf(0, fminf(1, v)); }
    }
    if (!stopped && !device && master_gain > 0 && SDL_GetTicks() >= retry_ms) {
        health.open_attempts++;
        if (!audio_initialized) audio_initialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
        if (audio_initialized)
            device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        if (device) health.opens++; else health.device_failed++;
        retry_ms = SDL_GetTicks() + RECOMP_AUDIO_RETRY_MS;
        fprintf(stderr, "[NATIVE-AUDIO] %s\n", device ? "SDL output opened" : "output unavailable; retry pending");
    }
    pthread_mutex_unlock(&lock);
}

/* Called with lock held; a stream failure does not tear down other voices. */
static void fail_voice(uint32_t slot)
{
    health.voice_failed++;
    SDL_DestroyAudioStream(voices[slot].stream);
    voices[slot].stream = NULL;
    voices[slot].retry_ms = SDL_GetTicks() + RECOMP_AUDIO_RETRY_MS;
    discontinuity[slot] = 1;
}

void recomp_audio_output_reset_voice(uint32_t slot)
{
    if (slot >= SLOT_COUNT) return;
    pthread_mutex_lock(&lock);
    SDL_DestroyAudioStream(voices[slot].stream);
    voices[slot].stream = NULL;
    voices[slot].retry_ms = 0;
    discontinuity[slot] = 0;
    spatial[slot].pan=0;spatial[slot].enabled=0;
    memset(&acoustics[slot],0,sizeof acoustics[slot]);
    pthread_mutex_unlock(&lock);
}

void recomp_audio_output_shutdown(void)
{
    pthread_mutex_lock(&lock);
    for (unsigned i = 0; i < SLOT_COUNT; i++) {
        SDL_DestroyAudioStream(voices[i].stream); voices[i].stream = NULL;
    }
    if (device) SDL_CloseAudioDevice(device);
    device = 0;
    stopped = 1;
    pthread_mutex_unlock(&lock);
}

int recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t rate, uint32_t channels, uint32_t bits, int32_t volume)
{
    if (slot >= SLOT_COUNT || !pcm || !bytes || bytes > 160000 || rate < 1000 || rate > 200000 ||
        (channels != 1 && channels != 2) || (bits != 8 && bits != 16) || bytes % (channels * bits / 8)) {
        pthread_mutex_lock(&lock); health.invalid_pcm++; pthread_mutex_unlock(&lock);
        return 0;
    }
    recomp_audio_output_initialize();
    pthread_mutex_lock(&lock);
    int ok = 0;
    if (!device) { health.unavailable++; discontinuity[slot] = 1; goto done; }
    uint32_t input_channels=channels;
    if(channels==1 || spatial[slot].enabled) {
        float gains[2]={1,1};
        if(spatial[slot].enabled)recomp_spatial_gains(spatial[slot].pan,&gains[0],&gains[1]);
        uint32_t frames=bytes/(input_channels*(bits/8));
        for(uint32_t frame=0;frame<frames;frame++)for(unsigned ch=0;ch<2;ch++) {
            uint32_t source=(frame*input_channels+(input_channels==1?0:ch))*(bits/8);
            uint32_t target=(frame*2+ch)*(bits/8);
            if(bits==8)stereo_pcm[target]=(uint8_t)(128+(int)(((int)pcm[source]-128)*gains[ch]));
            else {
                int16_t sample;memcpy(&sample,pcm+source,2);
                sample=(int16_t)(sample*gains[ch]);memcpy(stereo_pcm+target,&sample,2);
            }
        }
        pcm=stereo_pcm;bytes=frames*2*(bits/8);channels=2;
    }
    SDL_AudioSpec spec = { .format = bits == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE,
                          .channels = (int)channels, .freq = (int)rate };
    if (voices[slot].stream && (voices[slot].spec.format != spec.format ||
        voices[slot].spec.channels != spec.channels || voices[slot].spec.freq != spec.freq)) {
        SDL_DestroyAudioStream(voices[slot].stream); voices[slot].stream = NULL;
    }
    if (!voices[slot].stream) {
        if (SDL_GetTicks() < voices[slot].retry_ms) { health.retry_wait++; goto done; }
        SDL_AudioStream *s = SDL_CreateAudioStream(&spec, NULL);
        if (!s || !SDL_BindAudioStream(device, s)) {
            health.voice_failed++;
            SDL_DestroyAudioStream(s);
            voices[slot].retry_ms = SDL_GetTicks() + RECOMP_AUDIO_RETRY_MS;
            discontinuity[slot] = 1;
            goto done;
        }
        voices[slot].stream = s; voices[slot].spec = spec;
    }
    SDL_AudioStream *s = voices[slot].stream;
    int queued = SDL_GetAudioStreamQueued(s);
    /* Bound each voice to at most half a second plus one accepted packet. */
    if (queued < 0) { fail_voice(slot); goto done; }
    if ((uint32_t)queued > rate * channels * (bits / 8) / 2) { health.queue_full++; goto done; }
    int filtered=acoustics[slot].target || acoustics[slot].amount || acoustics[slot].wet ||
                 acoustics[slot].bed || acoustics[slot].bed_amount;
    if(filtered) {
        acoustic_backup=acoustics[slot];
        memcpy(acoustic_pcm,pcm,bytes);
        pc_acoustic_pcm(&acoustics[slot],acoustic_pcm,bytes,rate,channels,bits);
        pcm=acoustic_pcm;
    }
    ok = SDL_SetAudioStreamGain(s, master_gain * powf(10.0f, fminf(0, (float)volume) / 2000.0f)) &&
        SDL_PutAudioStreamData(s, pcm, (int)bytes);
    if(!ok && filtered)acoustics[slot]=acoustic_backup;
    if (!ok) fail_voice(slot);
done:
    pthread_mutex_unlock(&lock);
    return ok;
}

int recomp_audio_output_set_volume(uint32_t slot, int32_t volume)
{
    if (slot >= SLOT_COUNT) return 0;
    pthread_mutex_lock(&lock);
    int ok = voices[slot].stream && SDL_SetAudioStreamGain(voices[slot].stream,
        master_gain * powf(10.0f, fminf(0, (float)volume) / 2000.0f));
    if (!ok && voices[slot].stream) fail_voice(slot);
    pthread_mutex_unlock(&lock);
    return ok;
}
int recomp_audio_output_position(uint32_t slot, uint64_t *played, uint32_t *queued)
{ (void)slot; (void)played; (void)queued; return 0; }
int recomp_audio_output_engine_stats(RecompAudioEngineStats *stats)
{
    if (!stats) return 0;
    pthread_mutex_lock(&lock);
    *stats = health;
    pthread_mutex_unlock(&lock);
    /* SDL has no equivalent hardware latency/glitch snapshot here. */
    return 0;
}

int recomp_audio_output_take_discontinuity(uint32_t slot)
{
    if (slot >= SLOT_COUNT) return 0;
    pthread_mutex_lock(&lock);
    int lost = discontinuity[slot];
    discontinuity[slot] = 0;
    pthread_mutex_unlock(&lock);
    return lost;
}
