/* Successful firing events carry numeric weapon state, never model handles.
   The owning peer emits human shots; only the host emits NPC shots and relays
   guest events. Replay is cosmetic and cannot create another damage request. */
#ifdef _WIN32
#include "pc_network_bots_state.h"
typedef struct StoryShot {uint32_t magic,match,slot;PcNetworkBotShot shot;} StoryShot;
#define STORY_SHOT_MAGIC 0x31544853u
_Static_assert(sizeof(StoryShot)+4u<=256u,"Story shot sender limit");
static StoryShot story_shots_pending[32];
static uint32_t story_shots_sent[32],story_shots_seen[32];
static int story_shots_replaying;
static void story_shots_reset(void)
{
    memset(story_shots_pending,0,sizeof story_shots_pending);
    memset(story_shots_sent,0,sizeof story_shots_sent);
    memset(story_shots_seen,0,sizeof story_shots_seen);story_shots_replaying=0;
}
static int story_shots_scope(void)
{return story_npc_session() && WORD_AT(0x0076b1a4u)==15u && WORD_AT(0x0079094cu)<100u;}
static int story_shots_owned(unsigned slot)
{
    const uint32_t contexts=WORD_AT(0x007b0c48u),context=contexts+slot*0x1584u;
    return slot<story_npc_humans() && contexts && guest_writable(context,0x1584u) &&
        WORD_AT(context)==slot && WORD_AT(context+4u)<4u;
}
static int story_shots_send(const StoryShot *s)
{
    const uint32_t frame=g_esp,node=WORD_AT(0x00774028u);
    if(frame<1024u || !guest_writable(frame-1024u,1024u) || !guest_writable(node,0x35u)) return 0;
    const uint32_t buffer=frame-256u;WORD_AT(buffer)=0;
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),s,sizeof *s);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-768u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof *s+4u,1u);
    restore_registers(&saved);return 1;
}
static int story_shots_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || !guest_writable(payload,4u) || WORD_AT(payload)!=STORY_SHOT_MAGIC) return 0;
    if(!story_shots_scope() || length<sizeof(StoryShot) || length>sizeof(StoryShot)+4u ||
       !guest_writable(payload,sizeof(StoryShot)) || !guest_writable(peer,0x40u)) return 1;
    StoryShot s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(s.match!=story_npc_match() || s.slot>=32u || !s.shot.serial || !pc_network_bot_shot_valid(&s.shot,32u) ||
       (int32_t)(s.shot.serial-story_shots_seen[s.slot])<=0) return 1;
    const unsigned role=WORD_AT(0x0076b140u);
    if(role==1u) {
        /* A guest may publish only the human slot assigned to its peer. */
        if(s.slot>=story_npc_humans() || WORD_AT(peer+0x3cu)!=s.slot || story_shots_owned(s.slot)) return 1;
        story_shots_seen[s.slot]=s.shot.serial;story_shots_pending[s.slot]=s;
    } else if(role==2u && WORD_AT(peer)==WORD_AT(0x00774040u)) {
        story_shots_seen[s.slot]=s.shot.serial;
        if(!story_shots_owned(s.slot)) story_shots_pending[s.slot]=s;
    }
    return 1;
}
static void story_shots_frame(void)
{
    if(!story_shots_scope()) return;
    const unsigned role=WORD_AT(0x0076b140u);const uint32_t pool=WORD_AT(0x007356d8u);
    for(unsigned slot=0;slot<32u;slot++) {
        StoryShot *s=&story_shots_pending[slot];
        if(!s->shot.serial || s->match!=story_npc_match()) continue;
        if(role==1u || story_shots_owned(slot)) {if(story_shots_send(s)) s->shot.serial=0;continue;}
        if(role!=2u || !pool || !guest_writable(pool+slot*0xea0u,0xea0u)) continue;
        const uint32_t entity=WORD_AT(pool+slot*0xea0u);
        if(!guest_writable(entity,0x25cu) || WORD_AT(entity+0x258u)!=s->shot.spawn) continue;
        if((uint32_t)(WORD_AT(0x007de318u)-s->shot.tick)>120u) {s->shot.serial=0;continue;}
        const uint32_t weapon=pool+slot*0xea0u+0xa4u+s->shot.hand*0x130u;
        if(WORD_AT(weapon+0x30u)!=(uint32_t)s->shot.weapon ||
           (pc_network_bot_shot_needs_model(s->shot.weapon) && !guest_writable(WORD_AT(weapon+0xe8u),0x24u))) continue;
        GuestRegisters saved;save_registers(&saved);story_shots_replaying=1;
        pc_network_bots_present_shot(entity,&s->shot);
        story_shots_replaying=0;restore_registers(&saved);s->shot.serial=0;
    }
}
static int story_shots_damage(void)
{
    if(!story_shots_scope() || WORD_AT(0x0076b140u)!=2u || WORD_AT(0x0076b170u)) return 0;
    if(story_shots_replaying) return 1;
    if(!guest_writable(g_esp,8u)) return 0;
    const uint32_t context=WORD_AT(g_esp+4u),pool=WORD_AT(0x007356d8u);
    if(!pool || !guest_writable(context,12u)) return 0;
    for(unsigned offset=4u;offset<=8u;offset+=4u) {
        const uint32_t source=WORD_AT(context+offset);
        if(!guest_writable(source,0x80u)) continue;
        const uint32_t actor=WORD_AT(source+0x7cu);
        if(actor<pool || (actor-pool)%0xea0u || (actor-pool)/0xea0u>=32u || !guest_writable(actor,0xea0u)) continue;
        const unsigned slot=(actor-pool)/0xea0u;
        if(!story_shots_owned(slot) && (WORD_AT(actor)==source || WORD_AT(source+0x20u)==8u)) return 1;
    }
    return 0;
}
#endif
int pc_story_network_shot(void (*original)(void))
{
#ifdef _WIN32
    if(!story_shots_scope() || story_shots_replaying || !guest_writable(g_esp,20u)) return 0;
    const uint32_t frame=g_esp,entity=WORD_AT(frame+4u),pool=WORD_AT(0x007356d8u);
    if(!pool || !guest_writable(entity,0x25cu)) return 0;
    const uint32_t actor=WORD_AT(entity+0x7cu);
    if(actor<pool || (actor-pool)%0xea0u || (actor-pool)/0xea0u>=32u || !guest_writable(actor,0xea0u) || WORD_AT(actor)!=entity) return 0;
    const unsigned slot=(actor-pool)/0xea0u;
    if(!story_shots_owned(slot) && !(slot>=story_npc_humans() && WORD_AT(0x0076b140u)==1u)) return 0;
    StoryShot s;memset(&s,0,sizeof s);s.magic=STORY_SHOT_MAGIC;s.match=story_npc_match();s.slot=slot;
    PcNetworkBotShot *shot=&s.shot;shot->hand=WORD_AT(frame+8u);shot->mode=WORD_AT(frame+12u);shot->flags=WORD_AT(frame+16u);
    if(shot->hand>1u) return 0;
    shot->serial=story_shots_sent[slot]+1u;if(!shot->serial) shot->serial=1;
    shot->spawn=WORD_AT(entity+0x258u);shot->tick=WORD_AT(0x007de318u);
    const uint32_t weapon=actor+0xa4u+shot->hand*0x130u;
    shot->weapon=(int32_t)WORD_AT(weapon+0x30u);shot->target_slot=UINT32_MAX;
    const uint32_t target=WORD_AT(actor+4u);
    for(unsigned i=0;i<32u;i++) if(target && guest_writable(pool+i*0xea0u,4u) && WORD_AT(pool+i*0xea0u)==target) {shot->target_slot=i;break;}
    memcpy(shot->transform,(const void *)((uintptr_t)weapon+g_xbox_mem_offset),48u);
    memcpy(shot->aim,(const void *)((uintptr_t)(actor+0xb88u)+g_xbox_mem_offset),12u);
    memcpy(shot->muzzle,(const void *)((uintptr_t)(actor+0xbb4u)+g_xbox_mem_offset),12u);
    original();
    if(g_eax && pc_network_bot_shot_valid(shot,32u)) {
        story_shots_sent[slot]=shot->serial;
        if(!story_shots_send(&s)) story_shots_pending[slot]=s;
    }
    return 1;
#else
    (void)original;return 0;
#endif
}
