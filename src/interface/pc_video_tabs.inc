/* One native row owns three tabs. Hit testing uses the same final guest-canvas
 * rectangle as painting, so pause/front-end and aspect correction agree. */
#define VIDEO_TABS_ID 120u
#define VIDEO_TABS_DRAW 0x1203fu
static float video_tab_bounds[4];
static int video_tab_bounds_valid;
static int video_pending_tab=-1,video_tab_left=1;
static uint32_t video_pending_menu;
static ULONGLONG video_pending_at;
static uint32_t video_tab_measure;
static int video_anchor_ready;
static float video_anchor_y;
static int video_tab_index(void)
{ return (page==DISPLAY_GRAPHICS || page==DISPLAY_ADVANCED)?1:page==DISPLAY_VIEW?2:0; }
static int video_tab_page(int index)
{ static const int pages[]={DISPLAY_HOME,DISPLAY_GRAPHICS,DISPLAY_VIEW};return pages[index]; }
static int video_tab_hit(float x,float y)
{
    float width=video_tab_bounds[2]-video_tab_bounds[0];
    if(!video_tab_bounds_valid || width<=0 || x<video_tab_bounds[0] ||
       x>=video_tab_bounds[2] || y<video_tab_bounds[1] || y>=video_tab_bounds[3])return -1;
    return (int)((x-video_tab_bounds[0])*3.f/width);
}
#include "pc_settings_navigation.inc"
#include "pc_settings_tabs.inc"
static void video_tabs_draw(void)
{
    extern void sub_00041DC0(void),sub_0003FCF0(void),sub_0001EFB0(void),sub_0003F8D0(void);
    static const char *const names[]={"DISPLAY","GRAPHICS","VIEW & HUD"};
    GuestRegisters r;save_registers(&r);
    uint32_t menu=WORD_AT(g_esp+4),rect=WORD_AT(g_esp+12);
    if(menu==current_menu && page!=DISPLAY_CONFIRM && page!=DISPLAY_RESTART && guest_writable(menu,0xf8) && guest_writable(rect,16)) {
        memcpy(video_tab_bounds,(void *)((uintptr_t)rect+g_xbox_mem_offset),sizeof video_tab_bounds);
        video_tab_bounds_valid=1;
        uint32_t captions[3];
        for(int i=0;i<3;i++)captions[i]=text_at(MODE_TEXT_BASE+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT+i,"%s",names[i]);
        settings_tabs_paint(menu,video_tab_bounds,captions,3,video_tab_index());
    }
    restore_registers(&r);g_esp+=4;
}
static int video_clicked_tab(void)
{
    /* Only keyboard/controller Confirm may cycle. A mouse Confirm with a
     * missing hit must never masquerade as a request for the next tab. */
    if(!menu_mouse_dispatch || menu_mouse_dispatch->menu!=current_menu)return -1;
    if(!(menu_mouse_dispatch->actions&2) && !menu_mouse_dispatch->pointer.left &&
       !(menu_mouse_dispatch->injected&0x80u))return -1;
    float x,y,gw=(float)WORD_AT(0x004E8CF0u),gh=(float)WORD_AT(0x004E8CF4u);unsigned sw,sh;
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(!sh || !pc_menu_mouse_position(menu_mouse_dispatch->pointer,(float)sw/sh,gw,gh,&x,&y))return -2;
    int hit=video_tab_hit(x,y);
    return hit<0?-2:hit;
}

/* Retain the latest mouse target through the guest's transition/Confirm
 * cooldown. Rebuild only after the outer dispatcher releases its row pointers. */
static int menu_video_tabs_poll(uint32_t menu)
{
    if(controls_tabs_poll(menu))return 1;
    if(!video_font_menu(menu) || page==DISPLAY_CONFIRM || page==DISPLAY_RESTART ||
       !menu_mouse_context())return 0;
    PcMenuPointer pointer=pc_controls_menu_hover_pointer();
    float hx,hy,hgw=(float)WORD_AT(0x4e8cf0),hgh=(float)WORD_AT(0x4e8cf4);unsigned hsw,hsh;
    d3d8_display_output_shape((unsigned)hgw,(unsigned)hgh,&hsw,&hsh);
    int hover=hsh && pc_menu_mouse_position(pointer,(float)hsw/hsh,hgw,hgh,&hx,&hy)?video_tab_hit(hx,hy):-1;
    settings_tabs_hover(menu,hover,VIDEO_TABS_ID);
    int pressed=pointer.valid && pointer.left && !video_tab_left;
    video_tab_left=pointer.left;
    if(!pressed)return 0;
    float x,y,gw=(float)WORD_AT(0x004E8CF0u),gh=(float)WORD_AT(0x004E8CF4u);unsigned sw,sh;
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(!sh || !pc_menu_mouse_position(pointer,(float)sw/sh,gw,gh,&x,&y))return 0;
    int hit=video_tab_hit(x,y);
    if(hit<0)return 0;
    video_pending_tab=hit;video_pending_menu=menu;video_pending_at=GetTickCount64();
    /* Consume this click once; regular rows/footer keep their existing path. */
    pc_menu_mouse_actions(&menu_mouse,menu,pointer,video_pending_at);
    return 1;
}
static void read_choices(void);
static void menu_video_tabs_flush(void)
{
    controls_tabs_flush();
    if(video_pending_tab<0)return;
    if(menu_settings_input_owner(video_pending_menu)>0 || !menu_mouse_context() || !video_font_menu(video_pending_menu) || page==DISPLAY_CONFIRM || page==DISPLAY_RESTART ||
       GetTickCount64()-video_pending_at>1000) {video_pending_tab=-1;return;}
    if(!guest_writable(video_pending_menu,0x1b4) || WORD_AT(video_pending_menu+0x38)!=0 ||
       WORD_AT(video_pending_menu+0x1b0)!=3)return;
    GuestRegisters r;save_registers(&r);
    if(guest_call(sub_00074760,1,video_pending_menu)) {
        int target=video_tab_page(video_pending_tab);video_pending_tab=-1;
        if(target!=page) {
            fprintf(stderr,"[PC-DISPLAY-MENU] Mouse tab %d -> %d\n",page,target);
            read_choices();refresh(target,0);settings_tabs_sound(8);
        }
    }
    restore_registers(&r);
}
