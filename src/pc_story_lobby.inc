/* Story lobby controls. Keep the title's network session/roster alive
 * while using its Story mission and difficulty selectors. Original callbacks
 * are called directly; indirect dispatch alone is overridden. */
#ifdef _WIN32
extern void sub_001A8D30(void),sub_001A8F30(void),sub_002CB090(void);
extern void sub_00197890(void),sub_002CA9F0(void),sub_00359EB0(void);
extern void sub_00198E20(void),sub_0003D7B0(void),sub_00040F00(void),sub_00041DC0(void),sub_0003F8D0(void),sub_0003FCF0(void);
#define STORY_BODY_CB 0x12020u
static uint32_t story_body_menu,story_body_rect;
static uint32_t story_lobby_parent,story_lobby_descriptor,story_lobby_captions;
static uint32_t story_difficulty_descriptor;
static unsigned story_lobby_options;
/* Keep Story lobby selections in host-owned state while the original network
 * lobby stays active. Apply guest Story fields only on a coordinated launch. */
static uint32_t story_lobby_mission,story_lobby_difficulty;

static int story_lobby_host(void)
{
    return pc_online_mode()==PC_ONLINE_STORY && WORD_AT(0x0076B140u)==1u &&
        WORD_AT(0x0076B1A4u)==11u;
}

static int story_lobby_member(void)
{
    return story_lobby_host() || (pc_online_mode()==PC_ONLINE_STORY &&
        WORD_AT(0x0076B140u)==2u && WORD_AT(0x0076B1A4u)==12u);
}
unsigned pc_story_network_player_limit(void)
{ return pc_online_mode()==PC_ONLINE_STORY?4u:0u; }

/* The existing reliable lobby packet carries this same config. Keep its
   capacity/ownership bytes intact; only publish the host's Story selection. */
void pc_story_network_lobby_config(void)
{
    if(!story_lobby_host() || !WORD_AT(0x0076B108u) ||
       story_lobby_mission>=13u || story_lobby_difficulty>=3u) return;
    uint8_t *config=(uint8_t *)((uintptr_t)0x0078AD58u+g_xbox_mem_offset);
    int16_t scene;
    memcpy(&scene,(const void *)((uintptr_t)(0x004D1AC6u+story_lobby_mission*0x44u)+g_xbox_mem_offset),2u);
    if(scene<0 || scene>=100) return;
    config[0x54]=(uint8_t)scene;config[0x55]=10u;config[0x56]=2u;
    config[0x57]=(uint8_t)story_lobby_difficulty;config[2]=0;
    for(unsigned i=0;i<16u;i++)
        if(WORD_AT(0x0077728Cu+i*0x124u)&1u) config[0x14u+i]=12u;
}

static void story_lobby_received_selection(void)
{
    if(story_lobby_host()) return;
    const uint8_t *config=(const uint8_t *)((uintptr_t)0x0078AD58u+g_xbox_mem_offset);
    if(config[0x55]!=10u || config[0x56]!=2u || config[0x57]>=3u) return;
    for(unsigned i=0;i<13u;i++) {
        int16_t scene;
        memcpy(&scene,(const void *)((uintptr_t)(0x004D1AC6u+i*0x44u)+g_xbox_mem_offset),2u);
        if(scene==config[0x54]) {story_lobby_mission=i;story_lobby_difficulty=config[0x57];break;}
    }
}

