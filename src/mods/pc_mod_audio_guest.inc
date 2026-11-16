/* Supported US28240 receives a request at stack+4 with sound event at request+0.
   Attribute only this verified route; music/direct stream callers stay unknown. */
#include "mods/mod_audio_source.h"
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
#include "runtime/pc_guest_span.h"
extern size_t g_xbox_total_ram;
static int pc_mod_audio_read(uint32_t address,void *out,size_t size)
{
    uintptr_t offset=(uintptr_t)g_xbox_mem_offset,host;
    PcGuestRegion region={0,0,0};
    if(!out || !size || !address || size>g_xbox_total_ram || address>g_xbox_total_ram-size ||
       address>UINTPTR_MAX-offset)return 0;
    host=offset+address;
    if(!pc_guest_memory_in_span(&region,host,size,0))return 0;
    memcpy(out,(const void *)host,size);return 1;
}
static int pc_mod_audio_word(uint32_t address,uint32_t *out)
{return pc_mod_audio_read(address,out,4u);}
static void pc_mod_audio_metadata(TsfpModAudioSource *source)
{
    /* Supported XBE: US28532 reads event+4 and the35-entry Q12 table.
       US282C2 reads event+5; shared stream classification uses flags at+8.
       Pin selected Q12 values to original image metadata, not buffer defaults. */
    static const uint16_t q12[35]={500,600,684,938,1053,1130,1166,1168,1182,1327,
        1364,1366,1368,1406,1489,1578,1672,1681,1771,1811,1881,2100,2106,2136,
        2232,2236,2364,2436,2693,2736,2737,3036,3543,3754,3762};
    uint32_t table=0,flags=0;uint64_t at;uint8_t record[20];uint16_t frequency=0;
    if(source->event>=TSFP_MOD_AUDIO_EVENTS || !pc_mod_audio_word(0x4ca9b4u,&table) || !table)return;
    at=(uint64_t)table+(uint64_t)source->event*20u;
    if(at>UINT32_MAX || !pc_mod_audio_read((uint32_t)at,record,sizeof record) || record[4]>=35u)return;
    if(!pc_mod_audio_read(0x4ca9b8u+(uint32_t)record[4]*2u,&frequency,2u) || frequency!=q12[record[4]])return;
    memcpy(&flags,record+8,4u);
    source->authored_rate=(uint32_t)frequency*48000u/4096u;
    source->channels=(flags&0x200u)?2u:1u;source->looping=(flags&0x10u)?1u:0u;
}
#endif
static void pc_mod_audio_request(void (*original)(void))
{
#if defined(TSFP_PC_CONTROLS) || defined(TSFP_NATIVE_DISPLAY)
    uint32_t request=0,event=TSFP_MOD_AUDIO_UNKNOWN,sp=g_esp;
    if(sp && sp<=UINT32_MAX-8u && pc_mod_audio_word(sp+4u,&request))
        (void)pc_mod_audio_word(request,&event);
    TsfpModAudioSource source={event,0,0,0};pc_mod_audio_metadata(&source);
    unsigned token=tsfp_mod_audio_source_enter_metadata(&source,sp);
    original();tsfp_mod_audio_source_leave(token);
#else
    original();
#endif
}
