/* Host-owned developer values on the existing reliable Story channel.
   No camera positions, UI settings or native pointers cross the wire. */
#ifdef _WIN32
#include "pc_developer.h"
typedef struct StoryTools {uint32_t magic,match,serial;PcDeveloperShared values;} StoryTools;
#define STORY_TOOLS_MAGIC 0x31445453u
_Static_assert(sizeof(StoryTools)==28,"Story Tools wire layout");
static StoryTools story_tools_host,story_tools_pending;
static uint32_t story_tools_applied,story_tools_match,story_tools_tick;
static int story_tools_sent;
static int story_tools_valid(const StoryTools *s)
{
    return s->serial && s->values.speed_milli>=50u && s->values.speed_milli<=4000u &&
        s->values.audio_speed<=1u && s->values.god<=1u && s->values.infinite_ammo<=1u;
}
static int story_tools_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || length<sizeof(StoryTools) || length>sizeof(StoryTools)+4u ||
       !guest_writable(payload,sizeof(StoryTools)) || WORD_AT(payload)!=STORY_TOOLS_MAGIC)return 0;
    StoryTools s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(story_network_session() && WORD_AT(0x76b108u) && WORD_AT(0x76b140u)==2u &&
       guest_writable(peer,4u) && WORD_AT(peer)==WORD_AT(0x774040u) &&
       s.match==story_npc_match() && story_tools_valid(&s) &&
       (story_tools_pending.match!=s.match || (int32_t)(s.serial-story_tools_pending.serial)>0))
        story_tools_pending=s;
    return 1;
}
static void story_tools_frame(void)
{
    if(!story_network_session() || !WORD_AT(0x76b108u)) {
        memset(&story_tools_host,0,sizeof story_tools_host);
        memset(&story_tools_pending,0,sizeof story_tools_pending);
        story_tools_applied=story_tools_match=0;story_tools_sent=0;return;
    }
    const uint32_t match=story_npc_match(),role=WORD_AT(0x76b140u);
    if(role==2u) {
        if(story_tools_match!=match) {
            const PcDeveloperShared defaults={1000u,1u,0u,0u};pc_developer_shared_apply(&defaults);
            story_tools_applied=0;story_tools_match=match;
        }
        if(story_tools_pending.match==match && story_tools_valid(&story_tools_pending) &&
           (int32_t)(story_tools_pending.serial-story_tools_applied)>0) {
            pc_developer_shared_apply(&story_tools_pending.values);story_tools_applied=story_tools_pending.serial;
        }
        return;
    }
    if(role!=1u || WORD_AT(0x76b1a4u)!=15u)return;
    PcDeveloperShared values;pc_developer_shared_get(&values);
    if(story_tools_host.match!=match || !story_tools_host.serial || memcmp(&values,&story_tools_host.values,sizeof values)) {
        uint32_t serial=story_tools_host.serial+1u;if(!serial)serial=1;
        story_tools_host=(StoryTools){STORY_TOOLS_MAGIC,match,serial,values};story_tools_sent=0;
    }
    const uint32_t frame=g_esp,node=WORD_AT(0x774028u),tick=WORD_AT(0x7de318u);
    if((story_tools_sent && tick-story_tools_tick<30u) || frame<2048u ||
       !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u))return;
    const uint32_t buffer=frame-512u;WORD_AT(buffer)=0;
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&story_tools_host,sizeof story_tools_host);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-1024u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof story_tools_host+4u,1u);
    restore_registers(&saved);story_tools_tick=tick;story_tools_sent=1;
}
#endif
