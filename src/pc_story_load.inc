/* Story load adapter for typed Online Story sessions. The original network start
 * handshake still transports the match config; this does not synchronize Story
 * NPCs/scripts or certify that Story player creation accepts network ownership. */
#ifdef _WIN32
extern void sub_00065940(void);
extern void sub_00352640(void),sub_00352650(void),sub_00356490(void);
extern void sub_000D50E0(void);
extern void sub_000CF060_gen(void),sub_0009A420_gen(void);
static uint32_t story_extra_pad_storage;
extern void sub_0025EE60(void);
static int story_context_start_pending;
static unsigned story_pending_stage;
static void story_npc_reset(void);
static void story_npc_pending_frame(void);
static void story_loadout_reset(void);
static void story_loadout_frame(void);
static void story_human_teams_frame(void);
static int story_loadout_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length);
static int story_network_session(void)
{
    return pc_online_mode()==PC_ONLINE_STORY;
}
#endif

int pc_story_network_prepare(uint32_t address)
{
#ifdef _WIN32
    const unsigned role=WORD_AT(0x0076B140u),phase=WORD_AT(0x0076B1A4u);
    const int advance=role==1u && (phase==10u || phase==14u || phase==15u) && guest_writable(g_esp,4u) &&
        WORD_AT(g_esp)==0x00065CDCu;
    /* A changed-generation class3B packet invokes the original GO callback
       before its receiver changes phase15 to10. This is the guest pending-copy
       boundary, not another local GO or a serializer call. */
    const int received_intro_end=role==2u && phase==15u && address==0x0078AD58u &&
        (WORD_AT(0x007DE458u)&0x20000000u) && guest_writable(g_esp,4u) &&
        WORD_AT(g_esp)==0x002D398Du;
    if(!story_network_session() || !WORD_AT(0x0076B108u) ||
       (role!=1u && role!=2u) || (phase!=10u && phase!=11u && !advance && !received_intro_end) ||
       !guest_writable(address,0xa0u)) return 0;
    uint8_t *config=(uint8_t *)((uintptr_t)address+g_xbox_mem_offset);
    if(role==1u && phase==11u && address==0x0078AD58u) {
        unsigned humans=0;uint8_t header[0x14];
        memcpy(header,config,sizeof header);
        for(unsigned i=0;i<16u;i++) if(WORD_AT(0x0077728Cu+i*0x124u)&1u) humans=i+1u;
        if(!humans || humans>4u || !config[1] || config[1]>humans ||
           story_lobby_mission>=13u || story_lobby_difficulty>=3u) return 0;
        GuestRegisters saved;save_registers(&saved);
        const uint32_t mode=WORD_AT(0x0078AD2Cu),mission=WORD_AT(0x0078AD48u),difficulty=WORD_AT(0x0078AD54u);
        WORD_AT(0x0078AD2Cu)=4u;WORD_AT(0x0078AD48u)=story_lobby_mission;
        WORD_AT(0x0078AD54u)=story_lobby_difficulty;WORD_AT(0x0078AD50u)=0u;
        /* Original stage0 prepares the selected intro and its playback state.
           Missions without an intro fall through to original stage1. */
        guest_call(sub_00065940,1,address);
        WORD_AT(0x0078AD2Cu)=mode;WORD_AT(0x0078AD48u)=mission;WORD_AT(0x0078AD54u)=difficulty;
        restore_registers(&saved);
        memcpy(config,header,sizeof header);config[0]=(uint8_t)humans;config[2]=0;
        /* Keep the selected gameplay difficulty through the transient intro. */
        config[0x57]=(uint8_t)story_lobby_difficulty;
        for(unsigned i=0;i<humans;i++) {config[0x14u+i]=12u;config[0x24u+i]=0u;}
    }
    if(advance) {
        const uint8_t *active=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
        if(!active[0] || active[0]>4u || active[1]!=1u) return 0;
        /* The original movie-end builder creates a Local config. Retain the
           joined network ownership header before its reliable start handshake. */
        memcpy(config,active,0x14u);config[2]=0;
        config[0x57]=(uint8_t)story_lobby_difficulty;
        for(unsigned i=0;i<config[0];i++) {config[0x14u+i]=12u;config[0x24u+i]=0u;}
    }
    uint32_t flags;memcpy(&flags,config+0x58u,4u);
    if(!config[0] || config[0]>4u || !config[1] || config[1]>config[0] || config[2] ||
       config[0x55]!=10u || config[0x56]!=2u || config[0x57]>=3u ||
       (flags!=0x00080008u && flags!=0x20000000u)) return 0;
    unsigned selected=13u;
    for(unsigned i=0;i<13u;i++) {
        const uint32_t row=0x004D1AC0u+i*0x44u;
        if(flags==0x20000000u) {
            const uint32_t movie=WORD_AT(row+0x20u);
            const uint32_t movie_scene=0x0051FC30u+movie*36u;
            if(movie<256u && movie==config[0x9bu] && guest_writable(movie_scene,1u) &&
               *(const uint8_t *)((uintptr_t)movie_scene+g_xbox_mem_offset)==config[0x54u]) {selected=i;break;}
        } else {
            int16_t scene;memcpy(&scene,(const void *)((uintptr_t)(row+6u)+g_xbox_mem_offset),2u);
            if(scene==(int16_t)config[0x54u]) {selected=i;break;}
        }
    }
    if(selected>=13u) return 0;
    story_lobby_mission=selected;story_lobby_difficulty=config[0x57u];
    story_pending_stage=flags==0x20000000u?0u:1u;
    if(advance) {
        GuestRegisters saved;save_registers(&saved);
        /* Same generation increment and reliable config sender as original GO.
           The nested serializer is not this movie-end callsite, so cannot
           recursively initiate another transition. */
        unsigned generation=(guest_call(sub_00352640,0)+1u)&255u;
        if(!generation) generation=1u;
        guest_call(sub_00352650,1,generation);
        guest_call(sub_00356490,1,address);
        restore_registers(&saved);
    }
    if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,
        "[PC-STORY-LOAD] config role=%u scene=%u mission=%u difficulty=%u players=%u local=%u stage=%u advance=%u\n",
        role,config[0x54],selected,config[0x57],config[0],config[1],story_pending_stage,advance);
    return 1;
