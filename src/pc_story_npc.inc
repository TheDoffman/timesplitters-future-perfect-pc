/* Story NPC creation uses the stock reliable spawn channel, with explicit POD
 * creation/state data. Stock Arcade spawn packets assume an existing body. */
#ifdef _WIN32
#include <math.h>
#include <stddef.h>
#include "pc_character_pose.h"
void pc_character_pose_present(uint32_t entity,const PcCharacterPose *pose);
extern void sub_000DAC30(void),sub_000D7550(void),sub_000A9070(void);
extern void sub_0015BCA0(void),sub_0015BCC0(void),sub_002402B0(void),sub_00353A90(void);
void pc_network_bots_entity_refresh(uint32_t entity);
typedef struct StoryNpcConstructor {
    int valid;int32_t first,second,variant;
} StoryNpcConstructor;
static StoryNpcConstructor story_npc_constructors[32];
static unsigned char story_npc_kinds[256];
typedef struct StoryNpcSpawn {
    uint32_t magic,match,slot,serial;
    int32_t first,second,variant;
    uint32_t present,in_roster,spawn,network_id,script_id,body_flags;
    uint32_t flags[3],life[4],team,weapon;
    float position[3],angles[3],heading,health,armor,max_health;
    float bounds_center[3],bounds_radius[3];
    uint32_t pose_present;PcCharacterPose pose;
} StoryNpcSpawn;
_Static_assert(sizeof(StoryNpcSpawn)==372,"Story spawn wire layout");
#define STORY_NPC_MAGIC 0x32504E53u
#define STORY_NPC_POSE_MAGIC 0x32505053u
#define STORY_NPC_BASE_BYTES 156u
/* Original353990 copies into a256-byte guest stack buffer. Never enlarge it. */
typedef struct StoryNpcBaseMessage {uint8_t state[STORY_NPC_BASE_BYTES];uint32_t checksum;} StoryNpcBaseMessage;
typedef struct StoryNpcPoseMessage {uint32_t magic,match,slot,serial,checksum;PcCharacterPose pose;} StoryNpcPoseMessage;
_Static_assert(offsetof(StoryNpcSpawn,pose)==STORY_NPC_BASE_BYTES,"Physical snapshot prefix");
_Static_assert(sizeof(StoryNpcBaseMessage)+4u<=256u,"Stock physical message limit");
_Static_assert(sizeof(StoryNpcPoseMessage)+4u<=256u,"Stock pose message limit");
static uint32_t story_npc_checksum(const StoryNpcSpawn *s)
{
    const uint8_t *bytes=(const uint8_t *)s;uint32_t hash=2166136261u;
    for(unsigned i=0;i<sizeof *s;i++) hash=(hash^bytes[i])*16777619u;
    return hash;
}
static StoryNpcSpawn story_npc_pending[32];
static unsigned char story_npc_fresh[32],story_npc_parts[32];
static uint32_t story_npc_checksums[32];
static uint32_t story_npc_serial[32],story_npc_seen_match[32],story_npc_seen_serial[32];
typedef struct StoryNpcReplica {
    uint32_t entity,spawn,match,pose_present;
    float center[3],radius[3];
} StoryNpcReplica;
static StoryNpcReplica story_npc_replicas[32];
static uint32_t story_npc_publish_tick;
static int story_npc_published;
static void story_npc_reset(void)
{
    story_npc_published=0;
    memset(story_npc_replicas,0,sizeof story_npc_replicas);
    memset(story_npc_constructors,0,sizeof story_npc_constructors);
    memset(story_npc_kinds,0,sizeof story_npc_kinds);
    memset(story_npc_seen_match,0,sizeof story_npc_seen_match);
    memset(story_npc_seen_serial,0,sizeof story_npc_seen_serial);
    if(story_pending_stage==0u) {
        memset(story_npc_fresh,0,sizeof story_npc_fresh);
        memset(story_npc_parts,0,sizeof story_npc_parts);
    }
}

