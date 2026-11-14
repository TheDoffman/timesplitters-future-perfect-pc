/* Shared pause Settings hub. Child menus keep their native save/cancel paths. */
static uint32_t pause_settings_storage,pause_settings_parent,pause_settings_previous;
static unsigned pause_settings_port;
static int pause_settings_active;
static uint32_t pause_settings_caption(void)
{
    if(!pause_settings_storage) {
        pause_settings_storage=xbox_HeapAlloc(80,4);
        if(!pause_settings_storage)return 0;
        memset((void *)((uintptr_t)pause_settings_storage+g_xbox_mem_offset),0,80);
        WORD_AT(pause_settings_storage)=PAUSE_SETTINGS_BUILD;
        memcpy((void *)((uintptr_t)(pause_settings_storage+16u)+g_xbox_mem_offset),"Settings",9);
        memcpy((void *)((uintptr_t)(pause_settings_storage+32u)+g_xbox_mem_offset),"Video Options",14);
    }
    return pause_settings_storage+16u;
}
static void pause_settings_open(void)
{
    GuestRegisters saved;save_registers(&saved);
    uint32_t menu=WORD_AT(g_esp+4u);
    if(guest_writable(menu,0x3cu) && WORD_AT(menu+0x38u)<4u && pause_settings_caption()) {
        pause_settings_parent=WORD_AT(menu+0x2cu);
        pause_settings_previous=WORD_AT(menu+4u);
        pause_settings_port=WORD_AT(menu+0x38u);
        pause_settings_active=1;
        guest_call(sub_00079B70,2,pause_settings_parent,pause_settings_storage);
    }
    restore_registers(&saved);g_esp+=4u;
}
static void pause_settings_build(void)
{
    GuestRegisters saved;save_registers(&saved);uint32_t menu=WORD_AT(g_esp+4u);
    guest_call(sub_00075B70,5,0xa000u,pause_settings_caption(),0x425c0000u,0u,PAUSE_SETTINGS_BACK);
    guest_call(sub_00071B10,4,1u,0x40000000u,1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    guest_call(sub_00079D60_gen,6,audio_options_caption(),UINT32_MAX,0u,0u,0x002EC1F0u,0u);
    guest_call(sub_00079D60_gen,6,pause_settings_storage+32u,UINT32_MAX,0u,0u,PC_DISPLAY_MENU_CALLBACK,0u);
    guest_call(sub_00079D60_gen,6,controls_caption(),UINT32_MAX,0u,0u,0x002EC1A0u,0u);
    guest_call(sub_000771D0,1,menu);
    restore_registers(&saved);g_esp+=4u;
}
static void pause_settings_back(void)
{
    GuestRegisters saved;save_registers(&saved);pause_settings_active=0;
    guest_call(sub_00079B70,2,pause_settings_parent,pause_settings_previous);
    restore_registers(&saved);g_esp+=4u;
}
static uint32_t pause_settings_return(uint32_t parent,uint32_t descriptor)
{
    if(!pause_settings_active)return descriptor;
    if((int32_t)WORD_AT(0x79094cu)>100) {pause_settings_active=0;return descriptor;}
    if(parent!=pause_settings_parent || descriptor!=pause_settings_previous)return descriptor;
    uint32_t pause=WORD_AT(0x761c48u+pause_settings_port*4u);
    /* A fresh Start/pause request must open the root, not a stale child hub. */
    if(!guest_writable(pause,4u) || (WORD_AT(pause)&0x81u)!=1u) {
        pause_settings_active=0;return descriptor;
    }
    return pause_settings_storage;
}

void pc_display_menu_finish(void (*original)(void))
{
    const uint32_t caller=WORD_AT(g_esp),menu=WORD_AT(g_esp+4u);
    const int paused=caller==0x002F582Fu || caller==0x002F5879u;
    if(caller==0x002D0EDFu && guest_writable(menu,8u))main_update_notice_open(menu);
#ifdef _WIN32
    if(paused)pc_story_network_pause_rows();
#endif
    original();
#ifdef _WIN32
    if(paused)pc_story_network_pause_layout(menu);
#else
    (void)paused;
#endif
}