#else
    (void)address;return 0;
#endif
}

void pc_story_network_activate(void)
{
#ifdef _WIN32
    if(!story_network_session()) return;
    story_npc_reset();
    story_loadout_reset();
    /* One full local view per network instance; keep the frontend network mode. */
    WORD_AT(0x0078AD20u)=4u;story_context_start_pending=1;
    WORD_AT(0x0078AD48u)=story_lobby_mission;WORD_AT(0x0078AD50u)=story_pending_stage;
    *((uint8_t *)((uintptr_t)0x0078AD54u+g_xbox_mem_offset))=(uint8_t)story_lobby_difficulty;
#endif
}

/* Original6B100 filters authored scene records before objects/scripts/starts
 * are instantiated. Its two-player-only mask has no four-player variant, and
 * bit31 excludes records from ordinary network matches. A Story session uses
 * the authored co-op records while retaining the original difficulty filter. */
void pc_story_network_asset(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const int valid=guest_writable(frame,8u);
    const uint32_t flags=valid?WORD_AT(frame+4u):0u,players=WORD_AT(0x00790950u);
    const uint32_t caller=valid?WORD_AT(frame):0u;
    uint32_t authored=flags;
    const int adapt=story_network_session() && WORD_AT(0x0076B108u) &&
       WORD_AT(0x0079094Cu)<100u && WORD_AT(0x007DE458u)==0x00080008u &&
       ((const uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset))[0]==10u &&
       valid && players && players<=4u;
    if(adapt) {
        authored=flags&~0x80000000u;
        if(players>2u && (authored&0x00400000u)) authored&=~0x00600000u;
        WORD_AT(frame+4u)=authored;
    }
#endif
    original();
#ifdef _WIN32
    if(adapt) WORD_AT(frame+4u)=flags;
    if(story_network_session() && story_pending_stage==1u && valid &&
       (flags&0x80600000u) && getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<24u) fprintf(stderr,
            "[PC-STORY-ASSET] role=%u caller=%08X scene=%u phase=%u count=%u active=%08X input=%08X authored=%08X adapt=%u result=%u\n",
            WORD_AT(0x0076B140u),caller,WORD_AT(0x0079094Cu),WORD_AT(0x0076B1A4u),
            players,WORD_AT(0x007DE458u),flags,authored,(unsigned)adapt,g_eax);
    }
