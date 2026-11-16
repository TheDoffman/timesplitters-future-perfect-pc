#ifndef TSFP_MOD_AUDIO_SOURCE_H
#define TSFP_MOD_AUDIO_SOURCE_H
#include <stdint.h>
#define TSFP_MOD_AUDIO_EVENTS 5703u
#define TSFP_MOD_AUDIO_UNKNOWN UINT32_MAX
/* Metadata is absent when authored_rate is zero. ID-only attribution can still
   be valid; it is not enough to select an audio replacement. */
typedef struct TsfpModAudioSource {
    uint32_t event,authored_rate,channels,looping;
} TsfpModAudioSource;
#ifdef __cplusplus
extern "C" {
#endif
/* Guest-thread attribution only; never changes samples, format or clocks.
   A token bounds leave to this scope; invalid IDs mask a parent's identity.
   Overflow discards all attribution instead of mislabelling an inner source. */
unsigned tsfp_mod_audio_source_enter(uint32_t event,uint32_t guest_sp);
unsigned tsfp_mod_audio_source_enter_metadata(const TsfpModAudioSource *,uint32_t guest_sp);
void tsfp_mod_audio_source_leave(unsigned token);
uint32_t tsfp_mod_audio_source_current(uint32_t guest_sp);
TsfpModAudioSource tsfp_mod_audio_source_snapshot(uint32_t guest_sp);
#ifdef __cplusplus
}
#endif
#endif
#if defined(TSFP_MOD_AUDIO_SOURCE_IMPLEMENTATION) && !defined(TSFP_MOD_AUDIO_SOURCE_IMPLEMENTED)
#define TSFP_MOD_AUDIO_SOURCE_IMPLEMENTED
#ifdef _MSC_VER
#define TSFP_MOD_AUDIO_TLS __declspec(thread)
#elif defined(__cplusplus)
#define TSFP_MOD_AUDIO_TLS thread_local
#else
#define TSFP_MOD_AUDIO_TLS _Thread_local
#endif
static TSFP_MOD_AUDIO_TLS struct {TsfpModAudioSource source;uint32_t sp;} tsfp_audio_sources[8];
static TSFP_MOD_AUDIO_TLS unsigned tsfp_audio_source_count;
TsfpModAudioSource tsfp_mod_audio_source_snapshot(uint32_t guest_sp)
{
    const TsfpModAudioSource unknown={TSFP_MOD_AUDIO_UNKNOWN,0,0,0};
    if(!guest_sp){tsfp_audio_source_count=0;return unknown;}
    /* Guest exception unwinds may skip wrapper leave; never reuse a dead frame. */
    while(tsfp_audio_source_count && guest_sp>=tsfp_audio_sources[tsfp_audio_source_count-1].sp)
        tsfp_audio_source_count--;
    return tsfp_audio_source_count?tsfp_audio_sources[tsfp_audio_source_count-1].source:unknown;
}
uint32_t tsfp_mod_audio_source_current(uint32_t guest_sp)
{return tsfp_mod_audio_source_snapshot(guest_sp).event;}
unsigned tsfp_mod_audio_source_enter_metadata(const TsfpModAudioSource *input,uint32_t guest_sp)
{
    unsigned token;TsfpModAudioSource source={TSFP_MOD_AUDIO_UNKNOWN,0,0,0};
    (void)tsfp_mod_audio_source_current(guest_sp);
    token=tsfp_audio_source_count;
    if(!guest_sp || token>=8){tsfp_audio_source_count=0;return 0;}
    if(input && input->event<TSFP_MOD_AUDIO_EVENTS) {
        source.event=input->event;
        if(input->authored_rate>0 && input->authored_rate<=48000 &&
           (input->channels==1 || input->channels==2) && input->looping<=1) {
            source.authored_rate=input->authored_rate;
            source.channels=input->channels;source.looping=input->looping;
        }
    }
    tsfp_audio_sources[token].sp=guest_sp;
    tsfp_audio_sources[token].source=source;
    tsfp_audio_source_count++;
    return token;
}
unsigned tsfp_mod_audio_source_enter(uint32_t event,uint32_t guest_sp)
{const TsfpModAudioSource source={event,0,0,0};return tsfp_mod_audio_source_enter_metadata(&source,guest_sp);}
void tsfp_mod_audio_source_leave(unsigned token)
{if(token<tsfp_audio_source_count)tsfp_audio_source_count=token;}
#undef TSFP_MOD_AUDIO_TLS
#endif
