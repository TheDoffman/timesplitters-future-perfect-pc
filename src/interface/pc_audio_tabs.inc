/* Audio shares the native profile descriptor, draft, Accept/Cancel and Back.
 * Keep hidden profile records available to native readers and cleanup. */
#define AUDIO_TABS_ID 140u
#define AUDIO_TABS_DRAW 0x00012070u
#define AUDIO_TABS_SELECT 0x00012071u
#define AUDIO_VOLUME_DRAW 0x00012072u
extern void sub_00079710(void),sub_00074760(void);
extern void sub_002C3E20(void),sub_002C3720(void),sub_002C3D30(void);
extern void sub_002C3AC0(void),sub_002C3BF0(void);
extern void sub_002CBE10(void),sub_002EC1F0(void);
static int audio_building,audio_bounds_valid,audio_left=1,audio_pending=-1,audio_anchor_ready;
static uint32_t audio_pending_menu;
static ULONGLONG audio_pending_at;
static float audio_bounds[4],audio_anchor_y;
static uint32_t audio_rows[32],audio_next[32],audio_sibling[32];
static unsigned audio_row_count,audio_native_count,audio_native_selectable;
static uint32_t audio_text(unsigned i) {return audio_menu_storage+32u+i*128u;}
static void audio_link_rows(int all)
{
    if(!audio_font_menu(audio_menu) || !audio_row_count)return;
    uint32_t first=0,last=0;unsigned count=0,selectable=0;
    for(unsigned i=0;i<audio_row_count;i++) {
        uint32_t row=audio_rows[i],type=WORD_AT(row);
        if(!all && (WORD_AT(row+0x28)&0x20u))continue;
        if(all){WORD_AT(row+4)=audio_next[i];WORD_AT(row+8)=audio_sibling[i];}
        else if(last){WORD_AT(last+4)=row;WORD_AT(last+8)=row;}
        if(!first)first=row;
        last=row;count++;if(type==4 || type==7 || type==8)selectable++;
    }
    if(last && !all){WORD_AT(last+4)=0;WORD_AT(last+8)=0;}
    WORD_AT(audio_menu+0x178)=first;
    WORD_AT(audio_menu+0x17c)=all?audio_native_count:count;
    WORD_AT(audio_menu+0x180)=all?audio_native_selectable:selectable;
}
static void audio_collect_rows(void)
{
    audio_native_count=WORD_AT(audio_menu+0x17c);audio_native_selectable=WORD_AT(audio_menu+0x180);
    uint32_t row=WORD_AT(audio_menu+0x178);audio_row_count=0;
    while(row && audio_row_count<32 && guest_writable(row,0x10c)) {
        audio_rows[audio_row_count]=row;audio_next[audio_row_count]=WORD_AT(row+4);
        audio_sibling[audio_row_count++]=WORD_AT(row+8);row=WORD_AT(row+4);
    }
    audio_link_rows(0);
}
static int audio_native_button(void (*original)(void))
{
    if(!audio_building)return 0;
    uint32_t frame=g_esp,id=WORD_AT(frame+8),flags=WORD_AT(frame+12),height=WORD_AT(frame+16);
    if(id==0) {
        /* Same width/title and leading as the other tabbed settings roots. */
        uint32_t builder=WORD_AT(0x7bb1e0);
        if(guest_writable(builder,0x38))WORD_AT(builder+0x34)=bits(78.f);
        WORD_AT(audio_menu+0x50)=WORD_AT(audio_menu+0x54)=WORD_AT(audio_menu+0x58)=bits(78.f);
        WORD_AT(audio_menu+0x204)=audio_options_caption();
        guest_call(sub_00071B10,4,1u,bits(1.2f),1u,1u);
        guest_call(sub_00071C50,1,0x26200u);
        guest_call(sub_00079D60_gen,6,audio_text(0),AUDIO_TABS_ID,0x400u,0u,AUDIO_TABS_SELECT,AUDIO_TABS_DRAW);
        guest_call(sub_00079710,1,0u);
        if(audio_page==1)audio_mix_add();
        else if(audio_page==2)audio_feature_add();
        else if(audio_page==3)audio_listening_add();
        /* Native profile labels/values use their original alignment mode. */
        guest_call(sub_00071C50,1,0x2000u);
    }
    int hidden=id==5 || (id<4 && audio_page!=0);
    if(hidden)WORD_AT(frame+12)=flags|0x21u;
    uint32_t draw=WORD_AT(frame+24);
    if(id==1 || id==3)WORD_AT(frame+24)=AUDIO_VOLUME_DRAW;
    WORD_AT(frame+16)=0; /* compact automatic rows */
    original();WORD_AT(frame+12)=flags;WORD_AT(frame+16)=height;WORD_AT(frame+24)=draw;
    if(hidden && guest_writable(g_eax,0xb4))WORD_AT(g_eax+0xb0)=0;
    return 1;
}
/* Use the existing settings bar beside the caption; the native full-width
 * bar assumes tall console rows. Native draft values and input stay unchanged. */