#endif
}

/* Observe the authored model cache, character-list bootstrap and original
 * script/network spawn request independently. Never bypass the original gate. */
void pc_story_network_npc_trace(void (*original)(void),uint32_t address)
{
#ifdef _WIN32
    static unsigned observations[4];
    const unsigned slot=address==0x000D73B0u?0u:address==0x0015C560u?1u:
        address==0x0015F8C0u?2u:3u;
    const int local_trace=getenv("TSFP_STORY_TRACE_LOCAL") && !WORD_AT(0x0076B108u) &&
        WORD_AT(0x0078AD20u)==4u &&
        ((const uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset))[0]==10u;
    if((story_network_session() || local_trace) && getenv("TSFP_STORY_TRACE") &&
       WORD_AT(0x0079094Cu)<100u && WORD_AT(0x007DE458u)==0x00080008u &&
       guest_writable(g_esp,8u) && observations[slot]<8u) {
        ++observations[slot];
        const uint32_t caller=WORD_AT(g_esp),argument=WORD_AT(g_esp+4u);
        fprintf(stderr,"[PC-STORY-NPC] enter function=%08X caller=%08X argument=%08X role=%u phase=%u tick=%u replay=%u characters=%u allocation=%u\n",
            address,caller,argument,WORD_AT(0x0076B140u),WORD_AT(0x0076B1A4u),
            WORD_AT(0x007DE318u),WORD_AT(0x0076B168u),WORD_AT(0x0074C37Cu),WORD_AT(0x007A3500u));
        if(address==0x0015F8C0u && guest_writable(g_esp,128u)) {
            fprintf(stderr,"[PC-STORY-NPC] return-candidates");
            for(unsigned offset=0;offset<128u;offset+=4u) {
                const uint32_t value=WORD_AT(g_esp+offset);
                if(value>=0x00012000u && value<0x003E0000u) fprintf(stderr," %u:%08X",offset,value);
            }
            fputc('\n',stderr);
        }
        original();
        fprintf(stderr,"[PC-STORY-NPC] leave function=%08X result=%08X characters=%u allocation=%u\n",
            address,g_eax,WORD_AT(0x0074C37Cu),WORD_AT(0x007A3500u));
        return;
    }
#else
    (void)address;
#endif
    original();
}

/* D8E30 updates Story script actors before the first NPC spawn. Its ordinary
 * network activity predicate only accepts controller slots, so those scripts
 * never run. Accept an existing non-player Story actor at that caller only.
 * D8E30's later358560 host-ownership gate remains responsible for simulation. */
int pc_story_network_actor_active(uint32_t caller,unsigned index,uint32_t entity)
{
#ifdef _WIN32
    if(caller!=0x000D8E5Du || !story_network_session() || !WORD_AT(0x0076B108u) ||
       WORD_AT(0x0076B1A4u)!=15u || WORD_AT(0x0079094Cu)>=100u ||
       WORD_AT(0x007DE458u)!=0x00080008u) return 0;
    const uint8_t *config=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
    const unsigned role=WORD_AT(0x0076B140u),humans=config[0];
    if((role!=1u && role!=2u) || !humans || humans>4u ||
       config[0x55]!=10u || index<humans || index>=32u) return 0;
    const uint32_t pool=WORD_AT(0x007356D8u),actor=pool+index*0xea0u;
    if(!pool || actor<pool || !guest_writable(actor,0xea0u)) return 0;
    /* D8E30 keeps its actual entity in EBX. A dormant Story script actor can
       have no active body yet; its script must run to create that body. */
    const uint32_t body=WORD_AT(actor);
    return entity && guest_writable(entity,0x154u) && WORD_AT(actor+8u)==index &&
        (!body || body==entity) && WORD_AT(entity+0x7cu)==actor && !WORD_AT(entity+0x150u);
#else
    (void)caller;(void)index;(void)entity;return 0;
#endif
}

