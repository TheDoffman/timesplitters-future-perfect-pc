/* First-mission ambience is decided by the host. Replicate the actual emitter
   and explosion arguments, not another peer's random-number stream. */
#ifdef _WIN32
extern void sub_000FFDA0(void),sub_000FFE40(void),sub_000EFBD0(void);
extern void sub_00101660(void),sub_000FF420(void),sub_000FF490(void),sub_000FF200(void);
extern void sub_001128C0(void),sub_00080F20(void),sub_000D1EC0(void);
typedef struct StoryEffect {
    uint32_t magic,match,serial,kind,a,b;
    float position[3],direction[3],value;
    uint32_t options,color,room,decal_flags;
} StoryEffect;
#define STORY_EFFECT_MAGIC 0x32574653u
_Static_assert(sizeof(StoryEffect)+4u<=256u,"Story effect sender limit");
static StoryEffect story_effect_pending[256],story_effect_ground[256],story_effect_flight;
static uint32_t story_effect_ground_entities[256];
static unsigned story_effect_read,story_effect_count,story_effect_ground_count,story_effect_depth;
static uint32_t story_effect_serial,story_effect_seen,story_effect_flight_tick,story_effect_flight_phase;
static uint32_t story_effect_events_seen[256],story_effect_flight_seen;
static uint32_t story_effect_storage,story_effect_trail;
static int story_effect_flight_sent,story_effect_flight_fresh;
static int story_effect_decal_replay;
static unsigned story_effect_barrel_replay,story_effect_cosmetic_depth;
static uint32_t story_effect_host_blast_source;
typedef struct StoryLocalBlood {float position[3];uint32_t tick,mode;} StoryLocalBlood;
static StoryLocalBlood story_local_blood[32];
static unsigned story_local_blood_next,story_local_blood_count;
static int story_effect_session(void)
{ return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u; }
static void story_effect_reset(void)
{
    story_effect_read=story_effect_count=story_effect_ground_count=story_effect_depth=0;
    story_effect_serial=story_effect_seen=0;story_effect_trail=0;
    memset(story_effect_events_seen,0,sizeof story_effect_events_seen);story_effect_flight_seen=0;
    story_effect_flight_sent=story_effect_flight_fresh=0;
    story_effect_decal_replay=0;
    story_effect_barrel_replay=story_effect_cosmetic_depth=0;story_effect_host_blast_source=0;
    story_local_blood_next=story_local_blood_count=0;
}
static int story_effect_blood_seen_locally(const StoryEffect *s)
{
    for(unsigned i=0;i<story_local_blood_count;i++) {
        const StoryLocalBlood *local=&story_local_blood[i];float distance=0.f;
        if(local->mode!=s->options || (uint32_t)(WORD_AT(0x007de318u)-local->tick)>30u) continue;
        for(unsigned axis=0;axis<3u;axis++) {const float d=local->position[axis]-s->position[axis];distance+=d*d;}
        if(distance<=.04f) return 1;
    }
    return 0;
}
static int story_effect_valid(const StoryEffect *s)
{
    if(s->magic!=STORY_EFFECT_MAGIC || !s->serial || s->kind>4u ||
       !isfinite(s->value) || fabsf(s->value)>1000000.f) return 0;
    for(unsigned i=0;i<3u;i++) if(!isfinite(s->position[i]) || fabsf(s->position[i])>1000000.f ||
        !isfinite(s->direction[i]) || fabsf(s->direction[i])>1000000.f) return 0;
    if(s->kind==0u) return s->a<=32u && s->b<=32u && s->options<=65535u;
    if(s->kind==1u) return s->a==39u && s->b==UINT32_MAX && s->value==0.f && s->options==1u;
    if(s->kind==2u) return s->a<=3u && !s->b && !s->options;
    if(s->kind==4u) return (s->a==1u || s->a==40u || s->a==47u) &&
        s->b==23u && s->value==0.f && s->options==1u && s->decal_flags<=1u;
    return s->a>=957u && s->a<=963u && s->b<=2u && s->options<=4u &&
        s->value>0.f && s->value<=1000.f && s->decal_flags<=1u &&
        (s->room==UINT32_MAX || s->room<=65535u);
}
static void story_effect_send(StoryEffect *s)
{
    const uint32_t frame=g_esp,node=WORD_AT(0x00774028u);
    if(frame<2048u || !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u)) return;
    s->magic=STORY_EFFECT_MAGIC;s->match=story_npc_match();
    if(!++story_effect_serial) ++story_effect_serial;
    s->serial=story_effect_serial;
    if(!story_effect_valid(s)) return;
    const uint32_t buffer=frame-512u;WORD_AT(buffer)=0;
    memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),s,sizeof *s);
    GuestRegisters saved;save_registers(&saved);g_esp=frame-1024u;
    guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof *s+4u,1u);
    restore_registers(&saved);
    if((s->kind<2u || s->kind==4u) && getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<192u) fprintf(stderr,"[PC-STORY-EFFECT] host serial=%u kind=%u at=%g/%g/%g color=%08X\n",
            s->serial,s->kind,s->position[0],s->position[1],s->position[2],s->color);
    }
}
static int story_effect_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || length<sizeof(StoryEffect) || length>sizeof(StoryEffect)+4u ||
       !guest_writable(payload,sizeof(StoryEffect)) || WORD_AT(payload)!=STORY_EFFECT_MAGIC) return 0;
    StoryEffect s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(WORD_AT(0x0076b140u)!=2u || !guest_writable(peer,4u) || WORD_AT(peer)!=WORD_AT(0x00774040u) ||
       !story_effect_valid(&s) || s.match!=story_npc_match()) return 1;
    if(s.kind==2u) {
        if((int32_t)(s.serial-story_effect_flight_seen)<=0) return 1;
        story_effect_flight=s;story_effect_flight_fresh=1;story_effect_flight_seen=s.serial;
    }
    else {
        /* A newer flight sample must not erase an older one-shot explosion.
           Keep a bounded replay window for independently arriving events. */
        if((int32_t)(story_effect_seen-s.serial)>=256 || story_effect_events_seen[s.serial%256u]==s.serial ||
           story_effect_count>=256u) return 1;
        story_effect_pending[(story_effect_read+story_effect_count)%256u]=s;++story_effect_count;
        story_effect_events_seen[s.serial%256u]=s.serial;
    }
    if((int32_t)(s.serial-story_effect_seen)>0) story_effect_seen=s.serial;
    return 1;
}
/*219F9F..21A015 applies feedback separately from EFBD0. Use the owned
  camera, rather than the host/slot0 view, and the original squared falloff. */
