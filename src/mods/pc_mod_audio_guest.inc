/* Supported US28240 receives a request at stack+4 with sound event at request+0.
   Attribute only this verified route; music/direct stream callers stay unknown. */
#include "mods/mod_audio_source.h"
static void pc_mod_audio_request(void (*original)(void))
{
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    uint32_t request=0,event=TSFP_MOD_AUDIO_UNKNOWN,sp=g_esp;
    if(sp<=UINT32_MAX-8u && guest_writable(sp+4u,4u)) {
        memcpy(&request,(const void *)((uintptr_t)(sp+4u)+g_xbox_mem_offset),4);
        if(request && guest_writable(request,4u))
            memcpy(&event,(const void *)((uintptr_t)request+g_xbox_mem_offset),4);
    }
    unsigned token=tsfp_mod_audio_source_enter(event,sp);
    original();tsfp_mod_audio_source_leave(token);
#else
    original();
#endif
}