static uint32_t story_npc_match(void)
{
    const uint8_t *c=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
    return (c[0x99]|((uint32_t)c[0x54]<<8)|((uint32_t)c[0x55]<<16)|((uint32_t)c[0x56]<<24)) ^
        (story_reload_epoch&255u) ^ ((story_reload_epoch&0xff00u)<<16);
}
static unsigned story_npc_humans(void)
{ return *(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset); }
static int story_npc_session(void)
{
    const unsigned humans=story_npc_humans();
    return story_network_session() && WORD_AT(0x0076B108u) &&
        WORD_AT(0x0079094Cu)<100u && WORD_AT(0x007DE458u)==0x00080008u &&
        humans && humans<=4u;
}
static int story_npc_valid(const StoryNpcSpawn *s)
{
    if(s->magic!=STORY_NPC_MAGIC || !s->serial || ((s->match>>16)&255u)!=10u ||
       ((s->match>>8)&255u)>=100u || !s->slot || s->slot>=32u ||
       s->first<0 || s->first>=256 || s->second<0 || s->second>=256 ||
       s->variant< -1 || s->variant>=256 || s->pose_present>1u || s->present>1u || s->in_roster>1u || s->team>=32u ||
       (s->weapon>=70u && s->weapon!=UINT32_MAX)) return 0;
    for(unsigned i=0;i<3u;i++)
        if(!isfinite(s->position[i]) || fabsf(s->position[i])>1000000.f ||
           !isfinite(s->angles[i]) || fabsf(s->angles[i])>100000.f) return 0;
    if(s->pose_present && !pc_character_pose_valid(&s->pose)) return 0;
    for(unsigned i=0;i<3u;i++)
        if(!isfinite(s->bounds_center[i]) || fabsf(s->bounds_center[i])>10000.f ||
           !isfinite(s->bounds_radius[i]) || s->bounds_radius[i]<0.f || s->bounds_radius[i]>10000.f) return 0;
    return isfinite(s->heading) && fabsf(s->heading)<=100000.f &&
        isfinite(s->health) && s->health>=-1000.f && s->health<=1000.f &&
        isfinite(s->armor) && s->armor>=0.f && s->armor<=1000.f &&
        isfinite(s->max_health) && s->max_health>=0.f && s->max_health<=1000.f;
}
static int story_npc_listed(uint32_t actor)
{
    unsigned count=WORD_AT(0x0074C37Cu);
    if(count>32u) return 0;
    for(unsigned i=0;i<count;i++) if(WORD_AT(0x007A3580u+i*4u)==actor) return 1;
    return 0;
}
#endif

void pc_story_network_npc_constructor(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const int capture=story_npc_session() && guest_writable(frame,44u);
    const unsigned slot=capture?WORD_AT(frame+12u):32u;
    StoryNpcConstructor c={0,0,0,0};
    if(capture && slot>=story_npc_humans() && slot<32u) {
        c.first=(int32_t)WORD_AT(frame+4u);c.second=(int32_t)WORD_AT(frame+8u);
        c.variant=(int32_t)WORD_AT(frame+40u);
        c.valid=c.first>=0 && c.first<256 && c.second>=0 && c.second<256 &&
            c.variant>=-1 && c.variant<256;
    }
#endif
    original();
#ifdef _WIN32
    if(capture && slot>=story_npc_humans() && slot<32u) {
        story_npc_constructors[slot]=c;
        if(c.valid) story_npc_kinds[c.first]=story_npc_kinds[c.second]=1;
        if(!c.valid && getenv("TSFP_STORY_TRACE")) fprintf(stderr,
            "[PC-STORY-NPC-WIRE] invalid constructor slot=%u first=%d second=%d variant=%d\n",
            slot,c.first,c.second,c.variant);
    }
#endif
}

void pc_story_network_npc_send(void (*original)(void))
{
#ifdef _WIN32
    /* Original35CF80 has already performed its RNG/authority work. The outer
       spawn wrapper sends the completed body state instead of an empty slot. */
    if(story_npc_session() && WORD_AT(0x0076B140u)==1u &&
       guest_writable(g_esp,16u) && WORD_AT(g_esp)==0x0035CFEDu &&
       WORD_AT(g_esp+4u)==0x1eu && g_ebx==11u) {
        const uint32_t packet=WORD_AT(g_esp+12u);
        if(guest_writable(packet,11u)) {
            const unsigned slot=*(const uint8_t *)((uintptr_t)(packet+4u)+g_xbox_mem_offset);
            if(slot>=story_npc_humans() && slot<32u) {g_eax=1u;g_esp+=4u;return;}
        }
    }
#endif
    original();
}

