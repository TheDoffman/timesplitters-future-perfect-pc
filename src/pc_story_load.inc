/* Story load adapter for typed Online Story sessions. The original network start
 * handshake still transports the match config; this does not synchronize Story
 * NPCs/scripts or certify that Story player creation accepts network ownership. */
#ifdef _WIN32
extern void sub_002D2C70(void);
extern void sub_000D50E0(void);
extern void sub_000CF060_gen(void);
static int story_context_start_pending;
static int story_network_session(void)
{
    return pc_online_mode()==PC_ONLINE_STORY;
}
#endif

int pc_story_network_prepare(uint32_t address)
{
#ifdef _WIN32
    uint8_t *config;unsigned mission=13u,role=WORD_AT(0x0076B140u);
    const unsigned phase=WORD_AT(0x0076B1A4u);
    if(!story_network_session() || !WORD_AT(0x0076B108u) ||
       (role!=1u && role!=2u) || (phase!=10u && phase!=11u) ||
       !guest_writable(address,0xa0u)) return 0;
    config=(uint8_t *)((uintptr_t)address+g_xbox_mem_offset);
    /* The stock network builder leaves byte0 at map capacity (16 in the paired
       receipt), not admitted humans. Derive the occupied extent on the host;
       preserve holes in global IDs and the original local ownership count. */
    if(getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<16u)
            fprintf(stderr,"[PC-STORY-LOAD] boundary address=%08X role=%u phase=%u slots=%u local=%u\n",
                address,role,WORD_AT(0x0076B1A4u),config[0],config[1]);
    }
    if(role==1u && phase==11u && address==0x0078AD58u) {
        const uint32_t row=0x004D1AC0u+story_lobby_mission*0x44u;
        int16_t scene;unsigned humans=0;
        const unsigned locals=config[1];uint8_t network_header[0x14];
        memcpy(network_header,config,sizeof network_header);
        for(unsigned i=0;i<16u;i++)
            if(WORD_AT(0x0077728Cu+i*0x124u)&1u) humans=i+1u;
        if(!humans || humans>4u || !config[1] || config[1]>humans) return 0;
        if(story_lobby_mission>=13u || story_lobby_difficulty>=3u) return 0;
        memcpy(&scene,(const void *)((uintptr_t)(row+6u)+g_xbox_mem_offset),2u);
        if(scene<0 || scene>=100) return 0;
        /* Reuse the original no-argument Story config factory (the verified
           start call at2D392E). A partial Arcade config is not a Story config.
           Scope only frontend selection; transport and active mode stay live.
           The result is serialized once and received intact by the guest. */
        {
            GuestRegisters saved;save_registers(&saved);
            const uint32_t mode=WORD_AT(0x0078AD2Cu);
            const uint32_t mission=WORD_AT(0x0078AD48u),difficulty=WORD_AT(0x0078AD54u);
            WORD_AT(0x0078AD2Cu)=4u;
            WORD_AT(0x0078AD48u)=story_lobby_mission;
            *((uint8_t *)((uintptr_t)0x0078AD54u+g_xbox_mem_offset))=(uint8_t)story_lobby_difficulty;
            /* Mode4 does not initialize the rule/variant: its factory reads
               the existing selection. Original Local Story (cinematic and
               actual scene2) supplies rule10/variant2. Seed these BEFORE the
               factory so rule-dependent fields cannot inherit Arcade policy. */
            config[0x55]=10u;config[0x56]=2u;
            guest_call(sub_002D2C70,0);
            WORD_AT(0x0078AD2Cu)=mode;
            WORD_AT(0x0078AD48u)=mission;WORD_AT(0x0078AD54u)=difficulty;
            restore_registers(&saved);
        }
        /* Keep the original network header, including the local-view count.
           Never reconstruct or compact its other ownership/header bytes. */
        memcpy(config,network_header,sizeof network_header);
        config[0]=(uint8_t)humans;
        config[1]=(uint8_t)locals;
        config[2]=0; /* no Arcade bots in this experiment */
        config[0x54]=(uint8_t)scene;
        config[0x57]=(uint8_t)story_lobby_difficulty;
        /* Original Local baseline: scene2, flags0x80008, Cortez12. These
           observed flags are a hypothesis for co-op, to be checked live. */
        { const uint32_t flags=0x00080008u;memcpy(config+0x58u,&flags,4u); }
        for(unsigned i=0;i<config[0];i++) {
            config[0x14u+i]=12u;config[0x24u+i]=0u;
        }
    }
    if(!config[0] || config[0]>4u || !config[1] || config[1]>config[0]) return 0;
    { uint32_t flags;memcpy(&flags,config+0x58u,4u);
      if(config[2] || config[0x55]!=10u || config[0x56]!=2u ||
         config[0x57]>=3u || flags!=0x00080008u) return 0; }
    for(unsigned i=0;i<13u;i++) {
        int16_t scene;
        memcpy(&scene,(const void *)((uintptr_t)(0x004D1AC6u+i*0x44u)+g_xbox_mem_offset),2u);
        if(scene==(int16_t)config[0x54]) {mission=i;break;}
    }
    if(mission>=13u) return 0;
    /* Retain selection separately from transient cinematic config. The guest
       derives it from the host's existing reliable start packet, not its UI. */
    story_lobby_mission=mission;story_lobby_difficulty=config[0x57];
    if(getenv("TSFP_STORY_TRACE"))
        fprintf(stderr,"[PC-STORY-LOAD] config role=%u scene=%u mission=%u difficulty=%u players=%u local=%u\n",
            role,config[0x54],mission,config[0x57],config[0],config[1]);
    return 1;
#else
    (void)address;return 0;
#endif
}

void pc_story_network_activate(void)
{
#ifdef _WIN32
    if(!story_network_session()) return;
    /* Each network instance owns ONE local Story view. Test original mode4
       rather than assuming local co-op mode14 accepts one local controller.
       Keep frontend mode13 and the original network role/phase/ownership. */
    WORD_AT(0x0078AD20u)=4u;
    story_context_start_pending=1;
    WORD_AT(0x0078AD48u)=story_lobby_mission;
    *((uint8_t *)((uintptr_t)0x0078AD54u+g_xbox_mem_offset))=(uint8_t)story_lobby_difficulty;
    if(getenv("TSFP_STORY_TRACE"))
        fprintf(stderr,"[PC-STORY-LOAD] pending load switched to one-view Story mode4\n");
#endif
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
    if(story_context_start_pending && story_network_session() &&
       WORD_AT(0x0076B108u) && WORD_AT(0x0076B1A4u)==15u &&
       WORD_AT(0x007DE30Cu)==7u && !WORD_AT(0x007DE45Cu)) {
        const uint32_t count=WORD_AT(0x00790950u),pool=WORD_AT(0x007B0C48u);
        const uint8_t *config=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
        if(count && count<=4u && count==config[0] && config[1]==1u &&
           config[0x55]==10u && config[0x56]==2u && pool &&
           guest_writable(pool,(count-1u)*0x1584u+0x118u)) {
            /* A remote spawn packet can activate one context before this
               peer reaches its first evaluator. Initialize each still-inactive
               context independently, retaining D50E0's original ownership
               predicate. One attempt per pending load; never revive later deaths
               or restart an already active remote context. */
            story_context_start_pending=0;
            unsigned attempted=0;
            GuestRegisters saved;save_registers(&saved);
            for(unsigned i=0;i<count;i++)
                if(WORD_AT(pool+i*0x1584u+0x114u)==16u) {
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
