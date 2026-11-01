/* Story speech belongs to the host script. Sound handles, subtitle data and
   lip-sync tracks are created locally through the original speech API. */
#ifdef _WIN32
extern void sub_00164C40(void),sub_001CB260(void);
typedef struct StoryDialogue {
    uint32_t magic,match,serial,slot,spawn,line,flags;
} StoryDialogue;
#define STORY_DIALOGUE_MAGIC 0x314C4453u
static StoryDialogue story_dialogue_pending[64];
static uint32_t story_dialogue_arrived[64];
static uint32_t story_dialogue_serial,story_dialogue_seen[256],story_dialogue_high;
static unsigned story_dialogue_read,story_dialogue_count;
static int story_dialogue_replay;
static void story_dialogue_reset(void)
{
    story_dialogue_serial=story_dialogue_high=0;story_dialogue_read=story_dialogue_count=0;
    memset(story_dialogue_seen,0,sizeof story_dialogue_seen);story_dialogue_replay=0;
}
static int story_dialogue_session(void)
{return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u;}
static int story_dialogue_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || !guest_writable(payload,4u) || WORD_AT(payload)!=STORY_DIALOGUE_MAGIC) return 0;
    if(length<sizeof(StoryDialogue) || length>sizeof(StoryDialogue)+4u ||
       !guest_writable(payload,sizeof(StoryDialogue)) || WORD_AT(0x0076b140u)!=2u ||
       !story_dialogue_session() || !guest_writable(peer,4u) || WORD_AT(peer)!=WORD_AT(0x00774040u)) return 1;
    StoryDialogue s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(s.match!=story_npc_match() || !s.serial || s.slot>=32u || !s.line || s.line>0x1ffffu ||
       (s.flags&~0x1ffffu) || (int32_t)(story_dialogue_high-s.serial)>=256 ||
       story_dialogue_seen[s.serial%256u]==s.serial || story_dialogue_count>=64u) return 1;
    story_dialogue_seen[s.serial%256u]=s.serial;
    if((int32_t)(s.serial-story_dialogue_high)>0) story_dialogue_high=s.serial;
    const unsigned index=(story_dialogue_read+story_dialogue_count++)%64u;
    story_dialogue_pending[index]=s;story_dialogue_arrived[index]=WORD_AT(0x007de318u);
    return 1;
}
static void story_dialogue_frame(void)
{
    if(!story_dialogue_session() || WORD_AT(0x0076b140u)!=2u || WORD_AT(0x007de30cu)!=7u) return;
    const uint32_t pool=WORD_AT(0x007356d8u);
    if(!pool || !guest_writable(pool,32u*0xea0u)) return;
    GuestRegisters saved;save_registers(&saved);
    while(story_dialogue_count) {
        const StoryDialogue s=story_dialogue_pending[story_dialogue_read];
        const uint32_t actor=pool+s.slot*0xea0u,entity=WORD_AT(actor);
        /* A reliable dialogue may overtake its unreliable creation snapshot.
           Wait for this generation; an obsolete line must never attach to a
           replacement actor. Newer host snapshots make it safe to discard. */
        const int ready=guest_writable(entity,0x25cu) && WORD_AT(entity+0x7cu)==actor && WORD_AT(entity+0x258u)==s.spawn;
        if(!ready && s.match==story_npc_match() &&
           (uint32_t)(WORD_AT(0x007de318u)-story_dialogue_arrived[story_dialogue_read])<300u) break;
        if(ready && s.match==story_npc_match()) {
            story_dialogue_replay=1;
            guest_call(sub_00164C40,4,actor,s.line,s.flags,0u);
            story_dialogue_replay=0;
        }
        story_dialogue_read=(story_dialogue_read+1u)%64u;--story_dialogue_count;
    }
    /* 15EAE0 is host AI and owns the ordinary lip-track update. Run only its
       independent cosmetic audio-clock child on current guest replicas. */
    for(unsigned slot=story_npc_humans();slot<32u;slot++) {
        const uint32_t actor=pool+slot*0xea0u;
        if(story_npc_replica_valid(WORD_AT(actor),0) && WORD_AT(actor+0xe90u))
            guest_call(sub_001CB260,1,actor);
    }
    restore_registers(&saved);
}
#endif

void pc_story_network_dialogue(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const int capture=!story_dialogue_replay && story_dialogue_session() &&
        WORD_AT(0x0076b140u)==1u && guest_writable(frame,20u);
    StoryDialogue s;memset(&s,0,sizeof s);
    uint32_t actor=0;
    if(capture) {
        actor=WORD_AT(frame+4u);const uint32_t pool=WORD_AT(0x007356d8u);
        if(pool && actor>=pool && (actor-pool)%0xea0u==0 && (actor-pool)/0xea0u<32u && guest_writable(actor,0xea0u)) {
            const uint32_t entity=WORD_AT(actor);
            if(guest_writable(entity,0x25cu) && WORD_AT(entity+0x7cu)==actor) {
                s.magic=STORY_DIALOGUE_MAGIC;s.match=story_npc_match();s.slot=(actor-pool)/0xea0u;
                s.spawn=WORD_AT(entity+0x258u);s.line=WORD_AT(frame+8u);s.flags=WORD_AT(frame+12u);
            }
        }
    }
#endif
    original();
#ifdef _WIN32
    if(!s.magic || !s.line || s.line>0x1ffffu || (s.flags&~0x1ffffu) ||
       (int32_t)WORD_AT(actor+0xe8cu)<0) return;
    const uint32_t node=WORD_AT(0x00774028u);
    if(frame<1024u || !guest_writable(frame-1024u,1024u) || !guest_writable(node,0x35u)) return;
    s.serial=++story_dialogue_serial;if(!s.serial) s.serial=++story_dialogue_serial;
    const uint32_t buffer=frame-128u;WORD_AT(buffer)=0;
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&s,sizeof s);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-768u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof s+4u,1u);
    restore_registers(&saved);
#endif
}