#ifdef _WIN32
static void story_npc_publish(uint32_t actor,uint32_t frame)
{
    if(frame<2048u || !guest_writable(frame-2048u,2048u) ||
       !guest_writable(actor,0xea0u)) return;
    const unsigned slot=WORD_AT(actor+8u);
    const uint32_t pool=WORD_AT(0x007356D8u);
    if(slot<story_npc_humans() || slot>=32u || !pool || actor!=pool+slot*0xea0u ||
       !story_npc_constructors[slot].valid) return;
    StoryNpcSpawn s;memset(&s,0,sizeof s);
    s.magic=STORY_NPC_MAGIC;s.match=story_npc_match();s.slot=slot;
    s.serial=++story_npc_serial[slot];if(!s.serial) s.serial=++story_npc_serial[slot];
    s.first=story_npc_constructors[slot].first;s.second=story_npc_constructors[slot].second;
    s.variant=story_npc_constructors[slot].variant;s.weapon=WORD_AT(actor+0x94u);
    s.team=WORD_AT(actor+0x84u);
    s.in_roster=(uint32_t)story_npc_listed(actor);
    memcpy(s.flags,(const void *)((uintptr_t)(actor+0x24u)+g_xbox_mem_offset),sizeof s.flags);
    memcpy(s.life,(const void *)((uintptr_t)(actor+0x10u)+g_xbox_mem_offset),sizeof s.life);
    const uint32_t entity=WORD_AT(actor);
    if(entity && guest_writable(entity,0x25cu)) {
        s.present=1;s.spawn=WORD_AT(entity+0x258u);s.network_id=WORD_AT(entity+0x230u);
        s.script_id=WORD_AT(entity+0x60u);s.body_flags=WORD_AT(entity+0x28u);
        memcpy(s.position,(const void *)((uintptr_t)(entity+0x64u)+g_xbox_mem_offset),12u);
        memcpy(s.angles,(const void *)((uintptr_t)(entity+0xc0u)+g_xbox_mem_offset),12u);
        memcpy(&s.heading,(const void *)((uintptr_t)(entity+0x70u)+g_xbox_mem_offset),4u);
        memcpy(&s.health,(const void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),4u);
        memcpy(&s.armor,(const void *)((uintptr_t)(entity+0x1d0u)+g_xbox_mem_offset),4u);
        memcpy(&s.max_health,(const void *)((uintptr_t)(entity+0x1d8u)+g_xbox_mem_offset),4u);
        memcpy(s.bounds_center,(const void *)((uintptr_t)(entity+0x1a8u)+g_xbox_mem_offset),12u);
        memcpy(s.bounds_radius,(const void *)((uintptr_t)(entity+0x1c0u)+g_xbox_mem_offset),12u);
        const uint32_t model=WORD_AT(entity+4u);
        if(guest_writable(model,0x200u)) {
            pc_character_pose_read(&s.pose,(const void *)((uintptr_t)model+g_xbox_mem_offset));
            s.pose_present=(uint32_t)pc_character_pose_valid(&s.pose);
        }
    }
    if(!s.pose_present) memset(&s.pose,0,sizeof s.pose);
    if(!story_npc_valid(&s)) {
        if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,
            "[PC-STORY-NPC-WIRE] invalid snapshot slot=%u kind=%d variant=%d team=%u weapon=%u health=%g max=%g\n",
            slot,s.first,s.variant,s.team,s.weapon,s.health,s.max_health);
        return;
    }
    const uint32_t node=WORD_AT(0x00774028u),buffer=frame-768u;
    if(!guest_writable(node,0x35u)) return;
    const uint32_t checksum=story_npc_checksum(&s);
    StoryNpcBaseMessage base;memcpy(base.state,&s,STORY_NPC_BASE_BYTES);base.checksum=checksum;
    memset((void *)((uintptr_t)buffer+g_xbox_mem_offset),0,sizeof base+4u);
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&base,sizeof base);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-1280u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof base+4u,1u);
    if(s.pose_present) {
        const StoryNpcPoseMessage pose={STORY_NPC_POSE_MAGIC,s.match,s.slot,s.serial,checksum,s.pose};
        memset((void *)((uintptr_t)buffer+g_xbox_mem_offset),0,sizeof pose+4u);
        memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&pose,sizeof pose);
        guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof pose+4u,1u);
    }
    restore_registers(&saved);
    if(getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<48u) fprintf(stderr,
            "[PC-STORY-NPC-WIRE] sent slot=%u kind=%d present=%u roster=%u id=%u\n",
            slot,s.first,s.present,s.in_roster,s.network_id);
    }
}
#endif