static void story_effect_bomb_feedback(const StoryEffect *s)
{
    const unsigned count=story_npc_humans();
    const uint32_t pool=WORD_AT(0x007b0c48u);
    if(!count || count>4u || !guest_writable(pool,count*0x1584u)) return;
    for(unsigned slot=0;slot<count;slot++) {
        const uint32_t context=pool+slot*0x1584u,camera=WORD_AT(context+8u);
        if(WORD_AT(context)!=slot || WORD_AT(context+4u)!=0u || !guest_writable(camera,0x3a4u)) continue;
        float distance=0.f;
        for(unsigned axis=0;axis<3u;axis++) {
            float coordinate;memcpy(&coordinate,(const void *)((uintptr_t)(camera+0x27cu+axis*4u)+g_xbox_mem_offset),4u);
            if(!isfinite(coordinate)) return;
            const float delta=s->position[axis]-coordinate;distance+=delta*delta;
        }
        const float strength=(1200.f-distance)*0.0008333333535119891f;
        if(strength>0.f) {
            const float amplitude=strength*.5f,decay=strength*.2f;
            uint32_t amplitude_bits,strength_bits,decay_bits;
            memcpy(&amplitude_bits,&amplitude,4u);memcpy(&strength_bits,&strength,4u);memcpy(&decay_bits,&decay,4u);
            guest_call(sub_000D1EC0,4,context,amplitude_bits,strength_bits,decay_bits);
            const float response=2.75f,amount=fminf(strength,.9f);
            memcpy((void *)((uintptr_t)(context+0xfbcu)+g_xbox_mem_offset),&response,4u);
            memcpy((void *)((uintptr_t)(context+0xfc0u)+g_xbox_mem_offset),&amount,4u);
        }
        return;
    }
}
static void story_effect_frame(void)
{
    if(!story_effect_session() || WORD_AT(0x0076b140u)!=2u || WORD_AT(0x007de30cu)!=7u) return;
    if(!story_effect_storage) story_effect_storage=xbox_HeapAlloc(32u,4u);
    if(!story_effect_storage || !guest_writable(story_effect_storage,32u)) return;
    const uint32_t position=story_effect_storage+4u,direction=position+12u;
    GuestRegisters saved;save_registers(&saved);
    while(story_effect_count) {
        const StoryEffect s=story_effect_pending[story_effect_read];
        story_effect_read=(story_effect_read+1u)%256u;--story_effect_count;
        if(s.match!=story_npc_match()) continue;
        memcpy((void *)((uintptr_t)position+g_xbox_mem_offset),s.position,12u);
        memcpy((void *)((uintptr_t)direction+g_xbox_mem_offset),s.direction,12u);
        uint32_t value;memcpy(&value,&s.value,4u);
        if(s.kind==0u) {
            const uint32_t emitter=guest_call(sub_000FFDA0,6,s.a,s.b,position,direction,value,s.options);
            if(emitter && guest_writable(emitter,0x9cu)) guest_call(sub_000FFE40,2,emitter,s.color);
        } else if(s.kind==1u) {
            guest_call(sub_000EFBD0,8,s.a,s.b,position,direction,0u,0u,value,s.options);
            story_effect_bomb_feedback(&s);
        }
        else if(s.kind==4u) {
            /*40/47 dispatch through EDD40's EF444 branch: four authored
               emitters plus their original scale and camera feedback. Their
               empty ordinary particle-table rows do not mean no visuals.
               Keep that profile and direction; never replay host owners. */
            ++story_effect_barrel_replay;
            guest_call(sub_000EFBD0,8,s.a,UINT32_MAX,position,
                       s.decal_flags?direction:0u,0u,0u,0u,s.options);
            --story_effect_barrel_replay;
        }
        else if(s.kind==3u) {
            if(story_effect_blood_seen_locally(&s)) continue;
            /* Resolve the asset locally; host texture handles are not portable. */
            const uint32_t texture=guest_call(sub_00080F20,1,s.a);
            if(texture) {
                story_effect_decal_replay=1;
                guest_call(sub_001128C0,9,texture,s.b,s.color,s.options,value,s.decal_flags,s.room,position,direction);
                story_effect_decal_replay=0;
            }
        }
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned observations;
            if(observations++<192u) fprintf(stderr,"[PC-STORY-EFFECT] guest serial=%u kind=%u at=%g/%g/%g color=%08X\n",
                s.serial,s.kind,s.position[0],s.position[1],s.position[2],s.color);
        }
    }
    if(story_effect_flight_fresh && story_effect_flight.match==story_npc_match()) {
        const StoryEffect *s=&story_effect_flight;story_effect_flight_fresh=0;
        memcpy((void *)((uintptr_t)position+g_xbox_mem_offset),s->position,12u);
        if(s->a==2u || s->a==3u) {
            if(!story_effect_trail) {
                WORD_AT(story_effect_storage)=0;
                story_effect_trail=guest_call(sub_00101660,5,story_effect_storage,0x7eu,0u,position,65535u);
                if(story_effect_trail) guest_call(sub_000FF420,2,story_effect_trail,0x42200000u);
            }
            if(story_effect_trail) guest_call(sub_000FF490,2,story_effect_trail,position);
        } else if(story_effect_trail) {
            guest_call(sub_000FF200,1,story_effect_trail);story_effect_trail=0;
        }
    }
    restore_registers(&saved);
}
#endif