/* Complete the original network-ready exchange without its Arcade staging-pad
 * body loop. Story uses authored starts through its original scripts and the
 * pending-context adapter; that loop overwrites them with random team pads.
 * Original357540 also rejects cinematic flags, scoped off for this call only. */
void pc_story_network_ready(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t flags=WORD_AT(0x007DE458u),players=WORD_AT(0x00790950u);
    const unsigned role=WORD_AT(0x0076B140u);
    if(story_network_session() && WORD_AT(0x0076B108u) && (role==1u || role==2u) &&
       guest_writable(WORD_AT(0x00774028u),0x35u) &&
       ((flags&0x20000000u) || (flags==0x00080008u && story_context_start_pending && story_pending_stage==1u)) &&
       players && players<=4u &&
       WORD_AT(0x0079094Cu)<100u) {
        WORD_AT(0x007DE458u)=flags&~0x20000000u;
        WORD_AT(0x00790950u)=0;
        original(); /* stock ready packet/peer state, with no actor-loop work */
        WORD_AT(0x00790950u)=players;WORD_AT(0x007DE458u)=flags;
        return;
    }
#endif
    original();
}

void pc_story_network_advance(void (*original)(void))
{
#ifdef _WIN32
    if(story_network_session() && WORD_AT(0x0076B108u) && (WORD_AT(0x0076B1A4u)==10u || WORD_AT(0x0076B1A4u)==14u || WORD_AT(0x0076B1A4u)==15u) &&
       WORD_AT(0x0076B140u)==2u && (WORD_AT(0x007DE458u)&0x20000000u)) {
        /* Only the host may advance the original intro's next-load builder.
           Guests retain the final movie state until its reliable start arrives. */
        g_esp+=4u;return;
    }
#endif
    original();
}

void pc_story_failure_trace(void (*original)(void),uint32_t address);
void pc_story_network_script_trace(void (*original)(void),uint32_t address)
{
#ifdef _WIN32
    static unsigned observations;
    if(story_network_session() && getenv("TSFP_STORY_TRACE") && observations<32u &&
       WORD_AT(0x0079094Cu)<100u && guest_writable(g_esp,8u)) {
        ++observations;
        const uint32_t caller=WORD_AT(g_esp),argument=WORD_AT(g_esp+4u);
        const uint32_t count=WORD_AT(0x00790950u),pool=WORD_AT(0x007B0C48u);
        fprintf(stderr,"[PC-STORY-SCRIPT] enter function=%08X caller=%08X argument=%08X role=%u phase=%u scene=%u tick=%u result=%08X current=%08X contexts=",
            address,caller,argument,WORD_AT(0x0076B140u),WORD_AT(0x0076B1A4u),
            WORD_AT(0x0079094Cu),WORD_AT(0x007DE318u),WORD_AT(0x007DE45Cu),WORD_AT(0x007B0C7Cu));
        for(unsigned i=0;i<count && i<4u;i++)
            fprintf(stderr,"%s%u",i?"/":"",pool && guest_writable(pool+i*0x1584u,0x118u)?WORD_AT(pool+i*0x1584u+0x114u):UINT32_MAX);
        fputc('\n',stderr);
        original();
        fprintf(stderr,"[PC-STORY-SCRIPT] leave function=%08X tick=%u result=%08X contexts=",
            address,WORD_AT(0x007DE318u),WORD_AT(0x007DE45Cu));
        for(unsigned i=0;i<count && i<4u;i++)
            fprintf(stderr,"%s%u",i?"/":"",pool && guest_writable(pool+i*0x1584u,0x118u)?WORD_AT(pool+i*0x1584u+0x114u):UINT32_MAX);
        fputc('\n',stderr);
        return;
    }
#else
    (void)address;
#endif
    original();
}

#ifdef _WIN32
static int story_opening_starts(void)
{
    const unsigned role=WORD_AT(0x0076B140u),count=WORD_AT(0x00790950u);
    return story_network_session() && WORD_AT(0x0076B108u) &&
        WORD_AT(0x0076B1A4u)==15u && (role==1u || role==2u) &&
        WORD_AT(0x0079094Cu)==2u && WORD_AT(0x007DE458u)==0x00080008u &&
        ((const uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset))[0]==10u &&
        count>=1u && count<=4u;
}
#endif

