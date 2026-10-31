/* Persistent Story props need peer-independent combat identities. Original
   runtime IDs advance as NPCs are constructed, so they diverge across peers. */
#ifdef _WIN32
typedef struct StoryWorldIdentity {uint32_t local_id,spawn,key;} StoryWorldIdentity;
static StoryWorldIdentity story_world_ids[16384];
static uint32_t story_world_pool,story_world_match;
typedef struct StoryWorldRemove {uint32_t magic,match,serial,key;} StoryWorldRemove;
#define STORY_WORLD_REMOVE_MAGIC 0x31525753u
static StoryWorldRemove story_world_pending[256];
static uint32_t story_world_seen[256],story_world_serial,story_world_high;
static unsigned story_world_read,story_world_count;
static int story_world_replay;
static int story_world_session(void)
{return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u;}
static void story_world_reset(void)
{
    memset(story_world_ids,0,sizeof story_world_ids);memset(story_world_seen,0,sizeof story_world_seen);
    story_world_pool=story_world_match=story_world_serial=story_world_high=0;
    story_world_read=story_world_count=0;story_world_replay=0;
}
/* Call only inside a freshly validated pool span, with no guest calls while
   scanning it. This avoids one VM-region query for every prop on every frame. */
static uint32_t story_world_key_in_pool(uint32_t entity,uint32_t pool,uint32_t count)
{
    if(entity<pool || (entity-pool)%0x2e8u || (entity-pool)/0x2e8u>=count) return 0;
    const uint32_t type=WORD_AT(entity+0x20u);
    /* 1BE1A9 assigns CA..CD to local first-person model viewports (CE for
       the nonlocal fallback), and 1BE1C9 sets their first-person flag. These
       local models can have the same unnamed/type/position key on every peer.
       A host weapon change must never publish them as persistent prop removal. */
    const uint32_t flags=WORD_AT(entity+0x28u),room=WORD_AT(entity+0x34u);
    if((flags&0x40000000u) && room>=0xcau && room<=0xceu) return 0;

    if((type!=1u && type!=2u && type!=4u && type!=16u) || (WORD_AT(entity+0x28u)&0x80000000u)) return 0;
    if(pool!=story_world_pool || story_world_match!=story_npc_match()) {
        memset(story_world_ids,0,sizeof story_world_ids);story_world_pool=pool;story_world_match=story_npc_match();
    }
    StoryWorldIdentity *identity=&story_world_ids[(entity-pool)/0x2e8u];
    const uint32_t local=WORD_AT(entity+0x2cu),spawn=WORD_AT(entity+0x258u);
    if(identity->key && identity->local_id==local && identity->spawn==spawn) return identity->key;
    uint32_t words[6]={type,WORD_AT(entity+0x94u)&65535u,WORD_AT(entity+0x60u),0,0,0};
    if(!words[2] || words[2]==0x3f3f3f3fu) {
        words[2]=0;
        for(unsigned i=0;i<3u;i++) {
            float value;memcpy(&value,(const void *)((uintptr_t)(entity+0x64u+i*4u)+g_xbox_mem_offset),4u);
            if(!isfinite(value) || fabsf(value)>1000000.f) return 0;
            words[3u+i]=(uint32_t)(int32_t)roundf(value*100.f);
        }
    }
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<sizeof words;i++) hash=(hash^((const uint8_t *)words)[i])*16777619u;
    identity->key=hash?hash:1u;identity->local_id=local;identity->spawn=spawn;
    return identity->key;
}
static uint32_t story_world_key(uint32_t entity)
{
    const uint32_t pool=WORD_AT(0x0072f5e8u),count=WORD_AT(0x007b4facu);
    if(!story_world_session() || !pool || count>16384u || !guest_writable(entity,0x2e8u)) return 0;
    return story_world_key_in_pool(entity,pool,count);
}
static int story_world_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || !guest_writable(payload,4u) || WORD_AT(payload)!=STORY_WORLD_REMOVE_MAGIC) return 0;
    if(!story_world_session() || WORD_AT(0x0076b140u)!=2u || !guest_writable(peer,4u) ||
       WORD_AT(peer)!=WORD_AT(0x00774040u) || length<sizeof(StoryWorldRemove) || length>sizeof(StoryWorldRemove)+4u ||
       !guest_writable(payload,sizeof(StoryWorldRemove))) return 1;
    StoryWorldRemove s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(!s.serial || !s.key || s.match!=story_npc_match() || story_world_count>=256u ||
       (int32_t)(story_world_high-s.serial)>=256 || story_world_seen[s.serial%256u]==s.serial) return 1;
    story_world_seen[s.serial%256u]=s.serial;if((int32_t)(s.serial-story_world_high)>0) story_world_high=s.serial;
    story_world_pending[(story_world_read+story_world_count++)%256u]=s;return 1;
}
#endif
uint32_t pc_story_network_world_key(uint32_t entity)
{
#ifdef _WIN32
    return story_world_key(entity);
#else
    (void)entity;return 0;
#endif
}
uint32_t pc_story_network_world_entity(uint32_t key)
{
#ifdef _WIN32
    if(!key || !story_world_session()) return 0;
    const uint32_t pool=WORD_AT(0x0072f5e8u),count=WORD_AT(0x007b4facu);uint32_t found=0;
    if(!pool || count>16384u || !guest_writable(pool,count*0x2e8u)) return 0;
    for(unsigned i=0;i<count;i++) {
        const uint32_t entity=pool+i*0x2e8u;
        if(story_world_key_in_pool(entity,pool,count)==key) {if(found) return 0;found=entity;}
    }
    return found; /* Ambiguous identities are never resolved by pool order. */
#else
    (void)key;return 0;
#endif
}
#ifdef _WIN32
static void story_world_frame(void)
{
    if(!story_world_session()) return;
    const uint32_t pool=WORD_AT(0x0072f5e8u),count=WORD_AT(0x007b4facu);
    if(pool && count<=16384u && guest_writable(pool,count*0x2e8u))
        for(unsigned i=0;i<count;i++) (void)story_world_key_in_pool(pool+i*0x2e8u,pool,count);
    if(WORD_AT(0x0076b140u)!=2u) return;
    GuestRegisters saved;save_registers(&saved);
    while(story_world_count) {
        const StoryWorldRemove s=story_world_pending[story_world_read];
        story_world_read=(story_world_read+1u)%256u;--story_world_count;
        if(s.match!=story_npc_match()) continue;
        const uint32_t entity=pc_story_network_world_entity(s.key);
        if(entity) {story_world_replay=1;guest_call(sub_000A9070,1,entity);story_world_replay=0;}
    }
    restore_registers(&saved);
}
#endif
void pc_story_network_world_remove(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    if(story_world_session() && getenv("TSFP_STORY_TRACE") && guest_writable(frame,8u)) {
        const uint32_t entity=WORD_AT(frame+4u);
        if(guest_writable(entity,0x154u) && (WORD_AT(entity+0x28u)&0x40000000u) &&
           WORD_AT(entity+0x34u)>=0xcau && WORD_AT(entity+0x34u)<=0xceu) {
            static unsigned observations;
            if(observations++<16u) fprintf(stderr,"[PC-STORY-WORLD] local first-person cleanup role=%u entity=%08X replay=%d caller=%08X\n",
                WORD_AT(0x76b140u),entity,story_world_replay,WORD_AT(frame));
        }
    }

    const uint32_t key=!story_world_replay && WORD_AT(0x0076b140u)==1u && WORD_AT(0x007de30cu)==7u &&
        guest_writable(frame,8u)?story_world_key(WORD_AT(frame+4u)):0u;
#endif
    original();
#ifdef _WIN32
    if(!key) return;
    const uint32_t node=WORD_AT(0x00774028u);
    if(frame<1024u || !guest_writable(frame-1024u,1024u) || !guest_writable(node,0x35u)) return;
    StoryWorldRemove s={STORY_WORLD_REMOVE_MAGIC,story_npc_match(),++story_world_serial,key};
    if(!s.serial) s.serial=++story_world_serial;
    const uint32_t buffer=frame-128u;WORD_AT(buffer)=0;
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&s,sizeof s);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-768u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof s+4u,1u);restore_registers(&saved);
#endif
}
