/* Supported US speech ownership, before sound allocation/initial submission.
 * Helpers touch only host TLS; original routines retain all game side effects. */
#include "gameplay/pc_audio_mix.h"
static void pc_audio_mix_script(void (*original)(void))
{
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    uint32_t caller;memcpy(&caller,(const void *)((uintptr_t)g_esp+g_xbox_mem_offset),4);
    int speech=pc_audio_script_speech_caller(caller);
    unsigned token=speech?pc_audio_scope_enter(PC_AUDIO_DIALOGUE,g_esp):0;
    original();
    if(speech)pc_audio_scope_leave(token);
#else
    original();
#endif
}
static void pc_audio_mix_actor(void (*original)(void))
{
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    uint32_t caller;memcpy(&caller,(const void *)((uintptr_t)g_esp+g_xbox_mem_offset),4);
    int speech=pc_audio_actor_speech_caller(caller);
    unsigned token=speech?pc_audio_scope_enter(PC_AUDIO_DIALOGUE,g_esp):0;
    original();
    if(speech)pc_audio_scope_leave(token);
#else
    original();
#endif
}
static void pc_audio_mix_stream(void (*original)(void))
{
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    uint32_t sound;memcpy(&sound,(const void *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset),4);
    unsigned token=pc_audio_stream_scope(g_ecx,sound,g_esp);
    original();pc_audio_scope_leave(token);
#else
    original();
#endif
}
