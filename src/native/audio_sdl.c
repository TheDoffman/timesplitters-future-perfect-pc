/* SDL3 output for HLE PCM on macOS. Guest timing retains its wall-clock fallback:
 * SDL's queued-byte count is not a hardware playback cursor. */
#include "audio_output.h"
#include <SDL3/SDL.h>
#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SLOT_COUNT (RECOMP_AUDIO_SLOT_MOVIE + 1)
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static SDL_AudioDeviceID device;
static int initialized;
static float master_gain = RECOMP_AUDIO_DEFAULT_GAIN;
static struct { SDL_AudioStream *stream; SDL_AudioSpec spec; } voices[SLOT_COUNT];

void recomp_audio_output_initialize(void)
{
    pthread_mutex_lock(&lock);
    if (!initialized) {
        initialized = 1;
        const char *gain = getenv("RECOMP_AUDIO_GAIN");
        if (gain) { float v = strtof(gain, NULL); if (isfinite(v)) master_gain = fmaxf(0, fminf(1, v)); }
        if (master_gain > 0 && SDL_InitSubSystem(SDL_INIT_AUDIO))
            device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        fprintf(stderr, "[NATIVE-AUDIO] %s\n", device ? "SDL output opened" : "output unavailable or muted");
    }
    pthread_mutex_unlock(&lock);
}

void recomp_audio_output_reset_voice(uint32_t slot)
{
    if (slot >= SLOT_COUNT) return;
    pthread_mutex_lock(&lock);
    SDL_DestroyAudioStream(voices[slot].stream);
    voices[slot].stream = NULL;
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
    pthread_mutex_unlock(&lock);
}

int recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t rate, uint32_t channels, uint32_t bits, int32_t volume)
{
    if (slot >= SLOT_COUNT || !pcm || !bytes || bytes > 160000 || rate < 1000 || rate > 200000 ||
        (channels != 1 && channels != 2) || (bits != 8 && bits != 16) || bytes % (channels * bits / 8)) return 0;
    recomp_audio_output_initialize();
    pthread_mutex_lock(&lock);
    int ok = 0;
    if (!device) goto done;
    SDL_AudioSpec spec = { .format = bits == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE,
                          .channels = (int)channels, .freq = (int)rate };
    if (voices[slot].stream && (voices[slot].spec.format != spec.format ||
        voices[slot].spec.channels != spec.channels || voices[slot].spec.freq != spec.freq)) {
        SDL_DestroyAudioStream(voices[slot].stream); voices[slot].stream = NULL;
    }
    if (!voices[slot].stream) {
        SDL_AudioStream *s = SDL_CreateAudioStream(&spec, NULL);
        if (!s || !SDL_BindAudioStream(device, s)) { SDL_DestroyAudioStream(s); goto done; }
        voices[slot].stream = s; voices[slot].spec = spec;
    }
    SDL_AudioStream *s = voices[slot].stream;
    int queued = SDL_GetAudioStreamQueued(s);
    /* Bound each voice to at most half a second plus one accepted packet. */
    if (queued < 0 || (uint32_t)queued > rate * channels * (bits / 8) / 2) goto done;
    SDL_SetAudioStreamGain(s, master_gain * powf(10.0f, fminf(0, (float)volume) / 2000.0f));
    ok = SDL_PutAudioStreamData(s, pcm, (int)bytes);
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
    pthread_mutex_unlock(&lock);
    return ok;
}
int recomp_audio_output_position(uint32_t slot, uint64_t *played, uint32_t *queued)
{ (void)slot; (void)played; (void)queued; return 0; }
int recomp_audio_output_engine_stats(RecompAudioEngineStats *stats)
{ if (stats) memset(stats, 0, sizeof *stats); return 0; }