/*1128C0 performs the original scene raycast/decal creation, without damage or
  death side effects. Capture only the known blood textures957..963. */
void pc_story_network_blood_decal(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    StoryEffect s;memset(&s,0,sizeof s);
    if(!story_effect_decal_replay && story_effect_session() && guest_writable(frame,40u)) {
        const uint32_t texture=WORD_AT(frame+4u);
        for(uint32_t i=957u;i<=963u;i++) if(WORD_AT(0x004d2d64u+i*24u)==texture && texture) {s.a=i;break;}
        const uint32_t position=WORD_AT(frame+32u),direction=WORD_AT(frame+36u);
        if(s.a && (WORD_AT(0x0076b140u)==1u || WORD_AT(0x0076b140u)==2u) &&
           guest_writable(position,12u) && guest_writable(direction,12u)) {
            s.kind=3u;s.b=WORD_AT(frame+8u);s.color=WORD_AT(frame+12u);s.options=WORD_AT(frame+16u);
            memcpy(&s.value,(const void *)((uintptr_t)(frame+20u)+g_xbox_mem_offset),4u);
            s.decal_flags=WORD_AT(frame+24u);s.room=WORD_AT(frame+28u);
            memcpy(s.position,(const void *)((uintptr_t)position+g_xbox_mem_offset),12u);
            memcpy(s.direction,(const void *)((uintptr_t)direction+g_xbox_mem_offset),12u);
        } else s.a=0;
    }
#endif
    original();
#ifdef _WIN32
    if(s.a && g_eax) {
        if(WORD_AT(0x0076b140u)==1u) story_effect_send(&s);
        else {
            /* Guest-owned impacts still need immediate local feedback, even
               when the host does not create offscreen cosmetic blood. Avoid
               adding its matching nearby broadcast decal a second time. */
            StoryLocalBlood *local=&story_local_blood[story_local_blood_next++%32u];
            memcpy(local->position,s.position,sizeof local->position);
            local->tick=WORD_AT(0x007de318u);local->mode=s.options;
            if(story_local_blood_count<32u) ++story_local_blood_count;
        }
    }
#endif
}

