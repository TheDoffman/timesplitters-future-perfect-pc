/* Native feedback and fixed section shortcuts, shared by main and pause. */
static uint32_t settings_tabs_font(uint32_t menu,uint32_t fallback)
{
    uint32_t row=guest_writable(menu,0x17c)?WORD_AT(menu+0x178):0;
    for(unsigned n=0;row && n<64 && guest_writable(row,0x2c);n++,row=WORD_AT(row+4)) {
        uint32_t type=WORD_AT(row),id=WORD_AT(row+0x14);
        if(id!=120u && id!=130u && id!=140u && !(WORD_AT(row+0x28)&0x20u) &&
           (type==4 || type==7 || type==8))return settings_row_font(menu,WORD_AT(row+0x1c));
    }
    return settings_row_font(menu,fallback);
}
static void settings_tabs_sound(unsigned slot)
{
    extern void sub_00021780(void);
    uint32_t sounds=WORD_AT(0x7ba178);
    if(WORD_AT(0x7ba16c) && guest_writable(sounds,slot+4))
        guest_call(sub_00021780,1,WORD_AT(sounds+slot));
}
static uint32_t settings_input_menu,settings_input_descriptor;
static unsigned settings_input_held;
static void settings_tabs_input_reset(void) {settings_input_menu=0;settings_input_held=0;}
static int settings_tabs_direction(uint32_t menu)
{
    unsigned char keys[256]={0};
    int secondary=menu_settings_input_owner(menu)>0;
    pc_controls_binding_keys(keys);
    if(getenv("TSFP_SETTINGS_INPUT_TRACE") && WORD_AT(menu+0xc4))
        fprintf(stderr,"[PC-SETTINGS-INPUT] edges=%08x held=%08x\n",WORD_AT(menu+0xc4),WORD_AT(menu+0xc8));
    unsigned arrows=(keys[0x25]?1u:0u)|(keys[0x27]?2u:0u);
    if(arrows && !pc_controls_key_down(0x25) && !pc_controls_key_down(0x27))arrows=0;
    /* Native menu C4/C8 carry Black/White as 0x200/0x400 (confirmed by
     * live X/Z binding replay). The common adapter maps LB/RB to them. */
    unsigned shoulders=0;
    if((secondary || !pc_prompts_keyboard_active()) && pc_controls_accept_pad()) {
        unsigned buttons=WORD_AT(menu+0xc8)|WORD_AT(menu+0xc4);
        shoulders=(buttons&0x200u?4u:0u)|(buttons&0x400u?8u:0u);
        WORD_AT(menu+0xc4)&=~0x06000600u;
    }
    unsigned held=arrows|shoulders;
    if(settings_input_menu!=menu || settings_input_descriptor!=WORD_AT(menu+4)) {
        settings_input_menu=menu;settings_input_descriptor=WORD_AT(menu+4);settings_input_held=held;
        if(arrows)WORD_AT(menu+0xc4)&=~0x000c000cu;
        return 0;
    }
    unsigned fresh=held&~settings_input_held;
    if(arrows || (settings_input_held&3u))WORD_AT(menu+0xc4)&=~0x000c000cu;
    settings_input_held=held;
    return !!(fresh&10u)-!!(fresh&5u);
}
static uint32_t settings_hover_menu,settings_hover_descriptor;
static int settings_hover_index=-1,settings_hover_focused;
static void settings_tabs_hover(uint32_t menu,int hover,unsigned header)
{
    if(!pc_prompts_keyboard_active())hover=-1;
    int focused=WORD_AT(menu+0x18c)==header;
    if(settings_hover_menu==menu && settings_hover_descriptor==WORD_AT(menu+4) &&
       hover>=0 && hover!=settings_hover_index &&
       (settings_hover_index>=0 || settings_hover_focused))settings_tabs_sound(0);
    settings_hover_menu=menu;settings_hover_descriptor=WORD_AT(menu+4);
    settings_hover_index=hover;settings_hover_focused=focused;
}
static void settings_tabs_hover_reset(void) {settings_hover_menu=0;settings_hover_index=-1;}
static uint32_t settings_footer_menu,settings_footer_x,settings_footer_y,settings_footer_colour;
static void settings_tabs_footer_style(uint32_t menu,uint32_t sx,uint32_t sy,uint32_t colour)
{
    if(!menu || settings_footer_menu!=menu) {
        settings_footer_menu=menu;settings_footer_x=sx;settings_footer_y=sy;settings_footer_colour=colour;
    }
}
static void settings_tabs_footer(uint32_t menu)
{
    if(settings_footer_menu!=menu)return;
    if(!((video_font_menu(menu) && page!=DISPLAY_CONFIRM && page!=DISPLAY_RESTART) ||
         (controls_font_menu(menu) && controls_tabs_visible()) || audio_font_menu(menu)))return;
    extern void sub_00041DC0(void),sub_0003FCF0(void),sub_0003F8D0(void);
    static uint32_t scratch;if(!scratch)scratch=xbox_HeapAlloc(256,4);if(!scratch)return;
    snprintf((char *)(g_xbox_mem_offset+scratch+8),248,"%s",pc_prompts_keyboard_active()?
        "[Left] Previous tab   [Right] Next tab   [A] / [D] Adjust":"\v5 Previous tab   \v6 Next tab");
    GuestRegisters saved;save_registers(&saved);
    uint32_t sx=WORD_AT(0x7e7270),sy=WORD_AT(0x7e7280),align=WORD_AT(0x7e7260);
    int place=ui_placement;uint32_t tag=ui_tag;ui_place(2,0x1203fu);WORD_AT(0x7e7260)=0;
    WORD_AT(0x7e7270)=settings_footer_x;WORD_AT(0x7e7280)=settings_footer_y;
    guest_call(sub_0003F8D0,0);
    guest_call(sub_0003FCF0,4,scratch,scratch+4,scratch+8,0u);
    float fit=fminf(1.f,((float)WORD_AT(0x4e8cf0)-32.f)/fmaxf(1.f,FLOAT_AT(scratch)));
    WORD_AT(0x7e7270)=bits(FLOAT_AT(0x7e7270)*fit);
    WORD_AT(0x7e7280)=bits(FLOAT_AT(0x7e7280)*fit);guest_call(sub_0003F8D0,0);
    guest_call(sub_0003FCF0,4,scratch,scratch+4,scratch+8,0u);
    uint32_t colour=settings_footer_colour;
    guest_call(sub_00041DC0,4,bits(((float)WORD_AT(0x4e8cf0)-FLOAT_AT(scratch))*.5f),
        bits((float)WORD_AT(0x4e8cf4)-26.f),scratch+8,colour);
    guest_call(sub_00022FF0,0);WORD_AT(0x7e7270)=sx;WORD_AT(0x7e7280)=sy;
    WORD_AT(0x7e7260)=align;guest_call(sub_0003F8D0,0);
    ui_place(place,tag);restore_registers(&saved);
}
