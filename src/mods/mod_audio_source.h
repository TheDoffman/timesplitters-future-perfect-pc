#ifndef TSFP_MOD_AUDIO_SOURCE_H
#define TSFP_MOD_AUDIO_SOURCE_H
#include <stdint.h>
#define TSFP_MOD_AUDIO_EVENTS 5703u
#define TSFP_MOD_AUDIO_UNKNOWN UINT32_MAX
#ifdef __cplusplus
extern "C" {
#endif
/* Guest-thread attribution only; never changes samples, format or clocks.
   A token bounds leave to this scope; invalid IDs mask a parent's identity.
   Overflow discards all attribution instead of mislabelling an inner source. */
unsigned tsfp_mod_audio_source_enter(uint32_t event,uint32_t guest_sp);
void tsfp_mod_audio_source_leave(unsigned token);
uint32_t tsfp_mod_audio_source_current(uint32_t guest_sp);
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
static TSFP_MOD_AUDIO_TLS struct {uint32_t event,sp;} tsfp_audio_sources[8];
static TSFP_MOD_AUDIO_TLS unsigned tsfp_audio_source_count;
uint32_t tsfp_mod_audio_source_current(uint32_t guest_sp)
{
    if(!guest_sp){tsfp_audio_source_count=0;return TSFP_MOD_AUDIO_UNKNOWN;}
    /* Guest exception unwinds may skip wrapper leave; never reuse a dead frame. */
    while(tsfp_audio_source_count && guest_sp>=tsfp_audio_sources[tsfp_audio_source_count-1].sp)
        tsfp_audio_source_count--;
    return tsfp_audio_source_count?tsfp_audio_sources[tsfp_audio_source_count-1].event:TSFP_MOD_AUDIO_UNKNOWN;
}
unsigned tsfp_mod_audio_source_enter(uint32_t event,uint32_t guest_sp)
{
    unsigned token;
    (void)tsfp_mod_audio_source_current(guest_sp);
    token=tsfp_audio_source_count;
    if(!guest_sp || token>=8){tsfp_audio_source_count=0;return 0;}
    tsfp_audio_sources[token].sp=guest_sp;
    tsfp_audio_sources[token].event=event<TSFP_MOD_AUDIO_EVENTS?event:TSFP_MOD_AUDIO_UNKNOWN;
    tsfp_audio_source_count++;
    return token;
}
void tsfp_mod_audio_source_leave(unsigned token)
{if(token<tsfp_audio_source_count)tsfp_audio_source_count=token;}
#undef TSFP_MOD_AUDIO_TLS
#endif
