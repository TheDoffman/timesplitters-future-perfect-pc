/* Checkpoints stay local engine snapshots. Only the authored checkpoint index
   crosses the reliable channel; each peer saves/restores its own pointers. */
#ifdef _WIN32
extern void sub_0025EF80(void),sub_0025EAD0(void),sub_0025EC70(void);
typedef struct StoryCheckpoint {uint32_t magic,match,serial,index;} StoryCheckpoint;
#define STORY_CHECKPOINT_MAGIC 0x31504353u
static StoryCheckpoint story_checkpoint_host,story_checkpoint_pending;
static uint32_t story_checkpoint_serial,story_checkpoint_tick;
static int story_checkpoint_replay,story_checkpoint_sent;
static uint32_t story_checkpoint_restore_pending;
static int story_checkpoint_scope(void)
{ return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u; }
static int story_checkpoint_players(void)
{
    const unsigned count=story_npc_humans();
    const uint32_t contexts=WORD_AT(0x007b0c48u),bookmarks=WORD_AT(0x0075f5d0u);
    if(!contexts || !bookmarks || count!=WORD_AT(0x00790950u) ||
       !guest_writable(contexts,count*0x1584u) || !guest_writable(bookmarks,count*0x238u)) return 0;
    for(unsigned i=0;i<count;i++) {
        const uint32_t context=contexts+i*0x1584u,entity=WORD_AT(context+0x14u);
        if(WORD_AT(context)!=i || !entity || !guest_writable(entity,0x154u) ||
           WORD_AT(entity+0x150u)!=context || !guest_writable(WORD_AT(entity+0x7cu),0xea0u)) return 0;
    }
    return 1;
}
static void story_checkpoint_reset(void)
{
    memset(&story_checkpoint_host,0,sizeof story_checkpoint_host);
    memset(&story_checkpoint_pending,0,sizeof story_checkpoint_pending);
    story_checkpoint_serial=0;story_checkpoint_sent=0;story_checkpoint_tick=0;story_checkpoint_restore_pending=0;
}
static int story_checkpoint_available(uint32_t mask)
{
    const uint32_t bookmarks=WORD_AT(0x0075f5d0u);
    return mask && story_checkpoint_serial && bookmarks && guest_writable(bookmarks,8u) &&
        WORD_AT(bookmarks+4u)==mask;
}
static void story_checkpoint_extra_players(int restore,uint32_t index)
{
    if(!story_checkpoint_players()) return;
    GuestRegisters saved;save_registers(&saved);
    const uint32_t contexts=WORD_AT(0x007b0c48u),bookmarks=WORD_AT(0x0075f5d0u);
    for(unsigned slot=2u;slot<story_npc_humans();slot++) {
        /* These original __usercall entries take their bookmark in EBX;
           capture takes index/mask in ECX/EDX, restore takes context in EAX. */
        g_ebx=bookmarks+slot*0x238u;
        if(restore) {g_eax=contexts+slot*0x1584u;guest_call(sub_0025EC70,0);}
        else {g_ecx=index;g_edx=1u<<index;guest_call(sub_0025EAD0,1,contexts+slot*0x1584u);}
    }
    restore_registers(&saved);
}
static int story_checkpoint_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || length<sizeof(StoryCheckpoint) || length>sizeof(StoryCheckpoint)+4u ||
       !guest_writable(payload,sizeof(StoryCheckpoint)) || WORD_AT(payload)!=STORY_CHECKPOINT_MAGIC) return 0;
    StoryCheckpoint s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(WORD_AT(0x0076b140u)==2u && guest_writable(peer,4u) && WORD_AT(peer)==WORD_AT(0x00774040u) &&
       s.match==story_npc_match() && s.serial && s.index<32u &&
       (int32_t)(s.serial-story_checkpoint_serial)>0 &&
       (!story_checkpoint_pending.serial || (int32_t)(s.serial-story_checkpoint_pending.serial)>0))
        story_checkpoint_pending=s;
    return 1;
}
static void story_checkpoint_restore_inventory(void)
{
    if(!story_checkpoint_restore_pending || WORD_AT(0x007de30cu)!=7u || !story_checkpoint_players()) return;
    const uint32_t contexts=WORD_AT(0x007b0c48u),bookmarks=WORD_AT(0x0075f5d0u);
    for(unsigned slot=0;slot<story_npc_humans();slot++)
        if(WORD_AT(contexts+slot*0x1584u+0x114u)!=2u ||
           WORD_AT(bookmarks+slot*0x238u+4u)!=story_checkpoint_restore_pending) return;
    GuestRegisters saved;save_registers(&saved);
    for(unsigned slot=0;slot<story_npc_humans();slot++) {
        const uint32_t context=contexts+slot*0x1584u,bookmark=bookmarks+slot*0x238u;
        g_ebx=bookmark;g_eax=context;guest_call(sub_0025EC70,0);
        /* Original restore selects before it copies inventory. Select once the
           restored inventory and the final network-owned body both exist. */
        const uint32_t entity=WORD_AT(context+0x14u),weapon=WORD_AT(bookmark+0x10u);
        if(weapon>1u && weapon<70u) guest_call(sub_001BA130,2,WORD_AT(entity+0x7cu),weapon);
    }
    restore_registers(&saved);story_checkpoint_restore_pending=0;
}
static void story_checkpoint_frame(void)
{
    if(!story_checkpoint_scope()) return;
    story_checkpoint_restore_inventory();
    if(WORD_AT(0x0076b140u)==2u) {
        const StoryCheckpoint s=story_checkpoint_pending;
        if(!s.serial || s.match!=story_npc_match() || !story_checkpoint_players()) return;
        GuestRegisters saved;save_registers(&saved);
        story_checkpoint_replay=1;guest_call(sub_0025EF80,1,s.index);story_checkpoint_replay=0;
        restore_registers(&saved);
        const uint32_t bookmarks=WORD_AT(0x0075f5d0u);
        if(WORD_AT(bookmarks+4u)&(1u<<s.index)) {
            story_checkpoint_serial=s.serial;story_checkpoint_pending.serial=0;
            if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,"[PC-STORY-CHECKPOINT] guest captured index=%u serial=%u\n",s.index,s.serial);
        }
        return;
    }
    if(WORD_AT(0x0076b140u)!=1u || !story_checkpoint_host.serial) return;
    const uint32_t tick=WORD_AT(0x007de318u),frame=g_esp,node=WORD_AT(0x00774028u);
    if(story_checkpoint_sent && (uint32_t)(tick-story_checkpoint_tick)<60u) return;
    if(frame<2048u || !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u)) return;
    story_checkpoint_host.match=story_npc_match();
    const uint32_t buffer=frame-512u;WORD_AT(buffer)=0;
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&story_checkpoint_host,sizeof story_checkpoint_host);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-1024u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof story_checkpoint_host+4u,1u);
    restore_registers(&saved);story_checkpoint_tick=tick;story_checkpoint_sent=1;
}
#endif