/* The stock spawn packet carries a signed16 pad index. These two positive
   indices are reserved only in the first Online Story scene; no level records
   or ordinary pad tables are modified. Every peer resolves them locally. */
void pc_story_network_start_record(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const unsigned pad=guest_writable(frame,8u)?WORD_AT(frame+4u):0u;
    if(story_opening_starts() && (pad==0x7f02u || pad==0x7f03u)) {
        GuestRegisters saved;save_registers(&saved);
        const uint32_t source=guest_call(sub_0009A420_gen,1,0x1f40u);
        if(!story_extra_pad_storage) story_extra_pad_storage=xbox_HeapAlloc(64u,4u);
        const uint32_t target=story_extra_pad_storage+(pad-0x7f02u)*32u;
        if(story_extra_pad_storage && guest_writable(source,32u) && guest_writable(target,32u)) {
            memcpy((void *)((uintptr_t)target+g_xbox_mem_offset),
                (const void *)((uintptr_t)source+g_xbox_mem_offset),32u);
            float x;memcpy(&x,(const void *)((uintptr_t)(target+12u)+g_xbox_mem_offset),4u);
            x+=pad==0x7f02u?-2.f:2.f;
            memcpy((void *)((uintptr_t)(target+12u)+g_xbox_mem_offset),&x,4u);
            restore_registers(&saved);g_eax=target;g_esp=frame+4u;return;
        }
        /* A missing template must never index the stock table with7f02/3. */
        restore_registers(&saved);g_eax=source;g_esp=frame+4u;return;
    }
#endif
    original();
}

/* Keep the original pad tag/index helpers consistent with the private records.
   They otherwise index the level table directly, bypassing9A420. */
void pc_story_network_start_tag(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const unsigned pad=guest_writable(frame,8u)?WORD_AT(frame+4u):0u;
    if(story_opening_starts() && (pad==0x7f02u || pad==0x7f03u)) {
        WORD_AT(frame+4u)=0x1f40u;original();WORD_AT(frame+4u)=pad;return;
    }
#endif
    original();
}
void pc_story_network_start_index(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const uint32_t record=guest_writable(frame,8u)?WORD_AT(frame+4u):0u;
    if(story_opening_starts() && story_extra_pad_storage &&
       (record==story_extra_pad_storage || record==story_extra_pad_storage+32u)) {
        g_eax=0x7f02u+(record-story_extra_pad_storage)/32u;g_esp=frame+4u;return;
    }
#endif
    original();
}

