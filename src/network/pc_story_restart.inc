/* First-mission level reload: retain the original engine reload path on every
 * peer. No checkpoint buffers, guest pointers or saved profiles cross the wire. */
#ifdef _WIN32
extern void sub_000653B0(void);
extern void sub_00065470_gen(void),sub_00065C70(void);
extern void sub_002EAFC0_gen(void),sub_002EB030_gen(void),sub_002EC090_gen(void),sub_002EC650_gen(void);
static unsigned story_rewards_exit_depth;
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
    story_reload_epoch=epoch;story_rewards_tracking=0;
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
void pc_story_network_result_action(void (*original)(void),unsigned action);
#ifdef _WIN32
extern void sub_002EB0A0(void);
extern void sub_002CBBA0(void),sub_002CBAF0(void),sub_002CBB60(void);
extern void sub_002CB550(void),sub_00078980(void);
extern void sub_000668F0(void),sub_00356870(void),sub_00069A60(void);
static int story_rewards_waiting;
static uint32_t story_rewards_profiles[16];
static unsigned story_rewards_profile_count;
/* Capture persistent profile identities before opening the browser: joining
   already reorders them by network player ID. Metadata pool order is not profile
   identity: players may select any of their stored profiles. */
static void story_rewards_profiles_capture(void)
{
    if(!story_network_session() || WORD_AT(0x79094cu)!=101u ||
       WORD_AT(0x76b1a4u)>=4u) return;
    const unsigned count=WORD_AT(0x74e934u);
    story_rewards_profile_count=0;
    if(!count || count>16u) return;
    for(unsigned i=0;i<count;i++) {
        const uint32_t metadata=WORD_AT(0x7bedc0u+i*0x1f54u);
        if(metadata<0x7bc9c0u || metadata>=0x7bedc0u || (metadata-0x7bc9c0u)%0x40u) return;
        for(unsigned previous=0;previous<i;previous++) if(story_rewards_profiles[previous]==metadata) return;
        story_rewards_profiles[i]=metadata;
    }
    static unsigned captures;
    if(getenv("TSFP_STORY_TRACE") && captures++<4u)
        fprintf(stderr,"[PC-STORY-PROFILES] entry captured=%u first=%08X phase=%u\n",count,story_rewards_profiles[0],WORD_AT(0x76b1a4u));
    story_rewards_profile_count=count;
}

/* A reliable host gameplay->ending configuration is the completion authority.
   Original668F0 records co-op completion in this instance's loaded profiles;
   original69A60 selects its own port0 profile for unlock evaluation. Do not
   invoke66FC0's result menus or a guest campaign scheduler. */
/* Select this peer's loaded profile for its completion recorder. Original
   frontend2D2C70 skips the2CB210 pre-match snapshot for BOTH network roles
   (2D2CB4 branches past it whenever3533F0 is true). Preserve that same reward
   baseline on guests: snapshotting only here hides rewards shown by the host. */