static void audio_volume_draw(void)
{
    GuestRegisters saved;save_registers(&saved);
    uint32_t menu=WORD_AT(g_esp+4),item=WORD_AT(g_esp+8),rect=WORD_AT(g_esp+12);
    if(audio_font_menu(menu) && guest_writable(item,0x10c) && guest_writable(rect,16)) {
        unsigned id=WORD_AT(item+0x14);float bounds[4];
        memcpy(bounds,(void *)(g_xbox_mem_offset+rect),sizeof bounds);
        if((id==1 || id==3) && isfinite(bounds[2]) && isfinite(bounds[1]) && isfinite(bounds[3])) {
            /* Match the native value-column inset and compact caption baseline. */
            int x=(int)(bounds[2]-122.f),y=(int)((bounds[1]+bounds[3]-8.f)*.5f+3.f);
            uint32_t dark=guest_call(sub_00154FE0,2,0x11193c7fu,WORD_AT(menu+0xf4));
            uint32_t light=guest_call(sub_00154FE0,2,0x69737f7fu,WORD_AT(menu+0xf4));
            menu_slider_capture(id==1?MENU_MUSIC:MENU_SOUND,menu,item,(float)x,(float)y,100.f,8.f);
            unsigned value=WORD_AT(id==1?0x5236bcu:0x5236c0u);if(value>16)value=16;
            guest_call(sub_000814B0,9,bits((float)value/16.f),(uint32_t)x,(uint32_t)y,
                       100u,8u,dark,dark,light,0u);
        }
    }
    restore_registers(&saved);g_esp+=4;
}
static void audio_options_build(void)
{
    GuestRegisters saved;save_registers(&saved);audio_menu=WORD_AT(g_esp+4);
    audio_row_count=0; /* Keep the fixed header hit rectangle across tab rebuilds. */
    memset(audio_mix_rows,0,sizeof audio_mix_rows);memset(audio_feature_rows,0,sizeof audio_feature_rows);
    audio_focus_row=audio_range_row=0;
    if(audio_menu_prepare()) {
        audio_building=1;guest_call(sub_002C3E20,1,audio_menu);audio_building=0;
        audio_collect_rows();guest_call(sub_000771D0,1,audio_menu);
        if(!audio_anchor_ready){audio_anchor_y=FLOAT_AT(audio_menu+0x64);audio_anchor_ready=1;}
        WORD_AT(audio_menu+0x64)=bits(audio_anchor_y);
    } else guest_call(sub_002C3E20,1,audio_menu);
    restore_registers(&saved);g_esp+=4;
}
static void audio_open(void (*original)(void))
{
    /* Measure the largest page first, then show Volume before the first draw.
     * Shorter pages keep that fitted top anchor without blank spacer rows. */
    audio_page=1;audio_menu=0;audio_row_count=0;audio_pending=-1;
    audio_bounds_valid=audio_anchor_ready=0;audio_left=1;
    settings_tabs_input_reset();settings_tabs_hover_reset();original();
    GuestRegisters saved;save_registers(&saved);
    if(audio_font_menu(audio_menu)) {
        audio_link_rows(1);audio_page=0;
        guest_call(sub_00078980,1,audio_menu);guest_call(sub_000770E0,2,audio_menu,0u);
    }
    restore_registers(&saved);
}
static void audio_frontend_open(void){audio_open(sub_002CBE10);}
static void audio_pause_open(void){audio_open(sub_002EC1F0);}
static void audio_change_tab(int target)
{
    if(target<0 || target>3 || target==audio_page)return;
    /* The native tick has already published the profile draft. Building the
     * same descriptor never reruns the profile-opening initializer. */
    audio_link_rows(1);audio_page=target;
    guest_call(sub_00078980,1,audio_menu);guest_call(sub_000770E0,2,audio_menu,0u);
    settings_tabs_sound(8);
}
static void audio_native_action(void (*original)(void))
{
    audio_pending=-1;audio_bounds_valid=0;audio_link_rows(1);original();
    audio_row_count=0; /* Native actions own teardown of the full record chain. */
}
static void audio_options_back(void){audio_native_action(sub_002C3D30);}
static void audio_options_accept(void){audio_native_action(sub_002C3AC0);}
static void audio_options_cancel(void){audio_native_action(sub_002C3BF0);}
static void audio_options_tick(void)
{
    GuestRegisters saved;save_registers(&saved);uint32_t menu=WORD_AT(g_esp+4);
    int direction=settings_tabs_direction(menu);
    if(WORD_AT(menu+0x18c)==AUDIO_TABS_ID && (WORD_AT(menu+0xc4)&0xc0000u)) {
        if(!direction)direction=(WORD_AT(menu+0xc4)&0x40000u)?-1:1;
        guest_call(sub_00074910,3,menu,0u,1u);
    }
    /* Native tick reads IDs0/2/5 even while their records are hidden. */
    audio_link_rows(1);guest_call(sub_002C3720,1,menu);audio_link_rows(0);
    if(direction)audio_change_tab((audio_page+(direction<0?3:1))%4);
    restore_registers(&saved);g_esp+=4;
}
static int audio_pointer_tab(PcMenuPointer pointer)
{
    float x,y,gw=(float)WORD_AT(0x4e8cf0),gh=(float)WORD_AT(0x4e8cf4);unsigned sw,sh;
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(!audio_bounds_valid || !sh || !pc_menu_mouse_position(pointer,(float)sw/sh,gw,gh,&x,&y) ||
       x<audio_bounds[0] || x>=audio_bounds[2] || y<audio_bounds[1] || y>=audio_bounds[3] ||
       audio_bounds[2]<=audio_bounds[0])return -1;
    return (int)((x-audio_bounds[0])*4.f/(audio_bounds[2]-audio_bounds[0]));
}
static void audio_tabs_select(void)
{
    GuestRegisters saved;save_registers(&saved);int target=(audio_page+1)%4;
    if(menu_mouse_dispatch && menu_mouse_dispatch->menu==audio_menu &&
       ((menu_mouse_dispatch->actions&2) || menu_mouse_dispatch->pointer.left || (menu_mouse_dispatch->injected&0x80u)))
        target=audio_pointer_tab(menu_mouse_dispatch->pointer);
    audio_change_tab(target);restore_registers(&saved);g_esp+=4;
}
static void audio_tabs_draw(void)
{
    GuestRegisters saved;save_registers(&saved);uint32_t menu=WORD_AT(g_esp+4),rect=WORD_AT(g_esp+12);
    if(audio_font_menu(menu) && guest_writable(rect,16)) {
        memcpy(audio_bounds,(void *)(g_xbox_mem_offset+rect),sizeof audio_bounds);audio_bounds_valid=1;
        uint32_t captions[]={audio_text(18),audio_text(19),audio_text(20),audio_text(21)};
        settings_tabs_paint(menu,audio_bounds,captions,4,audio_page);
    }
    restore_registers(&saved);g_esp+=4;
}
static int audio_tabs_poll(uint32_t menu)
{
    if(!audio_font_menu(menu) || !menu_mouse_context())return 0;
    PcMenuPointer pointer=pc_controls_menu_hover_pointer();int hit=audio_pointer_tab(pointer);
    settings_tabs_hover(menu,hit,AUDIO_TABS_ID);
    int pressed=pointer.valid && pointer.left && !audio_left;audio_left=pointer.left;
    if(!pressed || hit<0)return 0;
    audio_pending=hit;audio_pending_menu=menu;audio_pending_at=GetTickCount64();
    pc_menu_mouse_actions(&menu_mouse,menu,pointer,audio_pending_at);return 1;
}
static void audio_tabs_flush(void)
{
    if(audio_pending<0)return;
    if(!menu_mouse_context() || !audio_font_menu(audio_pending_menu) || GetTickCount64()-audio_pending_at>1000)
        {audio_pending=-1;return;}
    if(WORD_AT(audio_pending_menu+0x38)!=0 || WORD_AT(audio_pending_menu+0x1b0)!=3)return;
    GuestRegisters saved;save_registers(&saved);
    if(guest_call(sub_00074760,1,audio_pending_menu)) {
        int target=audio_pending;audio_pending=-1;audio_change_tab(target);
    }
    restore_registers(&saved);
}
