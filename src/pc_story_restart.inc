/* First-mission level reload: retain the original engine reload path on every
 * peer. No checkpoint buffers, guest pointers or saved profiles cross the wire. */
#ifdef _WIN32
extern void sub_000653B0(void);
typedef struct StoryRestart {uint32_t magic,match,epoch,checkpoint;} StoryRestart;
#define STORY_RESTART_MAGIC 0x31525353u
static int story_restart_replay;
static int story_restart_session(void)
{
    return story_npc_session() && WORD_AT(0x0079094Cu)==2u &&
        WORD_AT(0x0076B1A4u)==15u && story_pending_stage==1u;
}
static void story_restart_reset(unsigned epoch)
{
    story_reload_epoch=epoch;
    story_npc_reset();story_loadout_reset();
    /* A reload reuses the active config rather than the pending-copy hook. */
    memset(story_npc_fresh,0,sizeof story_npc_fresh);
    memset(story_npc_parts,0,sizeof story_npc_parts);
    memset(story_loadout_pending,0,sizeof story_loadout_pending);
    story_context_start_pending=1;
}
static int story_restart_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || (length<sizeof(StoryRestart) || length>sizeof(StoryRestart)+4u) ||
       !guest_writable(payload,sizeof(StoryRestart)) || WORD_AT(payload)!=STORY_RESTART_MAGIC) return 0;
    StoryRestart message;
    memcpy(&message,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof message);
    if(!story_restart_session() || WORD_AT(0x0076B140u)!=2u ||
       !guest_writable(peer,4u) || WORD_AT(peer)!=WORD_AT(0x00774040u) ||
       message.match!=story_npc_match() || message.checkpoint ||
       message.epoch!=story_reload_epoch+1u || message.epoch>65535u) return 1;
    GuestRegisters saved;save_registers(&saved);
    story_restart_replay=1;
    guest_call(sub_000653B0,1,0u);
    story_restart_replay=0;
    restore_registers(&saved);
    story_restart_reset(message.epoch);
    fprintf(stderr,"[PC-STORY-RESTART] guest reload epoch=%u\n",message.epoch);
    return 1;
}
#endif

void pc_story_network_restart(void (*original)(void))
{
#ifdef _WIN32
    if(story_restart_replay) {original();return;}
    if(story_npc_session() && WORD_AT(0x0079094Cu)==2u) {
        const uint32_t frame=g_esp,node=WORD_AT(0x00774028u);
        /* Guests cannot initiate reloads. Checkpoint reload needs its own
         * authored state transfer; never silently turn it into a level reset. */
        if(!story_restart_session() || WORD_AT(0x0076B140u)!=1u ||
           !guest_writable(frame,8u) || WORD_AT(frame+4u) || story_reload_epoch>=65535u ||
           frame<2048u || !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u)) {
            g_esp+=4u;return;
        }
        const StoryRestart message={STORY_RESTART_MAGIC,story_npc_match(),story_reload_epoch+1u,0u};
        const uint32_t buffer=frame-256u;
        WORD_AT(buffer)=0;
        memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&message,sizeof message);
        GuestRegisters saved;save_registers(&saved);g_esp=frame-1024u;
        guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof message+4u,1u);
        restore_registers(&saved);
        original();
        story_restart_reset(message.epoch);
        fprintf(stderr,"[PC-STORY-RESTART] host reload epoch=%u\n",message.epoch);
        return;
    }
#endif
    original();
}

void pc_story_network_pause_rows(void)
{
#ifdef _WIN32
    if(!story_restart_session() || WORD_AT(0x0076B140u)!=1u) return;
    GuestRegisters saved;save_registers(&saved);
    const uint32_t text=guest_call(sub_0003D7B0,1,0x17a8u);
    if(text) guest_call(sub_00079D60_gen,6,text,UINT32_MAX,0u,0u,0x002EBC30u,0u);
    restore_registers(&saved);
#endif
}