void pc_story_network_spawn_pad(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const uint32_t context=guest_writable(frame,8u)?WORD_AT(frame+4u):0u;
    const int trace_pad=story_network_session() && getenv("TSFP_STORY_TRACE") &&
        WORD_AT(0x0076B1A4u)==15u && context && guest_writable(context,0x1508u);
    const uint32_t cached_pad=trace_pad?WORD_AT(context+0x1504u):0u;
#endif
    original();
#ifdef _WIN32
    if(trace_pad) {
        static unsigned pad_observations;
        if(pad_observations<24u) {
            ++pad_observations;
            fprintf(stderr,"[PC-STORY-PAD-SOURCE] role=%u tick=%u slot=%u caller=%08X cached=%08X random=%u selected=%08X\n",
                WORD_AT(0x0076B140u),WORD_AT(0x007DE318u),WORD_AT(context),
                WORD_AT(frame),cached_pad,WORD_AT(0x006B988Cu),g_eax);
        }
    }
    if(story_opening_starts() && context && guest_writable(context,0x1508u)) {
        const uint32_t slot=WORD_AT(context),pool=WORD_AT(0x007B0C48u);
        if(slot<WORD_AT(0x00790950u) && pool && context==pool+slot*0x1584u) {
            const uint32_t checkpoint=guest_call(sub_0025EE60,1,0u),selected=g_eax;
            if(getenv("TSFP_STORY_TRACE")) {
                static unsigned observations;
                if(observations++<24u) fprintf(stderr,
                    "[PC-STORY-OPENING-PAD] role=%u slot=%u checkpoint=%u original=%08X\n",
                    WORD_AT(0x0076B140u),slot,checkpoint,selected);
            }
            if(!checkpoint) {
                /* The original checkpoint predicate is a read-only getter. Before
                   the first checkpoint, every peer must resolve the same authored
                   opening pads even when network setup selects a remote pad later.
                   Clearing a slot token on its first active body was too early. */
                if(slot<2u) {g_eax=slot?0x1f62u:0x1f40u;return;}
                if(!story_extra_pad_storage) story_extra_pad_storage=xbox_HeapAlloc(64u,4u);
                if(story_extra_pad_storage && guest_writable(story_extra_pad_storage,64u)) {
                    g_eax=0x7f00u+slot;return;
                }
            }
        }
    }
    /* Rule10 selects the Nth valid authored Story start. Maps authored for
       local co-op can exhaust that list for online slots2/3 and return -1.
       Reuse the original first-player lookup only after that explicit failure;
       do not change ownership, body construction or any valid network pad. */
    if(g_eax!=UINT32_MAX || !story_network_session() ||
       !WORD_AT(0x0076B108u) || WORD_AT(0x0076B1A4u)!=15u ||
       ((uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset))[0]!=10u ||
       !context || !guest_writable(context,0x1508u)) return;
    const uint32_t count=WORD_AT(0x00790950u),pool=WORD_AT(0x007B0C48u);
    const uint32_t slot=WORD_AT(context);
    if(count<3u || count>4u || slot<2u || slot>=count || !pool ||
       context!=pool+slot*0x1584u) return;
    WORD_AT(context)=0u;
    const uint32_t pad=guest_call(sub_000CF060_gen,1,context);
    WORD_AT(context)=slot;
    g_eax=pad;
    if(getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<8u)
            fprintf(stderr,"[PC-STORY-SPAWN-PAD] slot=%u missing authored start; original first-player lookup=%08X\n",slot,pad);
    }
#endif
}

void pc_story_network_frame(void (*original)(void))
{
#ifdef _WIN32
    if(story_context_start_pending && story_pending_stage==1u && story_network_session() &&
       WORD_AT(0x0076B108u) && WORD_AT(0x0076B1A4u)==15u &&
       WORD_AT(0x007DE30Cu)==7u && !WORD_AT(0x007DE45Cu)) {
        const uint32_t count=WORD_AT(0x00790950u),pool=WORD_AT(0x007B0C48u);
        const uint8_t *config=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
        if(count && count<=4u && count==config[0] && config[1]==1u &&
           config[0x55]==10u && config[0x56]==2u && pool &&
           guest_writable(pool,(count-1u)*0x1584u+0x1508u)) {
            /* A remote spawn packet can activate one context before this
               peer reaches its first evaluator. Initialize each still-inactive
               context independently, retaining D50E0's original ownership
               predicate. One attempt per pending load; never revive later deaths
               or restart an already active remote context. */
            story_context_start_pending=0;
            unsigned attempted=0;
            GuestRegisters saved;save_registers(&saved);
            for(unsigned i=0;i<count;i++)
                if(WORD_AT(pool+i*0x1584u+0x114u)==16u || WORD_AT(pool+i*0x1584u+0x114u)==4u) {
                    /* Network staging cached Arcade map pads before the Story
                       start. Let the original Story selector choose authored
                       starts for this first owned spawn, not a random map pad.
                       Active remote contexts and later spawns remain untouched. */
                    WORD_AT(pool+i*0x1584u+0x1504u)=0x7fffffffu;
                    guest_call(sub_000D50E0,2,pool+i*0x1584u,1u);
                    ++attempted;
                }
            restore_registers(&saved);
            if(getenv("TSFP_STORY_TRACE")) {
                fprintf(stderr,"[PC-STORY-START] original spawn attempted %u/%u contexts; states",attempted,count);
                for(unsigned i=0;i<count;i++) fprintf(stderr," %u",WORD_AT(pool+i*0x1584u+0x114u));
                fputc('\n',stderr);
            }
        }
    }
#endif
#ifdef _WIN32
    story_human_teams_frame();
    story_npc_pending_frame();
    story_loadout_frame();
#endif
    pc_story_failure_trace(original,0x000681C0u);
}