static void story_rewards_begin(void)
{
    if(story_rewards_tracking || !story_network_session() || !WORD_AT(0x76b108u) ||
       WORD_AT(0x76b140u)!=2u || WORD_AT(0x76b1a4u)!=15u ||
       story_lobby_mission!=0u || story_pending_stage!=1u ||
       WORD_AT(0x79094cu)!=2u || WORD_AT(0x7de458u)!=0x80008u) return;
    const unsigned count=WORD_AT(0x790950u);
    const uint32_t contexts=WORD_AT(0x7b0c48u);
    if(count<2u || count>4u || !guest_writable(contexts,count*0x1584u)) return;
    GuestRegisters saved;save_registers(&saved);
    const uint32_t local=guest_call(sub_00356870,1,0u);
    if(local<count && WORD_AT(0x7bedc0u+local*0x1f54u)) {
        guest_call(sub_00069A60,1,1u);
        story_rewards_tracking=1;
    }
    restore_registers(&saved);
}
static void story_rewards_record(unsigned mission,unsigned stage)
{
    const unsigned count=WORD_AT(0x790950u);
    const uint32_t contexts=WORD_AT(0x7b0c48u),actors=WORD_AT(0x7356d8u);
    if(!story_network_session() || !WORD_AT(0x76b108u) || WORD_AT(0x76b140u)!=2u ||
       mission!=0u || stage!=2u || story_lobby_mission!=0u || story_pending_stage!=1u ||
       WORD_AT(0x79094cu)!=2u || WORD_AT(0x7de458u)!=0x80008u ||
       count<2u || count>4u || !guest_writable(contexts,count*0x1584u) ||
       !guest_writable(actors,count*0xea0u)) return;
    story_rewards_begin();
    if(!story_rewards_tracking) return;
    GuestRegisters saved;save_registers(&saved);
    const uint32_t local=guest_call(sub_00356870,1,0u);
    if(local<count && WORD_AT(0x7bedc0u+local*0x1f54u)) {
        const uint32_t result=WORD_AT(0x7de45cu);
        WORD_AT(0x7de45cu)=result|0x8000u;
        guest_call(sub_000668F0,0);
        WORD_AT(0x7de45cu)=result;
        if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,
            "[PC-STORY-REWARDS] recorded guest mission=%u local=%u humans=%u\n",mission,local,count);
    }
    restore_registers(&saved);
}
static unsigned story_rewards_count(void)
{
    GuestRegisters saved;save_registers(&saved);
    const unsigned count=guest_call(sub_002CB550,0);
    restore_registers(&saved);return count;
}
static uint32_t story_reward_text;
static int story_rewards_guest(void)
{
    return story_network_session() && WORD_AT(0x76b108u) && WORD_AT(0x76b140u)==2u &&
        story_lobby_mission==0u && story_pending_stage==3u && WORD_AT(0x79094cu)==106u;
}
static void story_rewards_build(void)
{
    if(!story_rewards_guest()) {sub_002CBBA0();return;}
    story_rewards_waiting=WORD_AT(0x75fd88u)>=story_rewards_count();
    if(!story_rewards_waiting) {sub_002CBBA0();return;}
    const uint32_t menu=guest_writable(g_esp,8u)?WORD_AT(g_esp+4u):0u;
    if(!story_reward_text) story_reward_text=xbox_HeapAlloc(128u,4u);
    if(!story_reward_text || !guest_writable(story_reward_text,128u) || !guest_writable(menu,0x21cu)) {
        g_esp+=4u;return;
    }
    memcpy((void *)((uintptr_t)story_reward_text+g_xbox_mem_offset),"Mission complete",17u);
    memcpy((void *)((uintptr_t)(story_reward_text+32u)+g_xbox_mem_offset),
        "Waiting for host to start the next mission.",43u);
    GuestRegisters saved;save_registers(&saved);
    WORD_AT(menu+0x218u)=1u;
    guest_call(sub_00075B70,5,0x100000u,story_reward_text,0x425a6666u,0u,0u);
    guest_call(sub_00071C50,1,0x10c04u);
    guest_call(sub_00079D60_gen,6,story_reward_text+32u,0u,1u,0u,0u,0u);
    guest_call(sub_000771D0,1,menu);
    guest_call(sub_000770E0,2,menu,0u);
    restore_registers(&saved);g_esp+=4u;
}
static void story_rewards_page(void)
{
    /* The manual wrapper also covers the original timer's direct tail call. */
    sub_002CBAF0();
}
static void story_rewards_tick(void)
{
    if(story_rewards_guest() && story_rewards_waiting) {g_esp+=4u;return;}
    sub_002CBB60();
}
static void story_rewards_continue(void)
{
    /* The initial rewards Continue opens an unlock dialog even when the guest
       has no local unlock rows. Keep it on rewards until the host's config. */
    pc_story_network_result_action(sub_002EB0A0,0u);
}
static int story_result_button(void (*original)(void))
{
    if(WORD_AT(0x0076b140u)!=2u || !guest_writable(g_esp,24u)) return 0;
    const uint32_t callback=WORD_AT(g_esp+20u);
    if(story_network_session() && WORD_AT(0x0076b108u) && story_lobby_mission==0u &&
       story_pending_stage==3u && WORD_AT(0x0079094cu)==106u && callback==0x002eb0a0u) {
        const uint32_t frame=g_esp,flags=WORD_AT(frame+12u);
        WORD_AT(frame+12u)=flags|1u;
        original(); /* retain a visible, disabled row while waiting for host */
        WORD_AT(frame+12u)=flags;
        return 1;
    }
    if(!story_restart_session()) return 0;
    if(callback!=0x002eb0c0u && callback!=0x002eb290u) return 0;
    g_eax=0;g_esp+=4u;return 1;
}
#endif
/* Keep original reward pages and their timer. Replace only the exhausted
 * guest page's transition to5236DC, which otherwise reaches frontend GO. */
