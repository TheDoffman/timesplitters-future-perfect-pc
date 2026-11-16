#ifndef TSFP_MOD_AUDIO_PLAYER_H
#define TSFP_MOD_AUDIO_PLAYER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TSFP_MOD_AUDIO_VOICES 32u
/* Trusted simulation-thread controls, not a VM entry point. Caller must enforce
 * match/offline authority. Resources come only from committed bootstrap assets.
 * Q16 pitch: 0 resets to authored WAV rate; otherwise 0.25..4. Volume: -10000..0
 * hundredths of a dB, followed by the existing effects-bus/master controls.
 * No file IO or decoding; state/scratch are fixed. The existing backend owns
 * its bounded copied submission queues and allocates them lazily.
 * 1 accepted control, 0 invalid/unavailable, leaving existing voices unchanged. */
int pc_mod_audio_play(uint32_t voice,uint32_t resource,uint32_t pitch_q16,int32_t volume,int loop);
int pc_mod_audio_pause(uint32_t voice,int paused);
int pc_mod_audio_pitch(uint32_t voice,uint32_t pitch_q16);
int pc_mod_audio_volume(uint32_t voice,int32_t volume);
void pc_mod_audio_stop(uint32_t voice);
void pc_mod_audio_stop_all(void);
#ifdef __cplusplus
}
#endif
#endif
