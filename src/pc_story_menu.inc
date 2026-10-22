/* Shared Story entry, inserted at the US main menu's original row, before
 * the guest copies its callback. Local opens the original descriptor from
 * sub_002D0A60's choice-zero branch. No generated code or XBE is modified. */
/* Four checksum-verified unused INT3 bytes following the Quit callbacks. */
#define STORY_OPEN_CB 0x1201cu
#define STORY_BUILD_CB 0x1201du
#define STORY_SELECT_CB 0x1201eu
#define STORY_BACK_CB 0x1201fu
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

static void story_open(void)
{
    GuestRegisters r;const uint32_t source=WORD_AT(g_esp+4u);
    save_registers(&r);
    if(guest_writable(source,0x3cu) && WORD_AT(source+0x38u)==0u && story_prepare()) {
        story_parent=WORD_AT(source+0x2cu);story_previous=WORD_AT(source+4u);
        story_online_unavailable=0;
        guest_call(sub_00079B70,2,story_parent,story_storage);
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

static void story_back(void)
{
    GuestRegisters r;save_registers(&r);
    guest_call(sub_00079B70,2,story_parent,story_previous);
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
