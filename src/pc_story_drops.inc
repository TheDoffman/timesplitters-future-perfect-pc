/* Stock35B190 reconstructs drops from numeric inventory and transforms in a
   temporary actor. That actor has no Story AI resource at+bf0. Replaying the
   host's already-authorized weapon drop must use its wire inventory, rather
   than rerun Story's NPC/difficulty drop-budget decision on that empty record. */
void pc_story_network_drop_replay(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    if(story_npc_session() && WORD_AT(0x0076b140u)==2u && WORD_AT(0x0076b1a4u)==15u &&
       guest_writable(frame,16u) && WORD_AT(frame)==0x0035b51cu) {
        const uint32_t actor=WORD_AT(frame+8u),hand=WORD_AT(frame+12u);
        if(guest_writable(actor,0xea0u) && guest_writable(hand,0x130u) &&
           WORD_AT(actor+8u)<32u && WORD_AT(actor+0x94u)>0u && WORD_AT(actor+0x94u)<70u &&
           WORD_AT(hand+0x30u)<66u && !WORD_AT(actor+0xbf0u)) {
            /* First-mission NPC drops are authoritative item snapshots below.
               Stock Story throws can return before sending0x16; accepting its
               occasional late copy would duplicate the same pickup. */
            if(WORD_AT(0x0079094cu)==2u && WORD_AT(actor+8u)>=story_npc_humans()) {
                g_esp+=4u;return;
            }
            uint8_t *rule=(uint8_t *)((uintptr_t)0x007de455u+g_xbox_mem_offset);
            const uint8_t saved=*rule;
            *rule=0u; /* Original inventory-driven network-drop branch. */
            original();
            *rule=saved;
            return;
        }
    }
#endif
    original();
}

#ifdef _WIN32
/* Completed item state, not an actor's potentially different drop budget.
   Thirty original pickup records at7B0D20 have a D8-byte stride. */