static int pc_story_lobby_button(void (*original)(void))
{
    const uint32_t frame=g_esp,callback=WORD_AT(frame+20u),caller=WORD_AT(frame);
    if(!story_lobby_member()) return 0;
    if(((caller==0x001A8DFCu && WORD_AT(frame+8u)==0x145u) ||
        (caller==0x001A8FFCu && WORD_AT(frame+8u)==0x14bu)) &&
       WORD_AT(frame+24u)==0x00197890u) {
        WORD_AT(frame+24u)=STORY_BUILD_CB;
        original();WORD_AT(frame+24u)=0x00197890u;
        return 1;
    }
    if(((caller==0x001A8E21u && WORD_AT(frame+8u)==0x147u) ||
        (caller==0x001A9021u && WORD_AT(frame+8u)==0x14du)) &&
       WORD_AT(frame+24u)==0x00198E20u) {
        WORD_AT(frame+24u)=STORY_BODY_CB;
        original();WORD_AT(frame+24u)=0x00198E20u;return 1;
    }
    if(!story_lobby_host() || !story_lobby_options) return 0;
    const int level=story_lobby_options==1u && callback==0x002CAFE0u && caller==0x002CB149u;
    const int difficulty=story_lobby_options==2u && callback==0x002CAD80u &&
        (caller==0x002D6383u || caller==0x002D63C9u || caller==0x002D640Fu);
    if(!level && !difficulty) return 0;
    if(getenv("TSFP_STORY_TRACE"))
        fprintf(stderr,"[PC-STORY-SELECT] row callback %08X -> %08X stage=%u\n",
            callback,STORY_SELECT_CB,story_lobby_options);
    /* Profile-guided indirect calls may call known original targets directly,
       bypassing manual lookup. A synthetic target forces the generic path.
       Reuse the existing Story selection callback, scoped by menu descriptor. */
    WORD_AT(frame+20u)=STORY_SELECT_CB;
    original();
    WORD_AT(frame+20u)=callback;
    return 1;
}

static void story_lobby_header(void)
{
    const uint32_t item=WORD_AT(g_esp+8u);
    if(!guest_writable(item,0x18u) ||
       (WORD_AT(item+0x14u)!=0x145u && WORD_AT(item+0x14u)!=0x14bu) ||
       story_lobby_mission>=13u) { sub_00197890();return; }
    /* Reuse the original Story row's year, name and thumbnail renderer. The
       verified draw ABI is (menu,item,rectangle); its mission is the row ID. */
    const uint32_t id=WORD_AT(item+0x14u);
    WORD_AT(item+0x14u)=story_lobby_mission;
    const uint32_t menu=WORD_AT(g_esp+4u);
    const int scoped=guest_writable(menu,8u);
    const uint32_t descriptor=scoped?WORD_AT(menu+4u):0u;
    /* The original briefing heading takes opacity directly from its menu;
       list rows instead fade from item+34, absent for this static heading. */
    if(scoped) WORD_AT(menu+4u)=0x005238C0u;
    sub_002CA9F0();
    if(scoped) WORD_AT(menu+4u)=descriptor;
    WORD_AT(item+0x14u)=id;
}

static void story_lobby_body(void)
{
    const uint32_t old_menu=story_body_menu,old_rect=story_body_rect;
    story_body_menu=WORD_AT(g_esp+4u);story_body_rect=WORD_AT(g_esp+12u);
    sub_00198E20(); /* original roster, icons, clipping and population */
    story_body_menu=old_menu;story_body_rect=old_rect;
}
#endif

