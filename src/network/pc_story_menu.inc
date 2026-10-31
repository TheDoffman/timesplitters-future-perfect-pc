#ifdef _WIN32
static void story_rewards_profiles_capture(void);
#endif
/* Shared Story entry, inserted at the US main menu's original row, before
 * the guest copies its callback. Local opens the original descriptor from
 * sub_002D0A60's choice-zero branch. No generated code or XBE is modified. */
/* Four checksum-verified unused INT3 bytes at 123C9..123CC. */
/* Separate from Unlock Content callbacks at 1237B..1237E and keyboard padding. */
#define STORY_OPEN_CB 0x123c9u
#define STORY_BUILD_CB 0x123cau
#define STORY_SELECT_CB 0x123cbu
#define STORY_BACK_CB 0x123ccu
static uint32_t story_storage,story_parent,story_previous;
static int story_online_unavailable;
#define STORY_TITLE (story_storage+16u)
#define STORY_LOCAL (story_storage+48u)
#define STORY_ONLINE (story_storage+80u)
#define STORY_UNAVAILABLE (story_storage+112u)

static int pc_story_menu_button(void (*original)(void))
{
    const uint32_t frame=g_esp;
    /* Original main builder sub_002D0BE0, first row: ID zero, inherited
       main-menu callback. All other builders and rows retain their callbacks. */
    if(WORD_AT(frame)!=0x002D0C8Eu || WORD_AT(frame+8u)!=0u || WORD_AT(frame+20u)!=0u)
        return 0;
    WORD_AT(frame+20u)=STORY_OPEN_CB;
    original();
    WORD_AT(frame+20u)=0u;
    return 1;
}

static int story_prepare(void)
{
    if(!story_storage) {
        story_storage=xbox_HeapAlloc(144u,4u);
        if(!story_storage) return 0;
        memset((void *)((uintptr_t)story_storage+g_xbox_mem_offset),0,144u);
        WORD_AT(story_storage)=STORY_BUILD_CB;
        memcpy((void *)((uintptr_t)STORY_TITLE+g_xbox_mem_offset),"Story",6);
        memcpy((void *)((uintptr_t)STORY_LOCAL+g_xbox_mem_offset),"Local",6);
        memcpy((void *)((uintptr_t)STORY_ONLINE+g_xbox_mem_offset),"Online",7);
        memcpy((void *)((uintptr_t)STORY_UNAVAILABLE+g_xbox_mem_offset),"Online unavailable on this host",31);
    }
    return 1;
}

/* Reused menu records need their own builder/layout context after a different
 * screen has occupied the same root. This applies to both main and chooser. */
static void story_rebuild_child(uint32_t parent,uint32_t descriptor)
{
    if(guest_writable(parent,0x174u)) {
        const uint32_t menu=WORD_AT(parent+0x170u);
        if(guest_writable(menu,0x44u) && WORD_AT(menu+4u)==descriptor)
            guest_call(sub_00078980,1,menu);
    }
}

static void story_open(void)
{
    GuestRegisters r;const uint32_t source=WORD_AT(g_esp+4u);
    save_registers(&r);
    if(guest_writable(source,0x3cu) && WORD_AT(source+0x38u)==0u && story_prepare()) {
        story_parent=WORD_AT(source+0x2cu);story_previous=WORD_AT(source+4u);
        story_online_unavailable=0;
        guest_call(sub_00079B70,2,story_parent,story_storage);
        story_rebuild_child(story_parent,story_storage);
        fprintf(stderr,"[PC-STORY] Local/Online chooser opened\n");
    }
    restore_registers(&r);g_esp+=4u;
}

