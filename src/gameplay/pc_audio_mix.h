#ifndef PC_AUDIO_MIX_H
#define PC_AUDIO_MIX_H
#include <stdint.h>
/* Host categories, independent of original sound flags and completion clocks. */
typedef enum PcAudioBus {PC_AUDIO_EFFECTS,PC_AUDIO_MUSIC,PC_AUDIO_DIALOGUE,
    PC_AUDIO_BACKGROUND,PC_AUDIO_MOVIES,PC_AUDIO_BUS_COUNT} PcAudioBus;
#ifdef __cplusplus
extern "C" {
#endif
unsigned pc_audio_scope_enter(PcAudioBus bus,uint32_t guest_sp);
void pc_audio_scope_leave(unsigned token);
int pc_audio_scope_bus(uint32_t guest_sp); /* -1 outside an established scope */
unsigned pc_audio_stream_scope(uint32_t record,uint32_t sound,uint32_t guest_sp);
#ifdef __cplusplus
}
#endif
/* US1643BD/164D1B call46D80 before the subtitle preference check. US2626FB
 * calls45820 for actor speech before storing its handle at actor+E8C.
 * Other sound callers retain the existing category or ordinary effects. */
static inline int pc_audio_script_speech_caller(uint32_t caller)
{return caller==0x001643c2u || caller==0x00164d20u;}
static inline int pc_audio_actor_speech_caller(uint32_t caller)
{return caller==0x00262700u;}
static inline PcAudioBus pc_audio_stream_category(uint32_t record,uint32_t flags,int scope)
{
    /* US28890/289C0 use this distinct stereo music/cinematic stream. Never
       claim to isolate dialogue already mixed into its recording. */
    if(record==0x0064bd98u)return PC_AUDIO_MUSIC;
    if(record<0x005836d0u || (record-0x005836d0u)%0x14094u ||
       (record-0x005836d0u)/0x14094u>=10u)return PC_AUDIO_EFFECTS;
    if(scope==PC_AUDIO_DIALOGUE)return PC_AUDIO_DIALOGUE;
    /* The same verified looping-stereo policy used by the ambience router. */
    return (flags&0x210u)==0x210u?PC_AUDIO_BACKGROUND:PC_AUDIO_EFFECTS;
}
#endif /* PC_AUDIO_MIX_H */
#if defined(PC_AUDIO_MIX_IMPLEMENTATION) && !defined(PC_AUDIO_MIX_IMPLEMENTED)
#define PC_AUDIO_MIX_IMPLEMENTED
#ifdef _MSC_VER
#define PC_AUDIO_MIX_TLS __declspec(thread)
#else
#define PC_AUDIO_MIX_TLS _Thread_local
#endif
static PC_AUDIO_MIX_TLS struct {PcAudioBus bus;uint32_t sp;} pc_audio_scopes[8];
static PC_AUDIO_MIX_TLS unsigned pc_audio_scope_count;
int pc_audio_scope_bus(uint32_t guest_sp)
{
    if(!guest_sp){pc_audio_scope_count=0;return -1;}
    /* A guest SEH/nonlocal unwind may skip a C wrapper's leave. Pop scopes
       above the current guest stack; do not leak speech ownership afterward. */
    while(pc_audio_scope_count && guest_sp>pc_audio_scopes[pc_audio_scope_count-1].sp)
        pc_audio_scope_count--;
    return pc_audio_scope_count?(int)pc_audio_scopes[pc_audio_scope_count-1].bus:-1;
}
unsigned pc_audio_scope_enter(PcAudioBus bus,uint32_t guest_sp)
{
    (void)pc_audio_scope_bus(guest_sp);unsigned token=pc_audio_scope_count;
    if(guest_sp && bus>=0 && bus<PC_AUDIO_BUS_COUNT && token<8) {
        pc_audio_scopes[token].bus=bus;pc_audio_scopes[token].sp=guest_sp;
        pc_audio_scope_count++;
    }
    return token;
}
void pc_audio_scope_leave(unsigned token)
{if(token<pc_audio_scope_count)pc_audio_scope_count=token;}
#undef PC_AUDIO_MIX_TLS
#endif
