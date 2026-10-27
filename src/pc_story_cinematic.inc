/* Story cameras use peer-local assets and callbacks. Share authored camera tags,
   numeric fades and the primary hero's pose; never send camera/model pointers. */
#ifdef _WIN32
extern void sub_000C6100(void),sub_000D4B00(void),sub_000D4BE0(void);
typedef struct StoryCinematic {
    uint32_t magic,match,serial,active,pose_present,opening,view_visible;
    uint32_t cue_serial,cue_active,cue_tag,cue_flags,cue_mode;
    uint32_t fade_present,fade[7];
    PcCharacterPose pose;
} StoryCinematic;
#define STORY_CINE_MAGIC 0x32594353u
#define STORY_CINE_POSE_MAGIC 0x32505953u
#define STORY_CINE_BASE_BYTES offsetof(StoryCinematic,pose)
typedef struct StoryCinematicBase {uint8_t state[STORY_CINE_BASE_BYTES];uint32_t checksum;} StoryCinematicBase;
typedef struct StoryCinematicPose {uint32_t magic,match,serial,checksum;PcCharacterPose pose;} StoryCinematicPose;
_Static_assert(sizeof(StoryCinematicBase)+4u<=256u,"Cinematic state sender limit");
_Static_assert(sizeof(StoryCinematicPose)+4u<=256u,"Cinematic pose sender limit");
static StoryCinematic story_cine_state,story_cine_pending;
static uint32_t story_cine_serial,story_cine_seen,story_cine_checksum_pending,story_cine_tick;
static uint32_t story_cine_cue_serial,story_cine_cue_active,story_cine_cue_tag,story_cine_cue_flags,story_cine_cue_mode;
static uint32_t story_cine_cue_applied;
static unsigned story_cine_parts;
static int story_cine_published,story_cine_replay,story_cine_was_active;
static int story_cine_session(void)
{ return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u; }
static void story_cine_reset(void)
{
    memset(&story_cine_state,0,sizeof story_cine_state);memset(&story_cine_pending,0,sizeof story_cine_pending);
    story_cine_serial=story_cine_seen=story_cine_parts=0;
    story_cine_cue_serial=story_cine_cue_active=story_cine_cue_tag=story_cine_cue_flags=story_cine_cue_mode=0;
    story_cine_cue_applied=0;story_cine_published=story_cine_replay=story_cine_was_active=0;
}
static uint32_t story_cine_checksum(const StoryCinematic *s)
{
    const uint8_t *bytes=(const uint8_t *)s;uint32_t hash=2166136261u;
    for(unsigned i=0;i<sizeof *s;i++) hash=(hash^bytes[i])*16777619u;
    return hash;
}
static int story_cine_valid(const StoryCinematic *s,int complete)
{
    if(s->magic!=STORY_CINE_MAGIC || !s->serial || s->active>1u || s->pose_present>1u || s->opening>1u ||
       s->view_visible>1u || s->cue_active>1u || s->cue_flags>65535u || s->cue_mode>64u || s->fade_present>1u ||
       (s->cue_active && (!s->cue_serial || !s->cue_tag))) return 0;
    if(complete && s->pose_present && !pc_character_pose_valid(&s->pose)) return 0;
    if(s->fade_present) {
        for(unsigned i=0;i<=4u;i+=2u) {
            float value;memcpy(&value,s->fade+i,4u);
            if(!isfinite(value) || fabsf(value)>1000000.f) return 0;
        }
        if(s->fade[6]>1u) return 0;
    }
    return 1;
}
static uint32_t story_cine_local_context(void)
{
    const uint32_t contexts=WORD_AT(0x007b0c48u);const unsigned count=story_npc_humans();
    if(!contexts || !guest_writable(contexts,count*0x1584u)) return 0;
    for(unsigned slot=0;slot<count;slot++) {
        const uint32_t context=contexts+slot*0x1584u,camera=WORD_AT(context+8u);
        if(WORD_AT(context)==slot && WORD_AT(context+4u)==0u && camera && guest_writable(camera,0x350u)) return context;
    }
    return 0;
}
static uint32_t story_cine_camera_tag(uint32_t descriptor)
{
    const uint32_t pool=WORD_AT(0x00733100u),count=WORD_AT(0x00733104u);
    if(!pool || !count || count>4096u || descriptor<pool ||
       (descriptor-pool)%0xc0u || (descriptor-pool)/0xc0u>=count || !guest_writable(descriptor,0xc0u)) return 0;
    return WORD_AT(descriptor+0xbcu);
}
static int story_cine_script(void (*original)(void),uint32_t address)
{
    if((address!=0x000d4b00u && address!=0x000d4be0u) || story_cine_replay ||
       !story_cine_session() || !story_cine_local_context()) return 0;
    if(WORD_AT(0x0076b140u)==2u) {g_esp+=4u;return 1;}
    if(WORD_AT(0x0076b140u)!=1u || !guest_writable(g_esp,address==0x000d4b00u?28u:8u) ||
       WORD_AT(g_esp+4u)!=WORD_AT(0x007b0c48u)) return 0;
    uint32_t tag=0,flags=0,mode=0;
    if(address==0x000d4b00u) {
        tag=story_cine_camera_tag(WORD_AT(g_esp+8u));flags=WORD_AT(g_esp+16u);mode=WORD_AT(g_esp+20u);
        if(!tag || flags>65535u || mode>64u) return 0;
    } else if(!story_cine_cue_active) return 0;
    original();
    if(!++story_cine_cue_serial) ++story_cine_cue_serial;
    story_cine_cue_active=address==0x000d4b00u;story_cine_cue_tag=tag;
    story_cine_cue_flags=flags;story_cine_cue_mode=mode;
    if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,"[PC-STORY-CINE] host cue=%u active=%u tag=%08X\n",
        story_cine_cue_serial,story_cine_cue_active,tag);
    return 1;
}
static int story_cine_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || !guest_writable(payload,4u)) return 0;
    const uint32_t magic=WORD_AT(payload);
    if(magic!=STORY_CINE_MAGIC && magic!=STORY_CINE_POSE_MAGIC) return 0;
    if(WORD_AT(0x0076b140u)!=2u || !guest_writable(peer,4u) || WORD_AT(peer)!=WORD_AT(0x00774040u)) return 1;
    StoryCinematic base;memset(&base,0,sizeof base);
    StoryCinematicPose pose;memset(&pose,0,sizeof pose);
    uint32_t match,serial,checksum,part;
    if(magic==STORY_CINE_MAGIC && length>=sizeof(StoryCinematicBase) && length<=sizeof(StoryCinematicBase)+4u &&
       guest_writable(payload,sizeof(StoryCinematicBase))) {
        StoryCinematicBase packet;memcpy(&packet,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof packet);
        memcpy(&base,packet.state,STORY_CINE_BASE_BYTES);
        if(!story_cine_valid(&base,0)) return 1;
        match=base.match;serial=base.serial;checksum=packet.checksum;part=1u;
    } else if(magic==STORY_CINE_POSE_MAGIC && length>=sizeof pose && length<=sizeof pose+4u && guest_writable(payload,sizeof pose)) {
        memcpy(&pose,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof pose);
        if(!pose.serial || !pc_character_pose_valid(&pose.pose)) return 1;
        match=pose.match;serial=pose.serial;checksum=pose.checksum;part=2u;
    } else return 1;
    if(match!=story_npc_match() || (int32_t)(serial-story_cine_seen)<=0 ||
       (story_cine_parts && (int32_t)(serial-story_cine_pending.serial)<0)) return 1;
    if(!story_cine_parts || story_cine_pending.serial!=serial || story_cine_pending.match!=match || story_cine_checksum_pending!=checksum) {
        memset(&story_cine_pending,0,sizeof story_cine_pending);story_cine_parts=0;
        story_cine_pending.serial=serial;story_cine_pending.match=match;story_cine_checksum_pending=checksum;
    }
    if(part==1u) memcpy(&story_cine_pending,&base,STORY_CINE_BASE_BYTES);
    else story_cine_pending.pose=pose.pose;
    story_cine_parts|=part;
    if((story_cine_parts&1u) && (!story_cine_pending.pose_present || (story_cine_parts&2u)) &&
       story_cine_checksum(&story_cine_pending)==checksum && story_cine_valid(&story_cine_pending,1)) {
        story_cine_state=story_cine_pending;story_cine_seen=serial;story_cine_parts=0;
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned observations;
            if(observations++<8u) fprintf(stderr,"[PC-STORY-CINE] guest snapshot serial=%u active=%u pose=%u opening=%u\n",serial,story_cine_state.active,story_cine_state.pose_present,story_cine_state.opening);
        }
    }
    return 1;
}
static int story_cine_camera(uint32_t context,int begin,uint32_t tag,uint32_t flags,uint32_t mode)
{
    GuestRegisters saved;save_registers(&saved);
    uint32_t descriptor=0;
    if(begin) {
        descriptor=guest_call(sub_000C6100,1,tag);
        if(!descriptor || story_cine_camera_tag(descriptor)!=tag) {restore_registers(&saved);return 0;}
    }
    const uint32_t contexts=WORD_AT(0x007b0c48u),count=WORD_AT(0x00790950u),current=WORD_AT(0x007b0c7cu);
    /* Original Local Story has a camera for every context. Online has only
       one local view, so scope the original begin/end loops to that view. */
    WORD_AT(0x007b0c48u)=context;WORD_AT(0x00790950u)=1u;WORD_AT(0x007b0c7cu)=context;
    story_cine_replay=1;
    if(begin) guest_call(sub_000D4B00,6,context,descriptor,0u,flags,mode,0u);
    else {
        WORD_AT(context+0x1580u)=0u; /* guest does not run the host's Story end callback */
        guest_call(sub_000D4BE0,1,context);
    }
    story_cine_replay=0;
    WORD_AT(0x007b0c48u)=contexts;WORD_AT(0x00790950u)=count;WORD_AT(0x007b0c7cu)=current;
    restore_registers(&saved);return 1;
}
static void story_cine_frame(void)
{
    if(!story_cine_session() || WORD_AT(0x007de30cu)!=7u) return;
    const uint32_t role=WORD_AT(0x0076b140u);
    if(role==1u) {
        const uint32_t context=WORD_AT(0x007b0c48u),tick=WORD_AT(0x007de318u),frame=g_esp,node=WORD_AT(0x00774028u);
        if(story_cine_published && story_cine_tick==tick) return;
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned guard_observations;
            if(guard_observations++<4u) fprintf(stderr,"[PC-STORY-CINE] host guard context=%08X port=%u frame=%08X node=%08X writable=%d/%d/%d\n",
                context,context && guest_writable(context,8u)?WORD_AT(context+4u):99u,frame,node,
                guest_writable(context,0x1584u),frame>=2048u && guest_writable(frame-2048u,2048u),guest_writable(node,0x35u));
        }
        if(!context || !guest_writable(context,0x1584u) || WORD_AT(context+4u)!=0u || frame<2048u ||
           !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u)) return;
        const uint32_t entity=WORD_AT(context+0x14u),bookmarks=WORD_AT(0x0075f5d0u);
        if(!entity || !guest_writable(entity,0x154u)) return;
        StoryCinematic s;memset(&s,0,sizeof s);
        s.magic=STORY_CINE_MAGIC;s.match=story_npc_match();
        if(!++story_cine_serial) ++story_cine_serial;s.serial=story_cine_serial;
        s.opening=!story_loadout_host[0].equipped &&
            !(bookmarks && guest_writable(bookmarks,8u) && story_checkpoint_available(WORD_AT(bookmarks+4u)));
        s.active=s.opening || story_cine_cue_active || WORD_AT(context+0x114u)==256u || (WORD_AT(0x007de45cu)&0x02000000u);
        const uint32_t camera=WORD_AT(context+8u);
        if(!camera || !guest_writable(camera,0x220u)) return;
        s.view_visible=WORD_AT(camera+0x21cu)&1u;
        s.cue_serial=story_cine_cue_serial;s.cue_active=story_cine_cue_active;
        s.cue_tag=story_cine_cue_tag;s.cue_flags=story_cine_cue_flags;s.cue_mode=story_cine_cue_mode;
        const uint32_t fade=WORD_AT(0x0070e164u),model=WORD_AT(entity+4u);
        if(fade && guest_writable(fade,0x24u)) {
            s.fade_present=1;memcpy(s.fade,(const void *)((uintptr_t)(fade+8u)+g_xbox_mem_offset),sizeof s.fade);
        }
        if(s.active && model && guest_writable(model,0x200u)) {
            pc_character_pose_read(&s.pose,(const void *)((uintptr_t)model+g_xbox_mem_offset));
            s.pose_present=(uint32_t)pc_character_pose_valid(&s.pose);
        }
        if(!s.pose_present) memset(&s.pose,0,sizeof s.pose);
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned observations;
            if(observations++<16u) fprintf(stderr,"[PC-STORY-CINE] host snapshot tick=%u active=%u pose=%u opening=%u clip=%d/%d fade=%08X valid=%d\n",
                tick,s.active,s.pose_present,s.opening,s.pose.clip,s.pose.next_clip,fade,story_cine_valid(&s,1));
        }
        if(!story_cine_valid(&s,1)) return;
        story_cine_state=s;story_cine_published=1;story_cine_tick=tick;
        const uint32_t checksum=story_cine_checksum(&s),buffer=frame-768u;
        StoryCinematicBase base;memcpy(base.state,&s,STORY_CINE_BASE_BYTES);base.checksum=checksum;
        WORD_AT(buffer)=0;memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&base,sizeof base);
        GuestRegisters saved;save_registers(&saved);g_esp=frame-1280u;
        guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof base+4u,1u);
        if(s.pose_present) {
            const StoryCinematicPose pose={STORY_CINE_POSE_MAGIC,s.match,s.serial,checksum,s.pose};
            WORD_AT(buffer)=0;memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&pose,sizeof pose);
            guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof pose+4u,1u);
        }
        restore_registers(&saved);return;
    }
    if(role!=2u || !story_cine_state.serial || story_cine_state.match!=story_npc_match()) return;
    const uint32_t context=story_cine_local_context();if(!context) return;
    const StoryCinematic *s=&story_cine_state;
    if(s->cue_serial && (int32_t)(s->cue_serial-story_cine_cue_applied)>0) {
        if(!story_cine_camera(context,(int)s->cue_active,s->cue_tag,s->cue_flags,s->cue_mode)) return;
        story_cine_cue_applied=s->cue_serial;
    } else if(story_cine_was_active && !s->active) {
        story_cine_camera(context,0,0u,0u,0u);
    }
    const uint32_t camera=WORD_AT(context+8u);
    /* CB110's original Local Story single-view camera policy disables slot1.
       Online owns a separate full view on that peer. Retain all local camera
       flags except the host-authored world-render enable bit. */
    if(camera && guest_writable(camera,0x220u)) {
        WORD_AT(camera+0x21cu)=(WORD_AT(camera+0x21cu)&~1u)|s->view_visible;
    }
    const uint32_t fade=WORD_AT(0x0070e164u);
    if(s->fade_present && fade && guest_writable(fade,0x24u)) {
        if(getenv("TSFP_STORY_TRACE") && WORD_AT(fade+0x20u)!=s->fade[6]) {
            static unsigned observations;
            if(observations++<16u) fprintf(stderr,"[PC-STORY-CINE] guest blackout %u -> host %u opening=%u\n",
                WORD_AT(fade+0x20u),s->fade[6],s->opening);
        }
        memcpy((void *)((uintptr_t)(fade+8u)+g_xbox_mem_offset),s->fade,sizeof s->fade);
    }
    story_cine_was_active=(int)s->active;
}
#endif

