#ifndef TSFP_MOD_WAVE_H
#define TSFP_MOD_WAVE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TSFP_MOD_WAVE_MAX_BYTES (64u * 1024u * 1024u)
#define TSFP_MOD_WAVE_MAX_CHUNKS 64u
#define TSFP_MOD_WAVE_MAX_SECONDS 600u
typedef enum TsfpModWaveError {
    TSFP_MOD_WAVE_OK=0, TSFP_MOD_WAVE_ARGUMENT, TSFP_MOD_WAVE_CONTAINER,
    TSFP_MOD_WAVE_CHUNK, TSFP_MOD_WAVE_FORMAT, TSFP_MOD_WAVE_DATA,
    TSFP_MOD_WAVE_LIMIT
} TsfpModWaveError;
/* Borrows validated little-endian PCM bytes from an immutable caller snapshot.
   No allocation, conversion, IO, guest access, decoding or playback occurs.
   Callers must retain the snapshot and its package/hash authorization. */
typedef struct TsfpModWave {
    const uint8_t *pcm;
    uint32_t bytes,frames,rate,channels;
} TsfpModWave;
/* Strict RIFF/WAVE PCM16, 1/2 channels, 8..48kHz, exact sizes, <=10min.
   Output is zero on every failure. Ancillary chunks are skipped, never executed. */
TsfpModWaveError tsfp_mod_wave_parse(const void *,size_t,TsfpModWave *);
const char *tsfp_mod_wave_error(TsfpModWaveError);
#ifdef __cplusplus
}
#endif
#endif