void pc_story_network_effect_decision(void (*original)(void),unsigned kind)
{
#ifdef _WIN32
    if(story_effect_session()) {
        if(WORD_AT(0x0076b140u)==2u) {g_eax=0;g_esp+=4u;return;}
        if(WORD_AT(0x0076b140u)==1u) {
            const unsigned depth=story_effect_depth;story_effect_depth|=1u<<kind;
            original();story_effect_depth=depth;
            if(kind==0u && !depth) {
                for(unsigned i=0;i<story_effect_ground_count;i++) {
                    const uint32_t emitter=story_effect_ground_entities[i];
                    if(emitter && guest_writable(emitter,0x9cu)) story_effect_ground[i].color=WORD_AT(emitter+0x98u);
                    story_effect_send(&story_effect_ground[i]);
                }
                story_effect_ground_count=0;
            }
            if(kind==1u) {
                const uint32_t phase=WORD_AT(0x0078d0e0u),tick=WORD_AT(0x007de318u);
                if(phase && story_effect_flight_sent && story_effect_flight_tick==tick) return;
                if(!phase && story_effect_flight_sent && !story_effect_flight_phase &&
                   (uint32_t)(tick-story_effect_flight_tick)<60u) return;
                StoryEffect s;memset(&s,0,sizeof s);s.kind=2u;s.a=phase;
                memcpy(s.position,(const void *)((uintptr_t)0x0078d0e8u+g_xbox_mem_offset),12u);
                memcpy(s.direction,(const void *)((uintptr_t)0x0078d0f4u+g_xbox_mem_offset),12u);
                memcpy(&s.value,(const void *)((uintptr_t)0x0078d100u+g_xbox_mem_offset),4u);
                story_effect_send(&s);story_effect_flight_phase=phase;story_effect_flight_tick=tick;story_effect_flight_sent=1;
            }
            return;
        }
    }
#else
    (void)kind;
#endif
    original();
}