int pc_story_network_cinematic_pose(uint32_t entity)
{
#ifdef _WIN32
    if((WORD_AT(0x0076b140u)!=1u && WORD_AT(0x0076b140u)!=2u) ||
       !story_cine_session() || WORD_AT(0x007de30cu)!=7u || !story_cine_state.active ||
       !story_cine_state.pose_present || story_cine_state.match!=story_npc_match() ||
       !guest_writable(entity,0x154u)) return 0;
    const uint32_t context=WORD_AT(entity+0x150u),contexts=WORD_AT(0x007b0c48u);
    if(!context || !contexts || !guest_writable(context,0x1584u) || WORD_AT(context)>=story_npc_humans() ||
       context!=contexts+WORD_AT(context)*0x1584u || WORD_AT(context+0x14u)!=entity ||
       (WORD_AT(0x0076b140u)==1u && !WORD_AT(context))) return 0;
    pc_character_pose_present(entity,&story_cine_state.pose);return 1;
#else
    (void)entity;return 0;
#endif
}

void pc_story_network_world_visibility(void (*original)(void))
{
    original();
#ifdef _WIN32
    if(!story_cine_session() || story_pending_stage!=1u || story_checkpoint_serial || WORD_AT(0x007de30cu)!=7u) return;
    const unsigned role=WORD_AT(0x0076b140u);
    if(role!=1u && role!=2u) return;
    const StoryLoadout *pistol=role==1u?&story_loadout_host[0]:&story_loadout_pending[0];
    if(pistol->equipped && pistol->match==story_npc_match()) return;
    const uint32_t contexts=WORD_AT(0x007b0c48u);
    if(!contexts || !guest_writable(contexts,story_npc_humans()*0x1584u)) return;
    for(unsigned slot=0;slot<story_npc_humans();slot++) {
        const uint32_t context=contexts+slot*0x1584u,entity=WORD_AT(context+0x14u);
        if(!guest_writable(entity,0x154u) || WORD_AT(entity+0x150u)!=context) continue;
        /* Original A0130 builds each world entity's per-view mask at+10 from
           the entity's culling mask. Hide the standing network body until the
           authored first-person handoff ends. Weapon/arm models stay local. */
        WORD_AT(entity+0x10u)&=0xffff0000u;
    }
#endif
}