void pc_story_network_rewards_page(void (*original)(void))
{
#ifdef _WIN32
    if(story_rewards_guest()) {
        const unsigned cursor=WORD_AT(0x75fd88u),count=story_rewards_count();
        if(cursor<count && count-cursor>3u) {original();return;}
        if(cursor<count) {
            WORD_AT(0x75fd88u)=cursor+3u;
            /* Original2CBAF0 tail-jumps here with its caller's menu argument.
               Forward that frame; a fresh zero-argument call would reinterpret
               the callback return address as a menu pointer. */
            sub_00078980();return;
        }
        g_esp+=4u;return;
    }
#endif
    original();
}
/* Original2D6E10 selects loading descriptor5236DC when there are no new unlocks.
 * That bypasses2CBBA0 entirely, so its exhausted-page adapter cannot create the
 * guest wait panel. Route this menu instance to the reward descriptor instead;
 * preserve the real zero reward count and the original save/transport paths. */
void pc_story_network_rewards_menu(void (*original)(void))
{
#ifdef _WIN32
    if(story_online_return_menu(original)) return;
    const uint32_t frame=g_esp;
    if(story_rewards_guest() && guest_writable(frame,16u) &&
       WORD_AT(frame+8u)==0x005236dcu && !story_rewards_count()) {
        WORD_AT(frame+8u)=0x00523900u;
        original();
        WORD_AT(frame+8u)=0x005236dcu;
        return;
    }
#endif
    original();
}
/* Network construction moves active profiles by player ID and leaves scratch
 * profiles in their old slots. Frontend1922D0 enumerates persistent profile IDs
 * instead. Restore that view only for its synchronous original save pass, then
 * put updated bodies back in their network slots. Metadata/checksums belong to
 * the original save routines and are never fabricated or bypassed here. */