void pc_story_network_npc_spawn(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const uint32_t target=guest_writable(frame,8u)?WORD_AT(frame+4u):0u;
    const uint32_t actor=target && guest_writable(target,0x80u)?WORD_AT(target+0x7cu):0u;
    const int host=story_npc_session() && WORD_AT(0x0076B140u)==1u &&
        WORD_AT(0x0076B1A4u)==15u;
#endif
    pc_story_network_npc_trace(original,0x0015F8C0u);
#ifdef _WIN32
    if(host) story_npc_publish(actor,frame);
#endif
}

#ifdef _WIN32
static int story_npc_apply(const StoryNpcSpawn *s)
{
    const uint32_t pool=WORD_AT(0x007356D8u),actor=pool+s->slot*0xea0u;
    if(!story_npc_session() || WORD_AT(0x0076B140u)!=2u || WORD_AT(0x0076B1A4u)!=15u ||
       s->match!=story_npc_match() || s->slot<story_npc_humans() ||
       !pool || actor<pool || !guest_writable(actor,0xea0u)) return 0;
    if(story_npc_seen_match[s->slot]==s->match &&
       (int32_t)(s->serial-story_npc_seen_serial[s->slot])<=0) return 1;
    if(!story_npc_kinds[s->first] || !story_npc_kinds[s->second]) return 0;
    GuestRegisters saved;save_registers(&saved);
    uint32_t entity=WORD_AT(actor);
    if(entity && (!guest_writable(entity,0x25cu) || WORD_AT(entity+0x7cu)!=actor)) {
        restore_registers(&saved);return 0;
    }
    if(entity && (!s->present || *(int16_t *)((uintptr_t)(actor+0x48u)+g_xbox_mem_offset)!=s->first ||
       *(int16_t *)((uintptr_t)(actor+0x4au)+g_xbox_mem_offset)!=s->second)) {
        if(story_npc_listed(actor)) guest_call(sub_0015BCC0,1,entity);
        guest_call(sub_000A9070,1,entity);WORD_AT(actor)=0;entity=0;
    }
    if(s->present && !entity) {
        entity=guest_call(sub_000DAC30,10,(uint32_t)s->first,(uint32_t)s->second,s->slot,
            0u,0u,0u,0u,UINT32_MAX,1u,(uint32_t)s->variant);
        if(entity && guest_writable(entity,0x25cu) && WORD_AT(actor)==entity) {
            guest_call(sub_002402B0,2,s->script_id,entity);
        } else entity=0;
    }
    if(s->present && entity && guest_writable(entity,0x25cu) && WORD_AT(entity+0x7cu)==actor) {
        WORD_AT(entity+0x230u)=s->network_id;WORD_AT(entity+0x258u)=s->spawn;
        WORD_AT(entity+0x28u)=(WORD_AT(entity+0x28u)&~0x71u)|(s->body_flags&0x71u);
        const uint32_t local_visible=WORD_AT(actor+0x2cu)&0x10u;
        memcpy((void *)((uintptr_t)(actor+0x24u)+g_xbox_mem_offset),s->flags,sizeof s->flags);
        WORD_AT(actor+0x2cu)=(WORD_AT(actor+0x2cu)&~0x10u)|local_visible;
        memcpy((void *)((uintptr_t)(actor+0x10u)+g_xbox_mem_offset),s->life,sizeof s->life);
        WORD_AT(actor+0x94u)=s->weapon;
        guest_call(sub_000D7550,2,actor,s->team);
        memcpy((void *)((uintptr_t)(entity+0x64u)+g_xbox_mem_offset),s->position,12u);
        memcpy((void *)((uintptr_t)(actor+0x30u)+g_xbox_mem_offset),s->position,12u);
        memcpy((void *)((uintptr_t)(entity+0xc0u)+g_xbox_mem_offset),s->angles,12u);
        memcpy((void *)((uintptr_t)(entity+0x70u)+g_xbox_mem_offset),&s->heading,4u);
        memcpy((void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),&s->health,4u);
        memcpy((void *)((uintptr_t)(entity+0x1d0u)+g_xbox_mem_offset),&s->armor,4u);
        memcpy((void *)((uintptr_t)(entity+0x1d8u)+g_xbox_mem_offset),&s->max_health,4u);
        const int listed=story_npc_listed(actor);
        if(s->in_roster && !listed && WORD_AT(0x0074C37Cu)<32u) guest_call(sub_0015BCA0,1,entity);
        if(!s->in_roster && listed) guest_call(sub_0015BCC0,1,entity);
        if(s->pose_present) pc_character_pose_present(entity,&s->pose);
        pc_network_bots_entity_refresh(entity);
        memcpy((void *)((uintptr_t)(entity+0x1a8u)+g_xbox_mem_offset),s->bounds_center,12u);
        memcpy((void *)((uintptr_t)(entity+0x1c0u)+g_xbox_mem_offset),s->bounds_radius,12u);
    }

    restore_registers(&saved);
    if(s->present && !entity) return 0;
    StoryNpcReplica *replica=&story_npc_replicas[s->slot];
    replica->entity=s->present?entity:0u;replica->spawn=s->spawn;
    replica->match=s->match;replica->pose_present=s->pose_present;
    memcpy(replica->center,s->bounds_center,12u);memcpy(replica->radius,s->bounds_radius,12u);
    story_npc_seen_match[s->slot]=s->match;story_npc_seen_serial[s->slot]=s->serial;
    if(getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<48u) fprintf(stderr,
            "[PC-STORY-NPC-WIRE] applied slot=%u serial=%u kind=%d present=%u roster=%u entity=%08X\n",
            s->slot,s->serial,s->first,s->present,s->in_roster,entity);
    }
    return 1;
}
static void story_npc_pending_frame(void)
{
    if(!story_npc_session() || WORD_AT(0x0076B1A4u)!=15u) return;
    if(WORD_AT(0x0076B140u)==1u) {
        /* A host may finish its first spawn before every guest has finished
           scene setup, and scripts change roster membership after15F8C0 returns.
           Publish current state again, including deletion, on simulation ticks.
           This also keeps moving bodies authoritative without guest Story AI. */
        const uint32_t tick=WORD_AT(0x007DE318u),pool=WORD_AT(0x007356D8u);
        if(story_npc_published && (uint32_t)(tick-story_npc_publish_tick)<6u) return;
        story_npc_published=1;story_npc_publish_tick=tick;
        if(pool) for(unsigned slot=story_npc_humans();slot<32u;slot++)
            if(story_npc_constructors[slot].valid)
                story_npc_publish(pool+slot*0xea0u,g_esp);
        return;
    }
    if(WORD_AT(0x0076B140u)!=2u) return;
    for(unsigned slot=0;slot<32u;slot++) if(story_npc_fresh[slot]) {
        if(story_npc_pending[slot].match!=story_npc_match() || slot<story_npc_humans() ||
           story_npc_apply(&story_npc_pending[slot]))
            story_npc_fresh[slot]=0;
    }
}
#endif
#ifdef _WIN32
static void story_npc_receive_part(uint32_t payload,uint32_t length)
{
    if(!guest_writable(payload,4u)) return;
    StoryNpcSpawn physical;memset(&physical,0,sizeof physical);
    StoryNpcPoseMessage pose;memset(&pose,0,sizeof pose);
    uint32_t match,slot,serial,checksum,part;
    if(WORD_AT(payload)==STORY_NPC_MAGIC && length>=sizeof(StoryNpcBaseMessage) &&
       length<=sizeof(StoryNpcBaseMessage)+4u && guest_writable(payload,sizeof(StoryNpcBaseMessage))) {
        StoryNpcBaseMessage message;
        memcpy(&message,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof message);
        memcpy(&physical,message.state,STORY_NPC_BASE_BYTES);
        const uint32_t posed=physical.pose_present;physical.pose_present=0;
        if(posed>1u || !story_npc_valid(&physical)) return;
        physical.pose_present=posed;match=physical.match;slot=physical.slot;
        serial=physical.serial;checksum=message.checksum;part=1u;
    } else if(WORD_AT(payload)==STORY_NPC_POSE_MAGIC && length>=sizeof pose &&
       length<=sizeof pose+4u && guest_writable(payload,sizeof pose)) {
        memcpy(&pose,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof pose);
        if(!pose.serial || !pose.slot || pose.slot>=32u || ((pose.match>>16)&255u)!=10u ||
           ((pose.match>>8)&255u)>=100u || !pc_character_pose_valid(&pose.pose)) return;
        match=pose.match;slot=pose.slot;serial=pose.serial;checksum=pose.checksum;part=2u;
    } else return;
    if(story_npc_seen_match[slot]==match && (int32_t)(serial-story_npc_seen_serial[slot])<=0) return;
    StoryNpcSpawn *s=&story_npc_pending[slot];
    if(story_npc_parts[slot] && s->match==match && (int32_t)(serial-s->serial)<0) return;
    if(!story_npc_parts[slot] || s->match!=match || s->serial!=serial || story_npc_checksums[slot]!=checksum) {
        memset(s,0,sizeof *s);s->magic=STORY_NPC_MAGIC;s->match=match;s->slot=slot;s->serial=serial;
        story_npc_parts[slot]=0;story_npc_fresh[slot]=0;story_npc_checksums[slot]=checksum;
    }
    if(part==1u) memcpy(s,&physical,STORY_NPC_BASE_BYTES);
    else s->pose=pose.pose;
    story_npc_parts[slot]|=(unsigned char)part;
    if((story_npc_parts[slot]&1u) && (!s->pose_present || (story_npc_parts[slot]&2u)) &&
       story_npc_checksum(s)==checksum && story_npc_valid(s)) {
        if(story_npc_apply(s)) {story_npc_fresh[slot]=0;story_npc_parts[slot]=0;}
        else story_npc_fresh[slot]=1;
    }
}
#endif
void pc_story_network_npc_receive(void (*original)(void))
{
#ifdef _WIN32
    if(story_network_session() && WORD_AT(0x0076B108u) && guest_writable(g_esp,20u)) {
        const uint32_t frame=g_esp,peer=WORD_AT(frame+4u),payload=WORD_AT(frame+8u);
        const uint32_t type=WORD_AT(frame+12u),length=WORD_AT(frame+16u);
        if(story_restart_receive(peer,payload,type,length)) {g_eax=0;g_esp=frame+4u;return;}
        if(story_loadout_receive(peer,payload,type,length)) {g_eax=0;g_esp=frame+4u;return;}
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned observations;
            if(observations++<16u) fprintf(stderr,
                "[PC-STORY-NPC-WIRE] receive type=%u bytes=%u sender=%u host=%u phase=%u\n",
                type,length,guest_writable(peer,4u)?WORD_AT(peer):UINT32_MAX,
                WORD_AT(0x00774040u),WORD_AT(0x0076B1A4u));
        }
        if(WORD_AT(0x0076B140u)==2u && type==0x1eu && guest_writable(peer,4u) &&
           WORD_AT(peer)==WORD_AT(0x00774040u)) story_npc_receive_part(payload,length);
        g_eax=0;g_esp=frame+4u;return;
    }