#define STORY_DROP_MAGIC 0x31504453u
#define STORY_DROP_COUNT 30u
typedef struct StoryDropState {
    uint32_t magic,match,slot,serial,present,id,source,weapon;
    uint32_t ammo[2],quantity[2];float position[3],angles[3];
} StoryDropState;
_Static_assert(sizeof(StoryDropState)+4u<=256u,"Story pickup message bound");
typedef struct StoryDropReplica {uint32_t entity,local_id,id;} StoryDropReplica;
static StoryDropState story_drop_host[STORY_DROP_COUNT],story_drop_pending[STORY_DROP_COUNT];
static StoryDropReplica story_drop_replicas[STORY_DROP_COUNT];
static uint32_t story_drop_seen[STORY_DROP_COUNT],story_drop_recovery[STORY_DROP_COUNT];
static unsigned char story_drop_fresh[STORY_DROP_COUNT];
static uint32_t story_drop_tick;static int story_drop_ticked;
static unsigned story_drop_traces;
extern void sub_000C1950(void);
static int story_drop_session(void)
{return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u;}
static void story_drop_reset(void)
{
    memset(story_drop_host,0,sizeof story_drop_host);memset(story_drop_pending,0,sizeof story_drop_pending);
    memset(story_drop_replicas,0,sizeof story_drop_replicas);memset(story_drop_seen,0,sizeof story_drop_seen);
    memset(story_drop_fresh,0,sizeof story_drop_fresh);memset(story_drop_recovery,0,sizeof story_drop_recovery);
    story_drop_tick=0;story_drop_ticked=0;story_drop_traces=0;
}
static void story_drop_trace(const char *action,const StoryDropState *s,uint32_t entity)
{
    if(getenv("TSFP_STORY_TRACE") && story_drop_traces++<64u)
        fprintf(stderr,"[PC-STORY-DROP] %s slot=%u id=%u source=%u weapon=%u ammo=%u/%u present=%u entity=%08x\n",
            action,s->slot,s->id,s->source,s->weapon,s->ammo[0],s->quantity[0],s->present,entity);
}
static int story_drop_valid(const StoryDropState *s)
{
    if(s->magic!=STORY_DROP_MAGIC || !s->serial || s->slot>=STORY_DROP_COUNT || s->present>1u ||
       !s->id || s->id==UINT32_MAX || s->source<story_npc_humans() || s->source>=32u ||
       !s->weapon || s->weapon>=70u) return 0;
    for(unsigned i=0;i<2u;i++) if(s->ammo[i]>=44u || s->quantity[i]>32767u || (!s->ammo[i] && s->quantity[i])) return 0;
    for(unsigned i=0;i<3u;i++) if(!isfinite(s->position[i]) || fabsf(s->position[i])>1000000.f ||
        !isfinite(s->angles[i]) || fabsf(s->angles[i])>1000000.f) return 0;
    return 1;
}
static int story_drop_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || !guest_writable(payload,4u) || WORD_AT(payload)!=STORY_DROP_MAGIC) return 0;
    if(!story_drop_session() || WORD_AT(0x0076b140u)!=2u || !guest_writable(peer,4u) ||
       WORD_AT(peer)!=WORD_AT(0x00774040u) || length<sizeof(StoryDropState) || length>sizeof(StoryDropState)+4u ||
       !guest_writable(payload,sizeof(StoryDropState))) return 1;
    StoryDropState s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(!story_drop_valid(&s) || s.match!=story_npc_match() ||
       (story_drop_seen[s.slot] && (int32_t)(s.serial-story_drop_seen[s.slot])<=0) ||
       (story_drop_fresh[s.slot] && (int32_t)(s.serial-story_drop_pending[s.slot].serial)<=0)) return 1;
    story_drop_pending[s.slot]=s;story_drop_fresh[s.slot]=1;return 1;
}
static int story_drop_owned(const StoryDropReplica *r)
{
    return r->entity && guest_writable(r->entity,0x2e8u) && WORD_AT(r->entity+0x20u)==0x40u &&
        !(WORD_AT(r->entity+0x28u)&0x80000000u) && WORD_AT(r->entity+0x2cu)==r->local_id &&
        WORD_AT(r->entity+0x230u)==r->id;
}
static int story_drop_apply(const StoryDropState *s)
{
    StoryDropReplica *r=&story_drop_replicas[s->slot];
    if(!story_drop_owned(r)) memset(r,0,sizeof *r);
    if(r->entity && (!s->present || r->id!=s->id)) {
        story_drop_trace("retired",s,r->entity);
        guest_call(sub_000A9070,1,r->entity);memset(r,0,sizeof *r);
    }
    if(!s->present) return 1;
    const int created=!r->entity;
    if(!r->entity) {
        const uint32_t frame=g_esp;
        if(frame<1024u || !guest_writable(frame-1024u,1024u)) return 0;
        const uint32_t scratch=frame-128u,saved_id=WORD_AT(0x00541ea4u);
        memcpy((void *)((uintptr_t)scratch+g_xbox_mem_offset),s->position,12u);
        WORD_AT(0x00541ea4u)=s->id;g_esp=frame-512u;
        const uint32_t entity=guest_call(sub_000C1950,10,1u,2u,s->weapon,0u,scratch,0x004db9a8u,
            s->ammo[0],s->quantity[0],s->ammo[1],s->quantity[1]);
        g_esp=frame;WORD_AT(0x00541ea4u)=saved_id;
        if(!guest_writable(entity,0x2e8u) || WORD_AT(entity+0x20u)!=0x40u) return 0;
        r->entity=entity;r->local_id=WORD_AT(entity+0x2cu);r->id=s->id;
    }
    const uint32_t entity=r->entity,owner=WORD_AT(entity+0x7cu);
    if(!guest_writable(owner,0xd8u) || WORD_AT(owner)!=entity || WORD_AT(owner+0xcu)!=2u) return 0;
    WORD_AT(entity+0x230u)=s->id;
    WORD_AT(entity+0x28u)=(WORD_AT(entity+0x28u)&~0x40000000u)|0x63u;
    memcpy((void *)((uintptr_t)(entity+0x64u)+g_xbox_mem_offset),s->position,12u);
    memcpy((void *)((uintptr_t)(entity+0xc0u)+g_xbox_mem_offset),s->angles,4u);
    memcpy((void *)((uintptr_t)(entity+0x70u)+g_xbox_mem_offset),s->angles+1,4u);
    memcpy((void *)((uintptr_t)(entity+0xc8u)+g_xbox_mem_offset),s->angles+2,4u);
    memset((void *)((uintptr_t)(entity+0xb4u)+g_xbox_mem_offset),0,12u);
    /* Host snapshots own the throw/landing. Local item state stays idle, while
       original pickup requests use the preserved host runtime ID. */
    WORD_AT(owner+0xa0u)=1;WORD_AT(owner+0xc0u)=3;WORD_AT(owner+0xa4u)=s->source;
    for(unsigned i=0;i<2u;i++) {WORD_AT(owner+0x40u+i*8u)=s->ammo[i];WORD_AT(owner+0x44u+i*8u)=s->quantity[i];}
    pc_network_bots_entity_refresh(entity);
    if(created) story_drop_trace("created",s,entity);
    return 1;
}
static void story_drop_frame(void)
{
    if(!story_drop_session()) return;
    GuestRegisters saved;save_registers(&saved);
    if(WORD_AT(0x0076b140u)==2u) {
        for(unsigned i=0;i<STORY_DROP_COUNT;i++) if(story_drop_fresh[i]) {
            const StoryDropState *s=&story_drop_pending[i];
            if(s->match!=story_npc_match() || story_drop_apply(s)) {
                story_drop_seen[i]=s->serial;story_drop_fresh[i]=0;
            }
        }
        restore_registers(&saved);return;
    }
    if(WORD_AT(0x0076b140u)!=1u) {restore_registers(&saved);return;}
    const uint32_t tick=WORD_AT(0x007de318u),frame=g_esp,node=WORD_AT(0x00774028u);
    if((story_drop_ticked && (uint32_t)(tick-story_drop_tick)<6u) || frame<1024u ||
       !guest_writable(frame-1024u,1024u) || !guest_writable(node,0x35u)) {restore_registers(&saved);return;}
    story_drop_tick=tick;story_drop_ticked=1;
    for(unsigned i=0;i<STORY_DROP_COUNT;i++) {
        const uint32_t owner=0x007b0d20u+i*0xd8u,entity=WORD_AT(owner);
        StoryDropState s;memset(&s,0,sizeof s);s.magic=STORY_DROP_MAGIC;s.match=story_npc_match();s.slot=i;
        const uint32_t source=WORD_AT(owner+0xa4u),id=guest_writable(entity,0x2e8u)?WORD_AT(entity+0x230u):0u;
        if(guest_writable(entity,0x2e8u) && WORD_AT(entity+0x20u)==0x40u &&
           !(WORD_AT(entity+0x28u)&0x80000000u) && WORD_AT(entity+0x7cu)==owner &&
           WORD_AT(owner+0xcu)==2u && (WORD_AT(owner+0x10u)&1u) &&
           source>=story_npc_humans() && source<32u && id && id!=UINT32_MAX) {
            s.present=1;s.id=id;s.source=source;s.weapon=WORD_AT(owner+0x14u);
            for(unsigned j=0;j<2u;j++) {s.ammo[j]=WORD_AT(owner+0x40u+j*8u);s.quantity[j]=WORD_AT(owner+0x44u+j*8u);}
            memcpy(s.position,(const void *)((uintptr_t)(entity+0x64u)+g_xbox_mem_offset),12u);
            memcpy(s.angles,(const void *)((uintptr_t)(entity+0xc0u)+g_xbox_mem_offset),4u);
            memcpy(s.angles+1,(const void *)((uintptr_t)(entity+0x70u)+g_xbox_mem_offset),4u);
            memcpy(s.angles+2,(const void *)((uintptr_t)(entity+0xc8u)+g_xbox_mem_offset),4u);
        } else {
            if(!story_drop_host[i].id) continue;
            s=story_drop_host[i];s.present=0;
        }
        const StoryDropState *old=&story_drop_host[i];s.serial=old->serial;
        const int changed=memcmp(&s,old,sizeof s)!=0;
        const int recovery=!story_drop_recovery[i] || (uint32_t)(tick-story_drop_recovery[i])>=60u;
        if(!changed && !recovery) continue;
        if(++s.serial==0u) ++s.serial;
        if(!story_drop_valid(&s)) continue;
        const int reliable=recovery || s.id!=old->id || s.present!=old->present ||
            memcmp(s.ammo,old->ammo,sizeof s.ammo+sizeof s.quantity)!=0;
        const uint32_t buffer=frame-256u;WORD_AT(buffer)=0;
        memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&s,sizeof s);g_esp=frame-768u;
        guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof s+4u,(uint32_t)reliable);g_esp=frame;
        if(s.id!=old->id || s.present!=old->present) story_drop_trace("sent",&s,entity);
        story_drop_host[i]=s;if(reliable) story_drop_recovery[i]=tick?tick:1u;
    }
    restore_registers(&saved);
}
#endif