void pc_story_lobby_details(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    if(story_lobby_member() && WORD_AT(frame)==0x00199650u &&
       guest_writable(story_body_menu,0xf8u) && guest_writable(story_body_rect,16u) &&
       story_lobby_mission<13u) {
        float bounds[4];memcpy(bounds,(const void *)((uintptr_t)story_body_rect+g_xbox_mem_offset),16u);
        if(bounds[2]>bounds[0]+32.f && bounds[3]>bounds[1]+32.f &&
           frame>=512u && guest_writable(frame-256u,256u)) {
            GuestRegisters r;save_registers(&r);g_esp=frame-256u;
            const uint32_t font=guest_call(sub_0003FA10,1,3u);
            const uint32_t text=guest_call(sub_0003D7B0,1,WORD_AT(0x004D1AD0u+story_lobby_mission*0x44u));
            float x=bounds[0]+8.f,y=bounds[1]+(bounds[3]-bounds[1])*.68f,width=bounds[2]-bounds[0]-16.f;
            uint32_t xb,yb,wb;memcpy(&xb,&x,4);memcpy(&yb,&y,4);memcpy(&wb,&width,4);
            const uint32_t old_x=WORD_AT(0x007E7270u),old_y=WORD_AT(0x007E7280u);
            float sx,sy;memcpy(&sx,&old_x,4);memcpy(&sy,&old_y,4);
            uint32_t wrapped=0;float scale=1.f;
            for(unsigned attempt=0;attempt<9u;attempt++) {
                const float nx=sx*scale,ny=sy*scale;
                memcpy(&WORD_AT(0x007E7270u),&nx,4);memcpy(&WORD_AT(0x007E7280u),&ny,4);
                guest_call(sub_0003F8D0,0);
                wrapped=guest_call(sub_00040F00,2,text,wb);
                guest_call(sub_0003FCF0,4,frame-16u,frame-12u,wrapped,0u);
                float height;memcpy(&height,&WORD_AT(frame-12u),4);
                if(height>=0.f && height<=bounds[3]-y-8.f) break;
                scale*=.85f;
            }
            const uint32_t color=guest_call(sub_00154FE0,2,0x7f7f7f7fu,WORD_AT(story_body_menu+0xf4u));
            guest_call(sub_00041DC0,4,xb,yb,wrapped,color);
            WORD_AT(0x007E7270u)=old_x;WORD_AT(0x007E7280u)=old_y;
            guest_call(sub_0003F8D0,0);guest_call(sub_0003FA10,1,font);
            restore_registers(&r);g_esp+=4u;return;
        }
    }
#endif
    original();
}
#ifdef _WIN32

static void story_lobby_footer(uint32_t menu)
{
    if(!story_lobby_captions) {
        story_lobby_captions=xbox_HeapAlloc(64u,4u);
        if(!story_lobby_captions) return;
        memset((void *)((uintptr_t)story_lobby_captions+g_xbox_mem_offset),0,64u);
        memcpy((void *)((uintptr_t)story_lobby_captions+g_xbox_mem_offset),"Back",5);
        memcpy((void *)((uintptr_t)story_lobby_captions+32u+g_xbox_mem_offset),"Player Progress",16);
    }
    if(story_lobby_host()) {
        WORD_AT(menu+0x14cu)=story_lobby_captions;
        WORD_AT(menu+0x15cu)=story_lobby_captions+32u;
    } else {
        /* Footer records are (physical action,caption,...) at128h/stride10h.
           Compact B/X into the first two slots; no Ready/Select action. The
           original guest tick rewrites Ready each frame, so apply AFTER it. */
        WORD_AT(menu+0x128u)=1u;WORD_AT(menu+0x12cu)=story_lobby_captions;
        WORD_AT(menu+0x138u)=2u;WORD_AT(menu+0x13cu)=story_lobby_captions+32u;
        WORD_AT(menu+0x148u)=0xffffffffu;WORD_AT(menu+0x158u)=0xffffffffu;
        WORD_AT(menu+0x1bcu)=0u; /* original builder's A callback */
    }
}

static void story_lobby_run_tick(void (*original)(void),uint32_t descriptor)
{
    const uint32_t menu=WORD_AT(g_esp+4u);
    if(!story_lobby_member() || !guest_writable(menu,0x1c4u) ||
       WORD_AT(menu+4u)!=descriptor) { original();return; }
    story_lobby_parent=WORD_AT(menu+0x2cu);
    story_lobby_descriptor=WORD_AT(menu+4u);
    story_lobby_received_selection();
    if(!story_lobby_host()) {
        const uint32_t local=WORD_AT(0x00774028u);
        /* Original Ready uses359EB0(0,1) when the local node's2FAh byte is0.
           Use its reliable setter, never toggle a ready guest or write flags. */
        if(guest_writable(local,0x300u) &&
           !*((const uint8_t *)((uintptr_t)(local+0x2fau)+g_xbox_mem_offset))) {
            GuestRegisters r;save_registers(&r);
            guest_call(sub_00359EB0,2,0u,1u);restore_registers(&r);
        }
        WORD_AT(menu+0x1bcu)=0u;
    }
    /* sub_000427B0 consumes raw network button presses. Prevent the original
       tick from opening Arcade Options or Select Character, retaining every
       other input and the original network/update behavior. */
    const uint32_t actions=WORD_AT(0x006B94D8u)&0x50u;
    WORD_AT(0x006B94D8u)&=~0x50u;
    original();
    WORD_AT(0x006B94D8u)=(WORD_AT(0x006B94D8u)&~0x50u)|actions;
    if(!story_lobby_member() || !guest_writable(menu,0x1c4u) ||
       WORD_AT(menu+4u)!=story_lobby_descriptor) return;
    story_lobby_footer(menu);
    if(actions) {
        GuestRegisters r;save_registers(&r);
        if((actions&0x40u) && story_lobby_host()) {
            story_lobby_options=1u;
            guest_call(sub_00079B70,2,story_lobby_parent,0x005238E0u);
        } else if(actions&0x10u) guest_call(sub_00079B70,2,story_lobby_parent,0x00523940u);
        restore_registers(&r);
    }
}
static void story_lobby_tick(void) { story_lobby_run_tick(sub_001A8D30,0x004E99C4u); }
static void story_lobby_guest_tick(void) { story_lobby_run_tick(sub_001A8F30,0x004E99D4u); }

