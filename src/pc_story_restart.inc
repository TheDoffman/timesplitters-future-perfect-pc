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
    story_objectives_reset();
    story_effect_reset();
    story_cine_reset();
    story_dialogue_reset();
    story_world_reset();
    /* A reload reuses the active config rather than the pending-copy hook. */
    memset(story_npc_fresh,0,sizeof story_npc_fresh);
    story_shots_reset();
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
       message.match!=story_npc_match() ||
       message.epoch!=story_reload_epoch+1u || message.epoch>65535u) return 1;
    if(message.checkpoint && !story_checkpoint_available(message.checkpoint)) {
        /* Reliable capture and restart can arrive in one receiver drain,
           before the next ordinary frame has applied the capture. */
        story_checkpoint_frame();
        if(!story_checkpoint_available(message.checkpoint)) return 1;
    }
    GuestRegisters saved;save_registers(&saved);
    story_restart_replay=1;
    guest_call(sub_000653B0,1,message.checkpoint);
    story_restart_replay=0;
    restore_registers(&saved);
    story_restart_reset(message.epoch);
    if(!message.checkpoint) story_checkpoint_reset();
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
        /* Guests cannot initiate reloads. A checkpoint reload requires a
           locally captured bookmark matching the host's checkpoint mask. */
        if(!story_restart_session() || WORD_AT(0x0076B140u)!=1u ||
           !guest_writable(frame,8u) || (WORD_AT(frame+4u) && !story_checkpoint_available(WORD_AT(frame+4u))) || story_reload_epoch>=65535u ||
           frame<2048u || !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u)) {
            g_esp+=4u;return;
        }
        const StoryRestart message={STORY_RESTART_MAGIC,story_npc_match(),story_reload_epoch+1u,WORD_AT(frame+4u)};
        const uint32_t buffer=frame-256u;
        WORD_AT(buffer)=0;
        memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&message,sizeof message);
        GuestRegisters saved;save_registers(&saved);g_esp=frame-1024u;
        guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof message+4u,1u);
        restore_registers(&saved);
        original();
        story_restart_reset(message.epoch);
        if(!message.checkpoint) story_checkpoint_reset();
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
    const uint32_t checkpoint=guest_call(sub_0003D7B0,1,0x17a7u);
    const uint32_t bookmarks=WORD_AT(0x0075f5d0u);
    const int available=bookmarks && guest_writable(bookmarks,8u) && story_checkpoint_available(WORD_AT(bookmarks+4u));
    if(checkpoint) guest_call(sub_00079D60_gen,6,checkpoint,UINT32_MAX,available?0u:1u,0u,0x002EBD50u,0u);
    restore_registers(&saved);
#endif
}

/* The stock Game Over actions call359160 (network teardown) before653B0.
   A Story reload must keep the joined roster and reliable channel alive. */
#ifdef _WIN32
static int story_result_button(void)
{
    if(!story_restart_session() || WORD_AT(0x0076b140u)!=2u || !guest_writable(g_esp,24u)) return 0;
    const uint32_t callback=WORD_AT(g_esp+20u);
    if(callback!=0x002eb0c0u && callback!=0x002eb290u) return 0;
    g_eax=0;g_esp+=4u;return 1;
}
#endif
void pc_story_network_result_action(void (*original)(void),unsigned action)
{
#ifdef _WIN32
    if(story_network_session() && WORD_AT(0x0076b108u) && story_lobby_mission==0u &&
       (WORD_AT(0x0076b140u)==1u || WORD_AT(0x0076b140u)==2u)) {
        if(!action && story_pending_stage>=2u && WORD_AT(0x0076b140u)==2u) {g_eax=0;g_esp+=4u;return;}
        if(action && story_restart_session()) {
            if(WORD_AT(0x0076b140u)==1u) {
                const uint32_t bookmarks=WORD_AT(0x0075f5d0u);
                const uint32_t checkpoint=action==2u && guest_writable(bookmarks,8u)?WORD_AT(bookmarks+4u):0u;
                if(action==2u && !story_checkpoint_available(checkpoint)) {g_esp+=4u;return;}
                GuestRegisters saved;save_registers(&saved);
                /* Port0 is this instance's single local menu, independently
                   of which network player died or last became current. */
                const uint32_t menu=WORD_AT(0x00761bbcu),state=WORD_AT(0x00761c48u);
                if(guest_writable(menu,8u)) guest_call(sub_00079B70,2,menu,0u);
                if(guest_writable(state,16u)) {WORD_AT(state)|=0x48u;WORD_AT(state+0xcu)=0;}
                guest_call(sub_000653B0,1,checkpoint);
                restore_registers(&saved);
            }
            g_eax=0;g_esp+=4u;return;
        }
    }
#else
    (void)action;
#endif
    original();
}

void pc_story_network_result_menu(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t contexts=WORD_AT(0x007b0c48u),current=WORD_AT(0x007b0c7cu),port=WORD_AT(0x007ba9e0u);
    const int scope=story_restart_session() && WORD_AT(0x0076b140u)==1u &&
        guest_writable(contexts,0x1584u) && WORD_AT(contexts)==0u && WORD_AT(contexts+4u)==0u;
    if(scope) {WORD_AT(0x007b0c7cu)=contexts;WORD_AT(0x007ba9e0u)=0u;}
#endif
    original();
#ifdef _WIN32
    if(scope) {WORD_AT(0x007b0c7cu)=current;WORD_AT(0x007ba9e0u)=port;}
#endif
}

void pc_story_network_pause_style(void)
{
#ifdef _WIN32
    if(!story_restart_session()) return;
    const uint32_t menu=WORD_AT(0x007bb1e8u),builder=WORD_AT(0x007bb1e0u);
    if(!guest_writable(menu,8u) || !guest_writable(WORD_AT(menu+4u),4u) ||
       WORD_AT(WORD_AT(menu+4u))!=0x002f5520u || !guest_writable(builder,0x58u)) return;
    /* The original row factory measures its default font scale before laying
       out each button. Fit all Story pause rows inside the header/footer. */
    const float scale=1.25f;
    memcpy((void *)((uintptr_t)(builder+0x50u)+g_xbox_mem_offset),&scale,sizeof scale);
    memcpy((void *)((uintptr_t)(builder+0x54u)+g_xbox_mem_offset),&scale,sizeof scale);
#endif
}

void pc_story_network_pause_layout(uint32_t menu)
{
#ifdef _WIN32
    if(!story_restart_session() || WORD_AT(0x0076b140u)!=1u || !guest_writable(menu,0x68u) ||
       !guest_writable(WORD_AT(menu+4u),4u) || WORD_AT(WORD_AT(menu+4u))!=0x002f5520u) return;
    /* Original771D0 computes a vertical origin against its parent frame. The
       expanded host list needs the space above that centre, inside the header. */
    float y;memcpy(&y,(const void *)((uintptr_t)(menu+0x64u)+g_xbox_mem_offset),4u);
    if(isfinite(y)) {y-=11.f;memcpy((void *)((uintptr_t)(menu+0x64u)+g_xbox_mem_offset),&y,4u);}
#else
    (void)menu;
#endif
}
