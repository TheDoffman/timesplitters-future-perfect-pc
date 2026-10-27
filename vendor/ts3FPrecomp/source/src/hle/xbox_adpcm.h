/*
 * xbox_adpcm.h -- Xbox ADPCM (format tag 0x69) block decoder.
 *
 * From doaxbv-re (https://github.com/NoRain211/doaxbv-re,
 * recomp-runtime/xbox_adpcm.h), GPL-3.0. Changed apart from this header only
 * in the return-value comment below (block headers are no longer refused).
 */
#ifndef XBOXRECOMP_XBOX_ADPCM_H
#define XBOXRECOMP_XBOX_ADPCM_H

#include <stddef.h>
#include <stdint.h>

#define XBOX_ADPCM_BLOCK_BYTES 36u
#define XBOX_ADPCM_BLOCK_SAMPLES 64u

#ifdef __cplusplus
extern "C" {
#endif

/* Decode one mono/stereo block to interleaved signed 16-bit PCM.
   Sizes above are per channel; pcm_samples is the output capacity in samples.
   Input and output must not overlap. Returns 1 on success, or 0 without
   changing output for invalid pointers, sizes or channels. A header's
   reserved byte is ignored and its step index is clamped to 0..88, as the
   hardware decodes them. */
int xbox_adpcm_decode_block(
    const uint8_t *block, size_t block_bytes, unsigned channels,
    int16_t *pcm, size_t pcm_samples);

#ifdef __cplusplus
}
#endif

#endif