/* Trace the original failure boundary without altering its predicate, result,
   ABI or flags. Normal/native-fallback runs call the original body directly. */
void pc_story_failure_trace(void (*original)(void),uint32_t address)
{
#ifdef _WIN32
    static unsigned observations;
    static unsigned context_wait_observations;
    /* Startup can replace contexts after a Story script has suspended one.
       Observe those original calls with the independent script trace budget,
       rather than losing them to the earlier initialization messages. */
    if((address==0x000D2FD0u || address==0x000D3C50u ||
        address==0x000D2CE0u || address==0x000D50E0u) &&
       story_network_session() && getenv("TSFP_STORY_TRACE") &&
       WORD_AT(0x0076B1A4u)==15u && (WORD_AT(0x007DE45Cu)&0x02000000u)) {
        pc_story_network_script_trace(original,address);
        return;
    }
    if(address==0x000D50E0u) { original();return; }
    /* Four-player preparation consumes the generic initialization trace budget.
       Preserve a separate bounded observation of the later state-256 transition;
       the original setter and all of its side effects still execute unchanged. */
    if(address==0x000D2CE0u && context_wait_observations<8u &&
       story_network_session() && getenv("TSFP_STORY_TRACE") &&
       WORD_AT(0x0076B1A4u)==15u && guest_writable(g_esp,12u) &&
       WORD_AT(g_esp+8u)==256u) {
        const uint32_t context=WORD_AT(g_esp+4u),caller=WORD_AT(g_esp);
        if(context && guest_writable(context,0x11cu)) {
            ++context_wait_observations;
            const uint32_t before=WORD_AT(context+0x114u);
            fprintf(stderr,"[PC-STORY-CONTEXT-WAIT] caller=%08X context=%08X tick=%u state=%u requested=256 result=%08X\n",
                caller,context,WORD_AT(0x007DE318u),before,WORD_AT(0x007DE45Cu));
            original();
            fprintf(stderr,"[PC-STORY-CONTEXT-WAIT] applied state=%u result=%08X\n",
                WORD_AT(context+0x114u),WORD_AT(0x007DE45Cu));
            return;
        }
    }
    if(story_network_session() && getenv("TSFP_STORY_TRACE") && observations<16u &&
       WORD_AT(0x0079094Cu)<100u) {
        const uint32_t caller=guest_writable(g_esp,4u)?WORD_AT(g_esp):0u;
        const uint32_t state=WORD_AT(0x007DE30Cu),result=WORD_AT(0x007DE45Cu);
        const uint32_t players=WORD_AT(0x00790950u),pool=WORD_AT(0x007B0C48u);
        uint32_t states[4]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
        for(unsigned i=0;i<players && i<4u;i++) {
            const uint32_t field=pool+i*0x1584u+0x114u;
            if(pool && field>=pool && guest_writable(field,4u)) states[i]=WORD_AT(field);
        }
        const int direct=address!=0x000681C0u;
        if(direct) {
            ++observations;
            fprintf(stderr,"[PC-STORY-FAILURE] enter function=%08X caller=%08X tick=%u state=%u result=%08X reason=%u players=%u pool=%08X contexts=%08X/%08X/%08X/%08X\n",
                address,caller,WORD_AT(0x007DE318u),state,result,WORD_AT(0x006F11A4u),
                players,pool,states[0],states[1],states[2],states[3]);
        }
        original();
        if(WORD_AT(0x007DE30Cu)!=state || WORD_AT(0x007DE45Cu)!=result) {
            if(!direct) ++observations;
            fprintf(stderr,"[PC-STORY-FAILURE] leave function=%08X caller=%08X tick=%u state=%u->%u result=%08X->%08X reason=%u\n",
                address,caller,WORD_AT(0x007DE318u),state,WORD_AT(0x007DE30Cu),
                result,WORD_AT(0x007DE45Cu),WORD_AT(0x006F11A4u));
        }
        return;
    }
#else
    (void)address;
#endif
    original();
}

