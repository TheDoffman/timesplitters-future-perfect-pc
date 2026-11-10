/* Extend authored Story inventory awards to remote humans. Mission one's
 * handoff retains its ammo+grant pair; later missions use the native script
 * grant. Each guest invokes the original inventory API on its owning peer. */
#ifdef _WIN32
extern void sub_001BA2B0(void),sub_001BFCA0(void),sub_001BA130(void),sub_001BA220(void);
typedef struct StoryLoadout {
    uint32_t magic,match,serial,mask,weapon,magazines,equipped;
    uint32_t ammo_kind[2],ammo_amount[2];
} StoryLoadout;
#define STORY_LOADOUT_MAGIC 0x33444c53u
_Static_assert(sizeof(StoryLoadout)==44,"Story loadout wire layout");
enum { STORY_LOADOUT_ITEMS=72u };
/* Keep the two mission-one handoff slots stable; other authored weapons have
   separate replay records, so reordered awards cannot replace one another. */
static unsigned story_loadout_item(uint32_t weapon)
{ return weapon==25u?0u:weapon==32u?1u:(unsigned)weapon+2u; }
static StoryLoadout story_loadout_host[STORY_LOADOUT_ITEMS],story_loadout_pending[STORY_LOADOUT_ITEMS];
static uint32_t story_loadout_actor,story_loadout_weapon,story_loadout_magazines;
static uint32_t story_loadout_authored_actor,story_loadout_authored_kind[2],story_loadout_authored_amount[2];
static uint32_t story_loadout_tick,story_loadout_applied_match[4][STORY_LOADOUT_ITEMS],story_loadout_applied_serial[4][STORY_LOADOUT_ITEMS];
static uint32_t story_loadout_wait_selection[4][STORY_LOADOUT_ITEMS];
static int story_loadout_sent;
static void story_loadout_reset(void)
{
    memset(&story_loadout_host,0,sizeof story_loadout_host);
    story_loadout_actor=0;story_loadout_sent=0;
    story_loadout_authored_actor=0;
    memset(story_loadout_authored_kind,0,sizeof story_loadout_authored_kind);
    memset(story_loadout_authored_amount,0,sizeof story_loadout_authored_amount);
    memset(story_loadout_applied_match,0,sizeof story_loadout_applied_match);
    memset(story_loadout_applied_serial,0,sizeof story_loadout_applied_serial);
    memset(story_loadout_wait_selection,0,sizeof story_loadout_wait_selection);
    if(story_pending_stage==0u) memset(&story_loadout_pending,0,sizeof story_loadout_pending);
}
static int story_loadout_session(void)
{
    return story_npc_session() && story_npc_humans()>1u && story_campaign_gameplay();
}
static int story_loadout_source(uint32_t actor)
{
    const uint32_t pool=WORD_AT(0x007356D8u);
    return story_loadout_session() && WORD_AT(0x0076B140u)==1u &&
        pool && actor==pool && guest_writable(actor,0xea0u) && WORD_AT(actor+8u)==0u;
}
static int story_loadout_valid(const StoryLoadout *s)
{
    return s->magic==STORY_LOADOUT_MAGIC && s->serial &&
        ((s->match>>8)&255u)==story_campaign_scene(story_lobby_mission) && ((s->match>>16)&255u)==10u &&
        s->mask && !(s->mask&~0xeu) &&
        (story_lobby_mission==0u ? ((s->weapon==25u || s->weapon==32u) && s->magazines && s->equipped<=1u) :
            (s->weapon>1u && s->weapon<70u && s->equipped<=2u)) && s->magazines<=64u &&
        s->ammo_kind[0]<32u && s->ammo_kind[1]<32u && s->ammo_amount[0]<=32767u && s->ammo_amount[1]<=32767u &&
        (story_lobby_mission!=0u || (!s->ammo_amount[0] && !s->ammo_amount[1]));
}
static void story_loadout_trace(const char *event,uint32_t caller,uint32_t actor,uint32_t weapon,uint32_t magazines)
{
    if(!story_campaign_scene_active() || !guest_writable(actor,0xea0u) || !getenv("TSFP_STORY_TRACE")) return;
    const unsigned slot=WORD_AT(actor+8u);const uint32_t pool=WORD_AT(0x007356d8u);
    static unsigned observations;
    if(slot<4u && pool && actor==pool+slot*0xea0u && observations++<96u)
        fprintf(stderr,"[PC-STORY-INVENTORY] event=%s role=%u scene=%u phase=%u slot=%u caller=%08X weapon=%u magazines=%u equipped=%u\n",
            event,WORD_AT(0x76b140u),WORD_AT(0x79094cu),WORD_AT(0x76b1a4u),slot,caller,weapon,magazines,WORD_AT(actor+0x94u));
}
static void story_loadout_rounds(uint32_t frame)
{
    if(!guest_writable(frame,16u) || story_lobby_mission==0u) return;
    const uint32_t caller=WORD_AT(frame),actor=WORD_AT(frame+4u),kind=WORD_AT(frame+8u),amount=WORD_AT(frame+12u);
    if((caller!=0x000c03a5u && caller!=0x000c03bdu) || !story_loadout_source(actor) || kind>=32u || amount>32767u) return;
    const unsigned item=caller==0x000c03a5u?0u:1u;
    if(!item || story_loadout_authored_actor!=actor) {
        memset(story_loadout_authored_kind,0,sizeof story_loadout_authored_kind);
        memset(story_loadout_authored_amount,0,sizeof story_loadout_authored_amount);
    }
    story_loadout_authored_actor=actor;story_loadout_authored_kind[item]=kind;story_loadout_authored_amount[item]=amount;
    story_loadout_trace("rounds",caller,actor,kind,amount);
}
#endif