#endif
    original();
}

/* Observe actual human-to-NPC hit acceptance without changing damage policy.
 * Host/guest replay and health before/after distinguish a real accepted request
 * from a muzzle flash or an unrelated AI fight. Disabled paths are untouched. */
int pc_story_network_hit_trace(void (*original)(void))
{
#ifdef _WIN32
    if(!getenv("TSFP_STORY_TRACE") || !story_npc_session() ||
       WORD_AT(0x0076B1A4u)!=15u || !guest_writable(g_esp,8u)) return 0;
    const uint32_t context=WORD_AT(g_esp+4u),pool=WORD_AT(0x007356D8u);
    if(!pool || !guest_writable(context,0x58u)) return 0;
    const uint32_t victim=WORD_AT(context);
    if(!guest_writable(victim,0x25cu) || WORD_AT(victim+0x150u)) return 0;
    const uint32_t actor=WORD_AT(victim+0x7cu);
    if(!guest_writable(actor,0xea0u)) return 0;
    const unsigned slot=WORD_AT(actor+8u);
    if(slot<story_npc_humans() || slot>=32u || actor!=pool+slot*0xea0u || WORD_AT(actor)!=victim) return 0;
    unsigned source_slot=UINT32_MAX;
    for(unsigned offset=4u;offset<=8u;offset+=4u) {
        const uint32_t source=WORD_AT(context+offset);
        if(!guest_writable(source,0x25cu)) continue;
        const uint32_t player=WORD_AT(source+0x150u);
        if(player && guest_writable(player,0x1584u) && WORD_AT(player)<story_npc_humans() &&
           player==WORD_AT(0x007B0C48u)+WORD_AT(player)*0x1584u && WORD_AT(player+0x14u)==source) {
            source_slot=WORD_AT(player);break;
        }
    }
    if(source_slot==UINT32_MAX) return 0;
    static unsigned observations;
    if(observations>=128u) return 0;
    ++observations;
    float before,after,amount;
    memcpy(&before,(const void *)((uintptr_t)(victim+0x1ccu)+g_xbox_mem_offset),4u);
    memcpy(&amount,(const void *)((uintptr_t)(context+0x20u)+g_xbox_mem_offset),4u);
    const unsigned role=WORD_AT(0x0076B140u),incoming=WORD_AT(0x0076B170u),id=WORD_AT(victim+0x230u);
    original();
    memcpy(&after,(const void *)((uintptr_t)(victim+0x1ccu)+g_xbox_mem_offset),4u);
    fprintf(stderr,"[PC-STORY-HIT] role=%u incoming=%u target=%u slot=%u source=%u damage=%g hp=%g->%g result=%u\n",
        role,incoming,id,slot,source_slot,amount,before,after,g_eax);
    return 1;
#else
    (void)original;return 0;
#endif
}