/* Arcade's network tick removes every character outside its player actor pool
 * from the active character list. Story NPCs belong to that list too. Keep the
 * scene's authored membership; deaths and script-driven removals still use the
 * original routine from their own callers. This does not replicate NPC state. */
/* Both initial D7570 and the later network pump can assign Arcade teams.
   Story uses team1 for every human, including remote bodies during loading. */
#ifdef _WIN32
extern void sub_000D7550(void);
static int story_human_team_scope(uint32_t actor)
{
    const uint32_t pool=WORD_AT(0x007356D8u),phase=WORD_AT(0x0076B1A4u);
    const unsigned humans=*(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
    const unsigned role=WORD_AT(0x0076B140u);
    return story_network_session() && WORD_AT(0x0076B108u) &&
        (phase==10u || phase==14u || phase==15u) && (role==1u || role==2u) &&
        WORD_AT(0x0079094Cu)<100u && WORD_AT(0x007DE458u)==0x00080008u &&
        *(const uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset)==10u &&
        humans && humans<=4u && pool && guest_writable(actor,0xea0u) &&
        WORD_AT(actor+8u)<humans && actor==pool+WORD_AT(actor+8u)*0xea0u;
}
#endif

#ifdef _WIN32
static void story_human_teams_frame(void)
{
    const uint32_t contexts=WORD_AT(0x007B0C48u),actors=WORD_AT(0x007356D8u);
    const unsigned count=WORD_AT(0x00790950u);
    if(!count || count>4u || !contexts || !actors ||
       count!=*(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset)) return;
    GuestRegisters saved;save_registers(&saved);
    for(unsigned slot=0;slot<count;slot++) {
        const uint32_t context=contexts+slot*0x1584u,actor=actors+slot*0xea0u;
        if(!guest_writable(context,0x118u) || !story_human_team_scope(actor)) continue;
        const uint32_t entity=WORD_AT(actor);
        if(!entity || !guest_writable(entity,0x154u) || WORD_AT(context)!=slot ||
           WORD_AT(context+0x14u)!=entity || WORD_AT(entity+0x7cu)!=actor ||
           WORD_AT(entity+0x150u)!=context || WORD_AT(actor+0x84u)==1u) continue;
        /* Remote construction can precede eligible Story initialization. Once
           its real body/context exists, apply the original shared-human team
           before NPC simulation; leave already-correct teams untouched. */
        guest_call(sub_000D7550,2,actor,1u);
    }
    restore_registers(&saved);
}
#endif

void pc_story_network_team(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const int valid=guest_writable(frame,12u);
    const uint32_t actor=valid?WORD_AT(frame+4u):0u;
    const int scope=valid && WORD_AT(frame)==0x0033ACDBu && story_network_session() &&
        story_human_team_scope(actor);
    const uint32_t saved=scope?WORD_AT(frame+8u):0u;
    if(scope) WORD_AT(frame+8u)=1u;
#endif
    original();
#ifdef _WIN32
    if(scope) WORD_AT(frame+8u)=saved;
#endif
}

void pc_story_network_character_remove(void (*original)(void))
{
#ifdef _WIN32
    if(story_network_session() && WORD_AT(0x0076B108u) &&
       (story_context_start_pending || (WORD_AT(0x0079094Cu)<100u &&
        ((const uint8_t *)((uintptr_t)0x007DE455u+g_xbox_mem_offset))[0]==10u)) &&
       guest_writable(g_esp,4u) && WORD_AT(g_esp)==0x00355EA4u) {
        /* The network pump also runs during loading. Waiting for phase15
           already allowed newly created Story characters to be pruned. */
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned observations;
            if(observations++<8u) fprintf(stderr,
                "[PC-STORY-ROSTER] retained character phase=%u scene=%u pending=%u count=%u\n",
                WORD_AT(0x0076B1A4u),WORD_AT(0x0079094Cu),story_context_start_pending,
                WORD_AT(0x0074C37Cu));
        }
        g_esp+=4u;return;
    }
#endif
    original();
}
#include "pc_story_npc.inc"
#include "pc_story_loadout.inc"
