/* Offline measurement bridge to the production Xbox decoder. No host audio,
 * guest code, input files, settings or saves are opened by this library. */
#include <stddef.h>
#include <stdint.h>
#include "hle/xbox_adpcm.h"
#ifdef _WIN32
#define AUDIT_EXPORT __declspec(dllexport)
#else
#define AUDIT_EXPORT __attribute__((visibility("default")))
#endif
AUDIT_EXPORT int audio_audit_decode(const uint8_t *source, size_t bytes,
    unsigned channels, int16_t *pcm, size_t capacity)
{
    if (!source || !pcm || channels < 1 || channels > 2) return 0;
    size_t block = XBOX_ADPCM_BLOCK_BYTES * channels;
    size_t samples = XBOX_ADPCM_BLOCK_SAMPLES * channels;
    if (bytes % block || bytes / block > capacity / samples) return 0;
    for (size_t at = 0; at < bytes; at += block, pcm += samples)
        if (!xbox_adpcm_decode_block(source + at, block, channels, pcm, samples)) return 0;
    return 1;
}