static void story_lobby_selector(void (*original)(void))
{
    const uint32_t mode=WORD_AT(0x0078AD2Cu);
    const uint32_t mission=WORD_AT(0x0078AD48u),difficulty=WORD_AT(0x0078AD54u);
    WORD_AT(0x0078AD2Cu)=4u;
    WORD_AT(0x0078AD48u)=story_lobby_mission;
    WORD_AT(0x0078AD54u)=story_lobby_difficulty;
    original();
    WORD_AT(0x0078AD2Cu)=mode;
    WORD_AT(0x0078AD48u)=mission;WORD_AT(0x0078AD54u)=difficulty;
}
static void story_lobby_levels(void) { story_lobby_selector(sub_002CB090); }
static void story_lobby_difficulties(void)
{
    GuestRegisters r;const uint32_t menu=WORD_AT(g_esp+4u);
    save_registers(&r);
    /* Build this dialog independently of the local Story builder's profile
       and frontend globals. All actions remain within the network menu parent. */
    guest_call(sub_00075B70,5,0xa000u,story_difficulty_descriptor+16u,
        0x42280000u,0u,STORY_BACK_CB);
    guest_call(sub_00071B10,4,1u,0x40000000u,1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    for(uint32_t i=0;i<3u;i++)
        guest_call(sub_00079D60_gen,6,story_difficulty_descriptor+48u+i*32u,
            i,0u,0u,STORY_SELECT_CB,0u);
    guest_call(sub_000771D0,1,menu);
    const uint32_t selected=story_lobby_difficulty;
    guest_call(sub_000770E0,2,menu,selected<3u?selected:0u);
    if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,"[PC-STORY-SELECT] custom difficulty built mission=%u\n",story_lobby_mission);
    restore_registers(&r);g_esp+=4u;
}
static void story_lobby_difficulty_back(void)
{
    GuestRegisters r;save_registers(&r);
    story_lobby_options=1u;
    guest_call(sub_00079B70,2,story_lobby_parent,0x005238E0u);
    restore_registers(&r);g_esp+=4u;
}
static void story_lobby_level_select(void)
{
    const uint32_t menu=WORD_AT(g_esp+4u);
    if(!guest_writable(menu,0x190u) || WORD_AT(menu+0x18cu)>=13u) { g_esp+=4u;return; }
    GuestRegisters r;save_registers(&r);
    if(!story_difficulty_descriptor) {
        story_difficulty_descriptor=xbox_HeapAlloc(144u,4u);
        if(!story_difficulty_descriptor) { restore_registers(&r);g_esp+=4u;return; }
        memset((void *)((uintptr_t)story_difficulty_descriptor+g_xbox_mem_offset),0,144u);
        WORD_AT(story_difficulty_descriptor)=STORY_BUILD_CB;
        memcpy((void *)((uintptr_t)story_difficulty_descriptor+16u+g_xbox_mem_offset),"Select Difficulty",18);
        memcpy((void *)((uintptr_t)story_difficulty_descriptor+48u+g_xbox_mem_offset),"Easy",5);
        memcpy((void *)((uintptr_t)story_difficulty_descriptor+80u+g_xbox_mem_offset),"Normal",7);
        memcpy((void *)((uintptr_t)story_difficulty_descriptor+112u+g_xbox_mem_offset),"Hard",5);
    }
    /* The original local selector changes frontend/load state and obtains its
       parent through the local frontend global. Keep this network menu's
       captured parent and explicitly open difficulty, without scheduling load. */
    story_lobby_mission=WORD_AT(menu+0x18cu);
    if(getenv("TSFP_STORY_TRACE"))
        fprintf(stderr,"[PC-STORY-SELECT] mission=%u difficulty descriptor=%08X\n",
            WORD_AT(menu+0x18cu),story_difficulty_descriptor);
    story_lobby_options=2u;
    guest_call(sub_00079B70,2,story_lobby_parent,story_difficulty_descriptor);
    restore_registers(&r);g_esp+=4u;
}
static void story_lobby_difficulty_select(void)
{
    GuestRegisters r;const uint32_t menu=WORD_AT(g_esp+4u);
    save_registers(&r);
    if(guest_writable(menu,0x190u) && WORD_AT(menu+0x18cu)<3u) {
        /* The original select launches a local game. Store only the host's
           Story choice; GO remains a separate coordinated start operation. */
        story_lobby_difficulty=WORD_AT(menu+0x18cu);
        if(getenv("TSFP_STORY_TRACE")) fprintf(stderr,"[PC-STORY-SELECT] confirmed mission=%u difficulty=%u\n",
            story_lobby_mission,story_lobby_difficulty);
        story_lobby_options=0u;
        guest_call(sub_00079B70,2,story_lobby_parent,story_lobby_descriptor);
    }
    restore_registers(&r);g_esp+=4u;
}
static void (*story_lobby_lookup(uint32_t address))(void)
{
    if(!story_lobby_member()) return NULL;
    if(address==STORY_BODY_CB) return story_lobby_body;
    if(address==0x001A8F30u && !story_lobby_host()) return story_lobby_guest_tick;
    if(address==STORY_BUILD_CB && guest_writable(WORD_AT(g_esp+4u),8u)) {
        const uint32_t descriptor=WORD_AT(WORD_AT(g_esp+4u)+4u);
        if(descriptor==0x004E99C4u || descriptor==0x004E99D4u) return story_lobby_header;
    }
    if(!story_lobby_host()) return NULL;
    switch(address) {
    case STORY_BACK_CB:
        if(story_lobby_options==2u && story_difficulty_descriptor &&
           guest_writable(WORD_AT(g_esp+4u),8u) &&
           WORD_AT(WORD_AT(g_esp+4u)+4u)==story_difficulty_descriptor)
            return story_lobby_difficulty_back;
        break;
    case STORY_BUILD_CB:
        if(guest_writable(WORD_AT(g_esp+4u),8u) &&
           WORD_AT(WORD_AT(g_esp+4u)+4u)==0x004E99C4u)
            return story_lobby_header;
        if(story_lobby_options==2u && story_difficulty_descriptor &&
           guest_writable(WORD_AT(g_esp+4u),8u) &&
           WORD_AT(WORD_AT(g_esp+4u)+4u)==story_difficulty_descriptor)
            return story_lobby_difficulties;
        break;
    case STORY_SELECT_CB:
        if(story_lobby_options && guest_writable(WORD_AT(g_esp+4u),8u)) {
            const uint32_t descriptor=WORD_AT(WORD_AT(g_esp+4u)+4u);
            if(story_lobby_options==1u && descriptor==0x005238E0u) return story_lobby_level_select;
            if(story_lobby_options==2u && descriptor==story_difficulty_descriptor) return story_lobby_difficulty_select;
        }
        break;
    case 0x001A8D30u:return story_lobby_tick;
    case 0x002CB090u:if(story_lobby_options) return story_lobby_levels;break;
    case 0x002CAFE0u:if(story_lobby_options) return story_lobby_level_select;break;
    case 0x002CAD80u:if(story_lobby_options==2u) return story_lobby_difficulty_select;break;
    }
    return NULL;
}
#endif