/* Freeze only replicas with a valid pose in this match and body generation.
 * Human animation, Local Story and ordinary multiplayer keep their own paths. */
#ifdef _WIN32
static int story_npc_replica_valid(uint32_t entity,int need_pose)
{
    if(!story_npc_session() || WORD_AT(0x0076B140u)!=2u || WORD_AT(0x0076B1A4u)!=15u ||
       !guest_writable(entity,0x25cu) || WORD_AT(entity+0x150u)) return 0;
    const uint32_t actor=WORD_AT(entity+0x7cu),pool=WORD_AT(0x007356D8u);
    if(!pool || !guest_writable(actor,0xea0u)) return 0;
    const unsigned slot=WORD_AT(actor+8u);
    if(slot<story_npc_humans() || slot>=32u || actor!=pool+slot*0xea0u || WORD_AT(actor)!=entity) return 0;
    const StoryNpcReplica *r=&story_npc_replicas[slot];
    return (!need_pose || r->pose_present) && r->entity==entity && r->spawn==WORD_AT(entity+0x258u) && r->match==story_npc_match();
}
#endif
int pc_story_network_replica(uint32_t entity)
{
#ifdef _WIN32
    return story_npc_replica_valid(entity,1);
#else
    (void)entity;return 0;
#endif
}
/* Replicas never run a second damage/death/script simulation. Only a locally
 * owned human may request a hit; the host applies it and republishes state. */
