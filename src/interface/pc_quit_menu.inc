/* Append only after the US menu builders finish their row setters, before
   the original layout pass. The original Quit returns to the frontend. */
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY) || defined(TSFP_QUIT_MENU_TEST)
#ifdef TSFP_NATIVE_DISPLAY
extern int tsfp_native_request_quit(void);
#endif
#define QUIT_OPEN_CB 0x12018u
#define QUIT_BUILD_CB 0x12019u
#define QUIT_SELECT_CB 0x1201au
#define QUIT_BACK_CB 0x1201bu
/* These four bytes are unused INT3 padding in the supported US XBE. */
static uint32_t quit_storage, quit_parent, quit_previous;
static int quit_requested;
#define QUIT_TEXT (quit_storage+16u)
#define QUIT_TITLE (quit_storage+48u)
#define QUIT_CANCEL (quit_storage+80u)
static int quit_prepare(void)
{
    if(!quit_storage) {
        quit_storage=xbox_HeapAlloc(112,4);
        if(!quit_storage) return 0;
        memset((void *)((uintptr_t)quit_storage+g_xbox_mem_offset),0,112);
        WORD_AT(quit_storage)=QUIT_BUILD_CB;
        memcpy((void *)((uintptr_t)QUIT_TEXT+g_xbox_mem_offset),"Quit to Desktop",16);
        memcpy((void *)((uintptr_t)QUIT_TITLE+g_xbox_mem_offset),"Quit to Desktop?",17);
        memcpy((void *)((uintptr_t)QUIT_CANCEL+g_xbox_mem_offset),"Cancel",7);
    }
    return 1;
}
static void quit_open(void)
{
    GuestRegisters r; uint32_t source=WORD_AT(g_esp+4);
    save_registers(&r);
    if(guest_writable(source,0x3c) && WORD_AT(source+0x38)==0 && quit_prepare()) {
        quit_parent=WORD_AT(source+0x2c); quit_previous=WORD_AT(source+4);
        quit_requested=0;
        guest_call(sub_00079B70,2,quit_parent,quit_storage);
        fprintf(stderr,"[PC-QUIT] Confirmation opened\n");
    }
    restore_registers(&r); g_esp+=4;
}
static void quit_build(void)
{
    GuestRegisters r; uint32_t menu=WORD_AT(g_esp+4);
    save_registers(&r);
    guest_call(sub_00075B70,5,0xa000u,QUIT_TITLE,0x425c0000u,0u,QUIT_BACK_CB);
    guest_call(sub_00071B10,4,1u,0x40000000u,1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    guest_call(sub_00079D60_gen,6,QUIT_CANCEL,0u,0u,0u,QUIT_SELECT_CB,0u);
    guest_call(sub_00079D60_gen,6,QUIT_TEXT,1u,0u,0u,QUIT_SELECT_CB,0u);
    guest_call(sub_000771D0,1,menu);
    guest_call(sub_000770E0,2,menu,0u); /* Cancel is the safe default. */
    restore_registers(&r); g_esp+=4;
}
static void quit_back(void)
{
    GuestRegisters r; save_registers(&r);
    guest_call(sub_00079B70,2,quit_parent,quit_previous);
    fprintf(stderr,"[PC-QUIT] Cancelled\n");
    restore_registers(&r); g_esp+=4;
}
static void quit_select(void)
{
    GuestRegisters r; uint32_t menu=WORD_AT(g_esp+4);
    save_registers(&r);
    if(guest_writable(menu,0x190)) {
        if(WORD_AT(menu+0x18c)==0) guest_call(quit_back,1,menu);
        else if(WORD_AT(menu+0x18c)==1 && !quit_requested) {
            /* The window thread owns shutdown through its ordinary close path. */
#ifdef TSFP_NATIVE_DISPLAY
            quit_requested=tsfp_native_request_quit();
#else
            quit_requested=PostMessage(xbox_D3D8Window(),WM_CLOSE,0,0)!=0;
#endif
            if(quit_requested) fprintf(stderr,"[PC-QUIT] Desktop exit confirmed\n");
        }
    }
    restore_registers(&r); g_esp+=4;
}
#endif

void pc_display_menu_finish(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY) || defined(TSFP_QUIT_MENU_TEST)
    const uint32_t caller=WORD_AT(g_esp), menu=WORD_AT(g_esp+4);
    const int frontend=caller==0x002D0EDFu;
    const int paused=caller==0x002F582Fu || caller==0x002F5879u;
#if defined(_WIN32) && !defined(TSFP_QUIT_MENU_TEST)
    if(paused) pc_story_network_pause_rows();
#endif
    if((frontend || paused) && guest_writable(menu,0x3c) &&
       WORD_AT(menu+0x38)==0 && quit_prepare()) {
        /* Main menu's original callback bounds-checks IDs 0..7; this new
           row owns its callback. Do not change the original global choice. */
        guest_call(sub_00079D60_gen,6,QUIT_TEXT,8u,frontend?0x30000u:0u,
                   0u,QUIT_OPEN_CB,0u);
    }
#endif
    original();
#if defined(_WIN32) && !defined(TSFP_QUIT_MENU_TEST)
    if(paused) pc_story_network_pause_layout(menu);
#endif
}