void pc_story_network_loadout_ammo(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const int valid=guest_writable(frame,16u);
    const uint32_t caller=valid?WORD_AT(frame):0u,actor=valid?WORD_AT(frame+4u):0u;
    const uint32_t weapon=valid?WORD_AT(frame+8u):0u,magazines=valid?WORD_AT(frame+12u):0u;
    if(valid) story_loadout_trace("ammo",caller,actor,weapon,magazines);
    if(valid && story_loadout_session() && (weapon==25u || weapon==32u) &&
       guest_writable(actor,0xea0u) && getenv("TSFP_STORY_TRACE")) {
        const unsigned slot=WORD_AT(actor+8u);const uint32_t pool=WORD_AT(0x007356D8u);
        static unsigned observations;
        if(slot<story_npc_humans() && pool && actor==pool+slot*0xea0u && observations++<16u)
            fprintf(stderr,"[PC-STORY-AMMO] role=%u slot=%u caller=%08X weapon=%u magazines=%u\n",
                WORD_AT(0x0076B140u),slot,caller,weapon,magazines);
    }
    const int capture=story_lobby_mission==0u && ((weapon==25u && (caller==0x00218967u || caller==0x0021B592u)) ||
         (weapon==32u && caller==0x00218A54u)) &&
        story_loadout_source(actor) && magazines && magazines<=64u;
#endif
    original();
#ifdef _WIN32
    if(capture) {story_loadout_actor=actor;story_loadout_weapon=weapon;story_loadout_magazines=magazines;}
#endif
}
void pc_story_network_loadout_grant(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const int valid=guest_writable(frame,12u);
    const uint32_t caller=valid?WORD_AT(frame):0u,actor=valid?WORD_AT(frame+4u):0u;
    const uint32_t weapon=valid?WORD_AT(frame+8u):0u;
    if(valid) story_loadout_trace("grant",caller,actor,weapon,0u);
    const int authored=story_lobby_mission!=0u && caller==0x000c0407u && weapon>1u && weapon<70u && story_loadout_source(actor);
    const int capture=story_lobby_mission==0u && ((weapon==25u && (caller==0x0021896Fu || caller==0x0021B59Au)) ||
         (weapon==32u && caller==0x00218A5Cu)) &&
        story_loadout_source(actor) && actor==story_loadout_actor && weapon==story_loadout_weapon;
#endif
    original();
#ifdef _WIN32
    if((capture || authored) && *(const uint8_t *)((uintptr_t)(actor+0x54cu+weapon)+g_xbox_mem_offset)) {
        const unsigned item=story_loadout_item(weapon);
        if(!story_loadout_host[item].serial) {
            story_loadout_host[item]=(StoryLoadout){STORY_LOADOUT_MAGIC,story_npc_match(),item+1u,
                ((1u<<story_npc_humans())-1u)&~1u,weapon,authored?0u:story_loadout_magazines,authored?2u:0u,{0,0},{0,0}};
            if(authored && story_loadout_authored_actor==actor) {
                memcpy(story_loadout_host[item].ammo_kind,story_loadout_authored_kind,sizeof story_loadout_authored_kind);
                memcpy(story_loadout_host[item].ammo_amount,story_loadout_authored_amount,sizeof story_loadout_authored_amount);
            }
            story_loadout_sent=0;
            if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,
                "[PC-STORY-LOADOUT] authored host grant weapon=%u magazines=%u recipients=%X\n",
                weapon,story_loadout_magazines,story_loadout_host[item].mask);
        }
        story_loadout_actor=0;
        if(authored) story_loadout_authored_actor=0;
    }
