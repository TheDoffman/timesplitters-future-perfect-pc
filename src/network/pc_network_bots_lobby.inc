/* Supported US lobby capacity/summary hooks. Keep the original human session
 * transport: its reliable settings packet already carries the admitted roster. */
#ifdef _WIN32
#define BOT_LOBBY_WORD(a) (*(uint32_t *)((uintptr_t)(a)+g_xbox_mem_offset))
static unsigned pc_network_bots_lobby_drawing;

static int pc_network_bots_lobby_network(void)
{
    unsigned mode=BOT_LOBBY_WORD(0x0078AD2Cu);
    return BOT_LOBBY_WORD(0x0076B108u) && mode>=11u && mode<=13u && pc_network_enabled();
}

static unsigned pc_network_bots_lobby_limit(void)
{
    unsigned mode=BOT_LOBBY_WORD(0x0078AD2Cu),map=BOT_LOBBY_WORD(0x0078AD4Cu);
    unsigned column=mode==11u?2u:mode==13u?3u:0u,limit;
    /* Same map/profile lookup as US sub_002BEA00, including mode 12's
       default column. Sixteen is the physical array bound, not a mode rule. */
    if(map>=17u) return 0;
    limit=BOT_LOBBY_WORD(0x004D239Cu+(map*15u+column)*4u);
    return limit<PC_NETWORK_CHARACTER_LIMIT?limit:PC_NETWORK_CHARACTER_LIMIT;
}

static PcNetworkBotPopulation pc_network_bots_lobby_prepare(int configure)
{
    uint32_t active[16];unsigned i;
    PcNetworkBotPopulation p;
    uint8_t *config=(uint8_t *)((uintptr_t)0x0078AD58u+g_xbox_mem_offset);
    const PcNetworkBot *roster=(const PcNetworkBot *)((uintptr_t)0x0075FD90u+g_xbox_mem_offset);
    int enabled=pc_network_bots_option("TSFP_NETWORK_BOTS",0u,1u)==1u;
    for(i=0;i<16u;i++) active[i]=BOT_LOBBY_WORD(0x0077728Cu+i*0x124u);
    p=pc_network_bots_population(active,roster,pc_network_bots_lobby_limit());
    if(!enabled) p.selected=p.bots=p.waiting=0;
    if(configure && pc_network_bots_lobby_network() && BOT_LOBBY_WORD(0x0076B140u)==1u)
        pc_network_bots_configure(config,PC_NETWORK_MATCH_BYTES,active,roster,
            enabled,1,1,BOT_LOBBY_WORD(0x0078AD2Cu),p.limit);
    return p;
}
#endif

static void pc_network_bots_guest_lobby_send(void (*original)(void))
{
#ifdef _WIN32
    if(pc_network_bots_lobby_network() && BOT_LOBBY_WORD(0x0076B1A4u)==11u)
        pc_network_bots_lobby_prepare(1);
#endif
    original();
}

static void pc_network_bots_guest_capacity(void (*original)(void))
{
    original();
#ifdef _WIN32
    if(pc_network_bots_lobby_network()) {
        unsigned limit=pc_network_bots_lobby_limit();
        if(limit<g_eax) g_eax=limit;
    }
#endif
}

static void pc_network_bots_guest_start(void (*original)(void))
{
#ifdef _WIN32
    if(pc_network_bots_lobby_network() && BOT_LOBBY_WORD(0x0076B140u)==1u) {
        PcNetworkBotPopulation p=pc_network_bots_lobby_prepare(0);
        /* A mode change must not launch a roster larger than that mode can
           allocate. Do not disconnect humans or renumber their network IDs. */
        if(!p.limit || p.human_slots>p.limit) { g_esp+=4u;return; }
    }
#endif
    original();
}

static void pc_network_bots_guest_lobby_draw(void (*original)(void))
{
#ifdef _WIN32
    unsigned previous=pc_network_bots_lobby_drawing;
    pc_network_bots_lobby_drawing=pc_network_bots_lobby_network();
    original();
    pc_network_bots_lobby_drawing=previous;
#else
    original();
#endif
}

static void pc_network_bots_guest_lobby_text(void (*original)(void))
{
#ifdef _WIN32
    uint32_t caller=BOT_LOBBY_WORD(g_esp),destination=BOT_LOBBY_WORD(g_esp+4u);
#endif
    original();
#ifdef _WIN32
    /* Only the current lobby's population line, before the stock renderer
       measures/centres it. The server browser and all other strings are intact.
       This local buffer in sub_001972C0 has 112 bytes below the saved
       registers; use at most 64. */
    if(pc_network_bots_lobby_drawing && caller==0x00197707u) {
        PcNetworkBotPopulation p=pc_network_bots_lobby_prepare(0);
        char *out=(char *)((uintptr_t)destination+g_xbox_mem_offset);
        if(BOT_LOBBY_WORD(0x0076B140u)!=1u) {
            const uint8_t *config=(const uint8_t *)((uintptr_t)0x0078AD58u+g_xbox_mem_offset);
            p.bots=config[2]<=PC_NETWORK_BOT_LIMIT?config[2]:0u;p.waiting=0;
        }
        if(p.human_slots>p.limit || !p.limit)
            snprintf(out,64,"Too many players - limit %u",p.limit);
        else if(p.waiting)
            snprintf(out,64,"%u players + %u bots / %u (%u waiting)",p.players,p.bots,p.limit,p.waiting);
        else
            snprintf(out,64,"%u players + %u bots / %u",p.players,p.bots,p.limit);
    }
#endif
}