void pc_story_network_profile_save(void (*original)(void))
{
#ifdef _WIN32
    static unsigned observations;
    if(getenv("TSFP_STORY_TRACE") && observations++<12u) fprintf(stderr,
        "[PC-STORY-PROFILE-SAVE] role=%u scene=%u pending=%u enabled=%u story=%d profiles=%u captured=%u caller=%08X\n",
        WORD_AT(0x76b140u),WORD_AT(0x79094cu),story_pending_stage,WORD_AT(0x76b108u),
        story_network_session(),WORD_AT(0x74e934u),story_rewards_profile_count,
        guest_writable(g_esp,4u)?WORD_AT(g_esp):0u);
    if(story_rewards_guest()) {
        enum { PROFILE_BYTES=0x1f54u,PROFILE_SLOTS=16u };
        const uint32_t table=0x7bedc0u;
        const unsigned count=WORD_AT(0x74e934u);
        unsigned source[PROFILE_SLOTS],changed=0;
        if(!count || count>PROFILE_SLOTS || count!=story_rewards_profile_count ||
           !guest_writable(table,PROFILE_BYTES*PROFILE_SLOTS)) {
            original();return;
        }
        for(unsigned i=0;i<count;i++) {
            unsigned matches=0;source[i]=0;
            for(unsigned slot=0;slot<PROFILE_SLOTS;slot++) {
                if(WORD_AT(table+slot*PROFILE_BYTES)==story_rewards_profiles[i]) {source[i]=slot;++matches;}
            }
            if(matches!=1u) {original();return;}
            changed|=source[i]!=i;
        }
        if(!changed) {original();return;}
        uint8_t *saved=(uint8_t *)malloc((PROFILE_SLOTS+count)*PROFILE_BYTES);
        if(!saved) {original();return;}
        uint8_t *updated=saved+PROFILE_SLOTS*PROFILE_BYTES;
        uint8_t *profiles=(uint8_t *)((uintptr_t)table+g_xbox_mem_offset);
        memcpy(saved,profiles,PROFILE_SLOTS*PROFILE_BYTES);
        for(unsigned i=0;i<count;i++) memcpy(profiles+i*PROFILE_BYTES,saved+source[i]*PROFILE_BYTES,PROFILE_BYTES);
        const uint32_t aggregate=WORD_AT(0x6f1e0cu);
        for(unsigned i=0;i<count;i++) {
            const uint32_t start=table+source[i]*PROFILE_BYTES;
            if(aggregate>=start && aggregate-start<PROFILE_BYTES)
                WORD_AT(0x6f1e0cu)=table+i*PROFILE_BYTES+(aggregate-start);
        }
        original();
        WORD_AT(0x6f1e0cu)=aggregate;
        memcpy(updated,profiles,count*PROFILE_BYTES);
        memcpy(profiles,saved,PROFILE_SLOTS*PROFILE_BYTES);
        for(unsigned i=0;i<count;i++) memcpy(profiles+source[i]*PROFILE_BYTES,updated+i*PROFILE_BYTES,PROFILE_BYTES);
        free(saved);return;
    }
#endif
    original();
}
void pc_story_network_result_action(void (*original)(void),unsigned action)
{
#ifdef _WIN32
    if(story_network_session() && getenv("TSFP_STORY_TRACE")) fprintf(stderr,
        "[PC-STORY-RESULT] caller=%08X role=%u phase=%u scene=%u pending=%u action=%u rewards_depth=%u exit=%u\n",
        guest_writable(g_esp,4u)?WORD_AT(g_esp):0u,WORD_AT(0x76b140u),WORD_AT(0x76b1a4u),
        WORD_AT(0x79094cu),story_pending_stage,action,story_rewards_exit_depth,original==sub_00065470_gen);
    if(story_network_session() && WORD_AT(0x0076b108u) && story_lobby_mission==0u &&
       (WORD_AT(0x0076b140u)==1u || WORD_AT(0x0076b140u)==2u)) {
        if(!action && story_pending_stage>=2u && WORD_AT(0x0076b140u)==2u) {g_eax=0;g_esp+=4u;return;}
        if(!action && story_rewards_exit_depth && original==sub_00065470_gen && story_pending_stage==3u &&
           WORD_AT(0x0079094cu)==106u) {
            /* The final reward callback tail-calls the generic exit scheduler.
               In Online mode that reloads the lobby's old pending config.
               Use the original campaign next-load builder instead; its
               pending-copy hook sends the coordinated next generation. */
            if(!story_load_header[0] || story_load_header[0]>4u || story_load_header[1]!=1u) {
                g_eax=0;g_esp+=4u;return;
            }
            sub_00065C70();
            return;
        }
        if(!action && story_pending_stage==3u && WORD_AT(0x0079094cu)==106u &&
           (original==sub_002EAFC0_gen || original==sub_002EB030_gen ||
            original==sub_002EC090_gen || original==sub_002EC650_gen)) {
            /* Scope only reward Continue callbacks. Quit and unrelated uses
               of the same generic scheduler must retain their old behavior. */
            ++story_rewards_exit_depth;original();--story_rewards_exit_depth;
            return;
        }
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