#endif
}

#ifdef _WIN32
static int story_loadout_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || !guest_writable(payload,4u) || WORD_AT(payload)!=STORY_LOADOUT_MAGIC) return 0;
    if(WORD_AT(0x0076B140u)==2u && guest_writable(peer,4u) &&
       WORD_AT(peer)==WORD_AT(0x00774040u) && length>=sizeof(StoryLoadout) &&
       length<=sizeof(StoryLoadout)+4u && guest_writable(payload,sizeof(StoryLoadout))) {
        StoryLoadout s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
        if(story_loadout_valid(&s)) story_loadout_pending[story_loadout_item(s.weapon)]=s;
    }
    return 1;
}
static void story_loadout_frame(void)
{
    if(!story_loadout_session()) return;
    if(WORD_AT(0x0076B140u)==1u) {
        int present=0;
        const uint32_t actor=WORD_AT(0x007356d8u);
        for(unsigned item=0;item<STORY_LOADOUT_ITEMS;item++) {
            StoryLoadout *s=&story_loadout_host[item];
            /* The authored pistol grant precedes the first-person handoff's
               equip action. Guests must not draw the weapon at the grant. */
            if(s->serial) present=1;
            if(s->serial && s->equipped!=1u && guest_writable(actor,0xea0u) && WORD_AT(actor+0x94u)==s->weapon) {
                s->equipped=1u;++s->serial;story_loadout_sent=0;
            }
        }
        if(!present) return;
        const uint32_t frame=g_esp,tick=WORD_AT(0x007DE318u),node=WORD_AT(0x00774028u);
        if((story_loadout_sent && (uint32_t)(tick-story_loadout_tick)<60u) || frame<768u ||
           !guest_writable(frame-768u,768u) || !guest_writable(node,0x35u)) return;
        const uint32_t buffer=frame-128u;
        GuestRegisters saved;save_registers(&saved);g_esp=frame-512u;
        for(unsigned item=0;item<STORY_LOADOUT_ITEMS;item++) if(story_loadout_host[item].serial &&
            story_loadout_host[item].match==story_npc_match()) {
            memset((void *)((uintptr_t)buffer+g_xbox_mem_offset),0,sizeof(StoryLoadout)+4u);
            memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&story_loadout_host[item],sizeof(StoryLoadout));
            guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof(StoryLoadout)+4u,1u);
        }
        restore_registers(&saved);story_loadout_sent=1;story_loadout_tick=tick;return;
    }
    if(WORD_AT(0x0076B140u)!=2u) return;
    const uint32_t contexts=WORD_AT(0x007B0C48u),pool=WORD_AT(0x007356D8u);
    if(!contexts || !pool) return;
    for(unsigned item=0;item<STORY_LOADOUT_ITEMS;item++) {
        const StoryLoadout *s=&story_loadout_pending[item];
        if(!story_loadout_valid(s) || !s->equipped || s->match!=story_npc_match() ||
           s->mask!=(((1u<<story_npc_humans())-1u)&~1u)) continue;
        for(unsigned slot=1u;slot<story_npc_humans();slot++) {
            const uint32_t context=contexts+slot*0x1584u,actor=pool+slot*0xea0u;
            if(!(s->mask&(1u<<slot)) || !guest_writable(context,0x1584u) || !guest_writable(actor,0xea0u) ||
               WORD_AT(context)!=slot || WORD_AT(context+4u)>=4u || WORD_AT(context+0x114u)!=2u ||
               (story_loadout_applied_match[slot][item]==s->match && story_loadout_applied_serial[slot][item]>=s->serial)) continue;
            const uint32_t entity=WORD_AT(actor);
            if(!guest_writable(entity,0x25cu) || WORD_AT(entity+0x7cu)!=actor ||
               WORD_AT(entity+0x150u)!=context || WORD_AT(context+0x14u)!=entity) continue;
            story_loadout_applied_match[slot][item]=s->match;story_loadout_applied_serial[slot][item]=s->serial;
            /* Already-owned weapons must not refill ammo or override the user's selection. */
            if(!*(const uint8_t *)((uintptr_t)(actor+0x54cu+s->weapon)+g_xbox_mem_offset)) {
                const uint32_t selected=WORD_AT(actor+0x94u);
                GuestRegisters saved;save_registers(&saved);
                for(unsigned mode=0;mode<2u;mode++) if(s->ammo_amount[mode])
                    guest_call(sub_001BA220,3,actor,s->ammo_kind[mode],s->ammo_amount[mode]);
                if(s->magazines) guest_call(sub_001BA2B0,3,actor,s->weapon,s->magazines);
                guest_call(sub_001BFCA0,2,actor,s->weapon);
                if(s->equipped==1u) guest_call(sub_001BA130,2,actor,s->weapon);
                else if(selected<70u) {
                    guest_call(sub_001BA130,2,actor,selected);
                    story_loadout_wait_selection[slot][item]=selected+1u;
                }
                restore_registers(&saved);
            } else if(s->equipped==1u && story_loadout_wait_selection[slot][item]) {
                /* The grant may arrive before the host's later equip action.
                   Complete that transition once without awarding ammo again,
                   and preserve any selection the guest made in between. */
                const uint32_t previous=story_loadout_wait_selection[slot][item]-1u;
                if(WORD_AT(actor+0x94u)==previous) {
                    GuestRegisters saved;save_registers(&saved);
                    guest_call(sub_001BA130,2,actor,s->weapon);
                    restore_registers(&saved);
                }
                story_loadout_wait_selection[slot][item]=0;
            }
            if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,
                "[PC-STORY-LOADOUT] applied owned slot=%u weapon=%u magazines=%u\n",slot,s->weapon,s->magazines);
        }
    }
}
#endif

/* Remote animation poses must not execute these authored inventory side effects
 * again. The host retains both callbacks and sends each award reliably. */
void pc_story_network_handoff(void (*original)(void))
{
#ifdef _WIN32
    if(story_lobby_mission==0u && story_loadout_session() && WORD_AT(0x0076B140u)==2u) {g_esp+=4u;return;}
#endif
    original();
}