void pc_story_network_checkpoint_capture(void (*original)(void))
{
#ifdef _WIN32
    if(story_checkpoint_scope()) {
        if(WORD_AT(0x0076b140u)==2u && !story_checkpoint_replay) {g_esp+=4u;return;}
        if(!guest_writable(g_esp,8u) || WORD_AT(g_esp+4u)>=32u || !story_checkpoint_players()) {g_esp+=4u;return;}
        const uint32_t index=WORD_AT(g_esp+4u),bookmarks=WORD_AT(0x0075f5d0u);
        const int fresh=!(WORD_AT(bookmarks+4u)&(1u<<index));
        original();
        if(fresh && (WORD_AT(bookmarks+4u)&(1u<<index))) {
            story_checkpoint_extra_players(0,index);
            if(WORD_AT(0x0076b140u)==1u) {
                if(!++story_checkpoint_serial) ++story_checkpoint_serial;
                story_checkpoint_host=(StoryCheckpoint){STORY_CHECKPOINT_MAGIC,story_npc_match(),story_checkpoint_serial,index};
                story_checkpoint_sent=0;
                if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,"[PC-STORY-CHECKPOINT] host captured index=%u serial=%u\n",index,story_checkpoint_serial);
            }
        }
        return;
    }
#endif
    original();
}

void pc_story_network_checkpoint_restore(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t bookmarks=WORD_AT(0x0075f5d0u);
    if(story_checkpoint_scope() && story_checkpoint_players() &&
       story_checkpoint_available(WORD_AT(bookmarks+4u))) {
        const uint32_t primary=WORD_AT(0x007b0c48u),current=WORD_AT(0x007b0c7cu);
        /* Original Local co-op saves world/VM state in bookmark0 and restores
           exactly two humans. A guest's current context is its own slot. */
        WORD_AT(0x007b0c7cu)=primary;
        original();
        story_checkpoint_extra_players(1,0u);
        story_checkpoint_restore_pending=WORD_AT(bookmarks+4u);
        WORD_AT(0x007b0c7cu)=current;
        return;
    }
#endif
    original();
}