int pc_story_network_damage(void)
{
#ifdef _WIN32
    if(!guest_writable(g_esp,8u)) return 0;
    const uint32_t context=WORD_AT(g_esp+4u);
    if(!guest_writable(context,0x58u) || !story_npc_replica_valid(WORD_AT(context),0)) return 0;
    if(!WORD_AT(0x0076B170u)) {
        const uint32_t pool=WORD_AT(0x007B0C48u);
        for(unsigned offset=4u;offset<=8u;offset+=4u) {
            const uint32_t source=WORD_AT(context+offset);
            if(!guest_writable(source,0x154u)) continue;
            const uint32_t player=WORD_AT(source+0x150u);
            if(!pool || !guest_writable(player,0x1584u)) continue;
            const unsigned slot=WORD_AT(player);
            if(slot>=story_npc_humans() || player!=pool+slot*0x1584u ||
               WORD_AT(player+0x14u)!=source || (int32_t)WORD_AT(player+4u)<0) continue;
            GuestRegisters saved;save_registers(&saved);
            pc_network_bots_request_hit(context);
            restore_registers(&saved);
            break;
        }
    }
    return 1;
#else
    return 0;
#endif
}
int pc_story_network_replica_bounds(uint32_t entity)
{
#ifdef _WIN32
    if(pc_story_network_replica(entity)) {
        const unsigned slot=WORD_AT(WORD_AT(entity+0x7cu)+8u);
        const StoryNpcReplica *r=&story_npc_replicas[slot];
        memcpy((void *)((uintptr_t)(entity+0x1a8u)+g_xbox_mem_offset),r->center,12u);
        memcpy((void *)((uintptr_t)(entity+0x1c0u)+g_xbox_mem_offset),r->radius,12u);
        return 1;
    }
#else
    (void)entity;
#endif
    return 0;
}