void pc_story_network_effect_create(void (*original)(void),unsigned kind)
{
#ifdef _WIN32
    StoryEffect s;memset(&s,0,sizeof s);s.kind=kind;
    const uint32_t frame=g_esp;
    /* The recorded mesh1341 uses218650, which creates authored47/40.
       BC670 is the separate standard prop blast. All bypass ambience scope. */
    const int authored_barrel=kind==1u && guest_writable(frame,36u) &&
        ((WORD_AT(frame)==0x00218702u && WORD_AT(frame+4u)==47u) ||
         (WORD_AT(frame)==0x00218727u && WORD_AT(frame+4u)==40u));
    const int barrel=authored_barrel || (kind==1u && guest_writable(frame,36u) && WORD_AT(frame)==0x000bc70cu &&
        WORD_AT(frame+4u)==1u && WORD_AT(frame+8u)==23u && !WORD_AT(frame+16u));
    int capture=story_effect_session() && WORD_AT(0x0076b140u)==1u &&
        ((story_effect_depth&(1u<<kind)) || barrel) && guest_writable(frame,kind?36u:28u);
    if(capture) {
        const uint32_t position=WORD_AT(frame+12u),direction=WORD_AT(frame+16u);
        capture=guest_writable(position,12u) &&
            (direction?guest_writable(direction,12u):barrel);
        if(capture) {
            s.a=WORD_AT(frame+4u);s.b=WORD_AT(frame+8u);
            memcpy(s.position,(const void *)((uintptr_t)position+g_xbox_mem_offset),12u);
            if(!barrel || (direction && guest_writable(direction,12u))) memcpy(s.direction,(const void *)((uintptr_t)direction+g_xbox_mem_offset),12u);
            memcpy(&s.value,(const void *)((uintptr_t)(frame+(kind?28u:20u))+g_xbox_mem_offset),4u);
            s.options=WORD_AT(frame+(kind?32u:24u));
            if(barrel) {
                /* Preserve the authored profile and optional direction. Never
                   transfer the host's local attacker/source entity handles. */
                s.kind=4u;s.b=23u;
                s.decal_flags=direction && guest_writable(direction,12u);
            }
            else if(kind && (WORD_AT(frame+20u) || WORD_AT(frame+24u))) capture=0;
        }
    }
#endif
    original();
#ifdef _WIN32
    if(capture) {
        if(kind==0u && story_effect_ground_count<256u) {
            const unsigned slot=story_effect_ground_count++;
            story_effect_ground[slot]=s;story_effect_ground_entities[slot]=g_eax;
        } else if(kind==1u) story_effect_send(&s);
    }
#else
    (void)kind;
#endif
}

/* EDD40 creates the original particles/camera feedback. Its separate EDBE0
   Story event and EFF90's BB850 radius damage honor these original effect bits.
   Set them before original initialization, rather than after the first blast. */
void pc_story_network_blast_start(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t effect=guest_writable(g_esp,8u)?WORD_AT(g_esp+4u):0u;
    const int cosmetic=story_effect_barrel_replay && story_effect_session() &&
        WORD_AT(0x0076b140u)==2u && guest_writable(effect,0x1c4u) &&
        WORD_AT(effect)==23u && (WORD_AT(effect+4u)==1u ||
        WORD_AT(effect+4u)==40u || WORD_AT(effect+4u)==47u);
    if(cosmetic) {WORD_AT(effect+0xcu)|=0x20001u;++story_effect_cosmetic_depth;}
#endif
    original();
#ifdef _WIN32
    if(cosmetic) --story_effect_cosmetic_depth;
#endif
}

/* Original186480 also mutates the host's destructible navigation cells.
   Visual replay must not run another world/script destruction pass. */
void pc_story_network_blast_cells(void (*original)(void))
{
#ifdef _WIN32
    if(story_effect_cosmetic_depth) {g_esp+=4u;return;}
#endif
    original();
}

/* Story host owns world explosions even when a remote human receives credit.
   Stock35AF40 otherwise rejects that remote source before sending/applying hits.
   Keep the original serializer and attribution; scope the ownership exception to
   the radius update of the authored218650 barrel kinds, never human gunfire. */
void pc_story_network_blast_update(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t previous=story_effect_host_blast_source;
    const uint32_t effect=guest_writable(g_esp,8u)?WORD_AT(g_esp+4u):0u;
    story_effect_host_blast_source=0;
    if(story_effect_session() && WORD_AT(0x0076b140u)==1u && guest_writable(effect,0x1c4u) &&
       WORD_AT(effect)==23u && (WORD_AT(effect+4u)==40u || WORD_AT(effect+4u)==47u))
        story_effect_host_blast_source=WORD_AT(effect+0x10cu);
#endif
    original();
#ifdef _WIN32
    story_effect_host_blast_source=previous;
#endif
}
void pc_story_network_blast_source(void (*original)(void))
{
#ifdef _WIN32
    if(story_effect_host_blast_source && g_esi==story_effect_host_blast_source &&
       guest_writable(g_esi,0x158u) && WORD_AT(g_esi+0x20u)==8u &&
       guest_writable(WORD_AT(g_esi+0x150u),8u)) {g_eax=1u;g_esp+=4u;return;}
#endif
    original();
}
