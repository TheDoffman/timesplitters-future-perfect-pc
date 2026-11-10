/* Shared native-font feedback while the desktop companion owns setup. */
static uint32_t online_notice_scratch;
static void online_opening_draw(uint32_t menu)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(!online_opening_pending)return;
    ++online_opening_frames;
    if(!online_notice_scratch)online_notice_scratch=xbox_HeapAlloc(256,4);
    if(!online_notice_scratch)return;
    const uint32_t scratch=online_notice_scratch;
    snprintf((char *)(g_xbox_mem_offset+scratch+8),248,"OPENING MULTIPLAYER - COMPLETE SETUP IN THE MULTIPLAYER WINDOW");
    GuestRegisters saved;save_registers(&saved);
    uint32_t sx=WORD_AT(0x7e7270),sy=WORD_AT(0x7e7280),align=WORD_AT(0x7e7260);
    int placement=ui_placement;uint32_t tag=ui_tag;ui_place(2,0x12060u);WORD_AT(0x7e7260)=0;
    float footer_x,footer_y;memcpy(&footer_x,&settings_footer_x,4);memcpy(&footer_y,&settings_footer_y,4);
    WORD_AT(0x7e7270)=bits(footer_x*.8f);WORD_AT(0x7e7280)=bits(footer_y*.8f);
    guest_call(sub_0003F8D0,0);
    guest_call(sub_0003FCF0,4,scratch,scratch+4,scratch+8,0u);
    float width=(float)WORD_AT(0x4e8cf0),height=(float)WORD_AT(0x4e8cf4);
    float fit=fminf(1.f,(width-32.f)/fmaxf(1.f,FLOAT_AT(scratch)));
    WORD_AT(0x7e7270)=bits(FLOAT_AT(0x7e7270)*fit);WORD_AT(0x7e7280)=bits(FLOAT_AT(0x7e7280)*fit);
    guest_call(sub_0003F8D0,0);guest_call(sub_0003FCF0,4,scratch,scratch+4,scratch+8,0u);
    guest_call(sub_00041DC0,4,bits((width-FLOAT_AT(scratch))*.5f),bits(height-105.f),scratch+8,settings_footer_colour);
    guest_call(sub_00022FF0,0);WORD_AT(0x7e7270)=sx;WORD_AT(0x7e7280)=sy;WORD_AT(0x7e7260)=align;
    guest_call(sub_0003F8D0,0);ui_place(placement,tag);restore_registers(&saved);
#else
    (void)menu;
#endif
}
