/* First-mission ambience is decided by the host. Replicate the actual emitter
   and explosion arguments, not another peer's random-number stream. */
#ifdef _WIN32
extern void sub_000FFDA0(void),sub_000FFE40(void),sub_000EFBD0(void);
extern void sub_00101660(void),sub_000FF420(void),sub_000FF490(void),sub_000FF200(void);
typedef struct StoryEffect {
    uint32_t magic,match,serial,kind,a,b;
    float position[3],direction[3],value;
    uint32_t options,color;
} StoryEffect;
#define STORY_EFFECT_MAGIC 0x31574653u
_Static_assert(sizeof(StoryEffect)+4u<=256u,"Story effect sender limit");
static StoryEffect story_effect_pending[256],story_effect_ground[256],story_effect_flight;
static uint32_t story_effect_ground_entities[256];
static unsigned story_effect_read,story_effect_count,story_effect_ground_count,story_effect_depth;
static uint32_t story_effect_serial,story_effect_seen,story_effect_flight_tick,story_effect_flight_phase;
static uint32_t story_effect_events_seen[256],story_effect_flight_seen;
static uint32_t story_effect_storage,story_effect_trail;
static int story_effect_flight_sent,story_effect_flight_fresh;
static int story_effect_session(void)
{ return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u; }
static void story_effect_reset(void)
{
    story_effect_read=story_effect_count=story_effect_ground_count=story_effect_depth=0;
    story_effect_serial=story_effect_seen=0;story_effect_trail=0;
    memset(story_effect_events_seen,0,sizeof story_effect_events_seen);story_effect_flight_seen=0;
    story_effect_flight_sent=story_effect_flight_fresh=0;
}
static int story_effect_valid(const StoryEffect *s)
{
    if(s->magic!=STORY_EFFECT_MAGIC || !s->serial || s->kind>2u ||
       !isfinite(s->value) || fabsf(s->value)>1000000.f) return 0;
    for(unsigned i=0;i<3u;i++) if(!isfinite(s->position[i]) || fabsf(s->position[i])>1000000.f ||
        !isfinite(s->direction[i]) || fabsf(s->direction[i])>1000000.f) return 0;
    if(s->kind==0u) return s->a<=32u && s->b<=32u && s->options<=65535u;
    if(s->kind==1u) return s->a==39u && s->b==UINT32_MAX && s->value==0.f && s->options==1u;
    return s->a<=3u && !s->b && !s->options;
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
    if(s->kind<2u && getenv("TSFP_STORY_TRACE")) {
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
        } else guest_call(sub_000EFBD0,8,s.a,s.b,position,direction,0u,value,0u,s.options);
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
    int capture=story_effect_session() && WORD_AT(0x0076b140u)==1u &&
        (story_effect_depth&(1u<<kind)) && guest_writable(frame,kind?36u:28u);
    if(capture) {
        const uint32_t position=WORD_AT(frame+12u),direction=WORD_AT(frame+16u);
        capture=guest_writable(position,12u) && guest_writable(direction,12u);
        if(capture) {
            s.a=WORD_AT(frame+4u);s.b=WORD_AT(frame+8u);
            memcpy(s.position,(const void *)((uintptr_t)position+g_xbox_mem_offset),12u);
            memcpy(s.direction,(const void *)((uintptr_t)direction+g_xbox_mem_offset),12u);
            memcpy(&s.value,(const void *)((uintptr_t)(frame+(kind?24u:20u))+g_xbox_mem_offset),4u);
            s.options=WORD_AT(frame+(kind?32u:24u));
            if(kind && (WORD_AT(frame+20u) || WORD_AT(frame+28u))) capture=0;
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