static void story_build(void)
{
    GuestRegisters r;const uint32_t menu=WORD_AT(g_esp+4u);
    save_registers(&r);
    guest_call(sub_00075B70,5,0xa000u,story_online_unavailable?STORY_UNAVAILABLE:STORY_TITLE,
               0x425c0000u,0u,STORY_BACK_CB);
    guest_call(sub_00071B10,4,1u,0x40000000u,1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    guest_call(sub_00079D60_gen,6,STORY_LOCAL,0u,0u,0u,STORY_SELECT_CB,0u);
    guest_call(sub_00079D60_gen,6,STORY_ONLINE,1u,0u,0u,STORY_SELECT_CB,0u);
    guest_call(sub_000771D0,1,menu);
    guest_call(sub_000770E0,2,menu,0u);
    restore_registers(&r);g_esp+=4u;
}

static void story_main_open(void)
{
    guest_call(sub_00079B70,2,story_parent,story_previous);
    /* The stock factory can reuse the original main record after the Online
       chooser has been rebuilt. Rebuild its rows in its own layout context. */
    story_rebuild_child(story_parent,story_previous);
}
static void story_back(void)
{
    GuestRegisters r;save_registers(&r);
    story_main_open();
    restore_registers(&r);g_esp+=4u;
}

static void story_select(void)
{
    GuestRegisters r;const uint32_t menu=WORD_AT(g_esp+4u);
    save_registers(&r);
    if(guest_writable(menu,0x190u)) {
        const uint32_t selected=WORD_AT(menu+0x18cu);
        if(selected==0u) {
#ifdef _WIN32
            pc_online_disconnect();
#endif
            guest_call(sub_00079B70,2,story_parent,0x005238E0u);
            fprintf(stderr,"[PC-STORY] Original local Story route selected\n");
        } else if(selected==1u) {
#ifdef _WIN32
            if(pc_online_connect_mode(PC_ONLINE_STORY)) {
                /* Joining already reorders active profile slots. Retain their
                   persistent identities before opening the network browser. */
                story_rewards_profiles_capture();
                /* Shared System Link browser/lobby is the current transport
                   entry. Story lobby presentation/loading require their own
                   mode-aware hooks; this is not gameplay validation. */
                guest_call(sub_001A0A60,0);
                fprintf(stderr,"[PC-STORY] Story bridge ready; shared lobby opened\n");
            }
#else
            /* Existing native transport gap MAC-013. Keep the local route
               available and provide feedback without entering an offline lobby. */
            story_online_unavailable=1;
            guest_call(sub_00078980,1,menu);
#endif
        }
    }
    restore_registers(&r);g_esp+=4u;
}

#ifdef _WIN32
extern void sub_001A4350(void),sub_001A8CE0(void),sub_001A8EE0(void),sub_001CC360(void);
static uint32_t story_return_destination;
/* Run stock Back cleanup, replacing only its final multiplayer destination.
 * The menu factory is already wrapped for Story rewards. Scope this redirect
 * across the original callback, then close this peer's Online companion. */
static int story_online_return_menu(void (*original)(void))
{
    const uint32_t frame=g_esp;
    if(!story_return_destination || pc_online_mode()!=PC_ONLINE_STORY ||
       WORD_AT(0x79094cu)!=101u || !guest_writable(frame,16u) ||
       WORD_AT(frame+8u)!=story_return_destination ||
       !guest_writable(story_storage,144u) || WORD_AT(story_storage)!=STORY_BUILD_CB)
        return 0;
    const uint32_t destination=WORD_AT(frame+8u);
    /* The skipped network-type builder normally invokes this full guest reset.
       Retire its sockets/peers while the companion still exists, then close
       the native transport after Back. Closing native sockets alone leaves
       stale guest handles which cannot advertise a recreated match. */
    GuestRegisters saved;save_registers(&saved);
    guest_call(sub_001CC360,0);
    restore_registers(&saved);
    WORD_AT(frame+8u)=story_storage;
    original();
    WORD_AT(frame+8u)=destination;
    return 1;
}
static void story_online_return(void (*original)(void),uint32_t destination)
{
    const uint32_t previous=story_return_destination;
    story_return_destination=destination;
    original();
    story_return_destination=previous;
    pc_online_disconnect();
}
static void story_browser_back(void){story_online_return(sub_001A4350,0x4e9974u);}
static void story_host_lobby_back(void){story_online_return(sub_001A8CE0,0x4e9404u);}
static void story_guest_lobby_back(void){story_online_return(sub_001A8EE0,0x4e93f4u);}
static void (*story_online_back_lookup(uint32_t address))(void)
{
    if(pc_online_mode()!=PC_ONLINE_STORY || WORD_AT(0x79094cu)!=101u ||
       !guest_writable(story_storage,144u) || WORD_AT(story_storage)!=STORY_BUILD_CB)
        return NULL;
    switch(address) {
    case 0x1a4350u:return story_browser_back;
    case 0x1a8ce0u:return story_host_lobby_back;
    case 0x1a8ee0u:return story_guest_lobby_back;
    }
    return NULL;
}
#endif

/* Native lobby loss reports8D through1AF950 and opens the multiplayer network
 * selector directly, without running the guest's Back callback. Keep this
 * redirect limited to that failure on an owned Story guest lobby. */
void pc_story_network_lobby_lost(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    if(pc_online_mode()==PC_ONLINE_STORY && WORD_AT(0x79094cu)==101u &&
       guest_writable(frame,12u) &&
       (WORD_AT(frame)==0x1cc4d9u || WORD_AT(frame)==0x1cc552u) &&
       !WORD_AT(frame+4u) && WORD_AT(frame+8u)==0x8du &&
       guest_writable(story_storage,144u) && WORD_AT(story_storage)==STORY_BUILD_CB &&
       story_previous && guest_writable(story_parent,0x174u)) {
        const uint32_t lobby=WORD_AT(story_parent+0x170u);
        if(guest_writable(lobby,0x30u) && WORD_AT(lobby+4u)==0x4e99d4u &&
           WORD_AT(lobby+0x2cu)==story_parent) {
            GuestRegisters saved;save_registers(&saved);
            /* Preserve the reset normally owned by the network selector, while
               sockets/companion are still available. Then open and rebuild main
               in its own layout context, just like the chooser's Back route. */
            guest_call(sub_001CC360,0);
            pc_online_disconnect();
            story_main_open();
            restore_registers(&saved);g_esp+=4u;
            fprintf(stderr,"[PC-STORY-LOBBY-LOST] guest returned to main\n");
            return;
        }
    }
#endif
    original();
}
