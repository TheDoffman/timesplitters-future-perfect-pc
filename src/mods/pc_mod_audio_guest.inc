/* Supported US28240 receives a request at stack+4 with sound event at request+0.
   Attribute only this verified route; music/direct stream callers stay unknown. */
#include "mods/mod_audio_source.h"
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
#include "runtime/pc_guest_span.h"
extern size_t g_xbox_total_ram;
static int pc_mod_audio_word(uint32_t address,uint32_t *out)
{
    uintptr_t offset=(uintptr_t)g_xbox_mem_offset,host;
    PcGuestRegion region={0,0,0};
    if(!out || !address || (uint64_t)address+4u>g_xbox_total_ram ||
       address>UINTPTR_MAX-offset)return 0;
    host=offset+address;
    if(!pc_guest_memory_in_span(&region,host,4u,0))return 0;
    memcpy(out,(const void *)host,4u);return 1;
}
#endif
static void pc_mod_audio_request(void (*original)(void))
{
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    uint32_t request=0,event=TSFP_MOD_AUDIO_UNKNOWN,sp=g_esp;
    if(sp && sp<=UINT32_MAX-8u && pc_mod_audio_word(sp+4u,&request))
        (void)pc_mod_audio_word(request,&event);
    unsigned token=tsfp_mod_audio_source_enter(event,sp);
    original();tsfp_mod_audio_source_leave(token);
#else
    original();
#endif
}
