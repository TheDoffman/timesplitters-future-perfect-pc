/* One native row owns three tabs. Hit testing uses the same final guest-canvas
 * rectangle as painting, so pause/front-end and aspect correction agree. */
#define VIDEO_TABS_ID 120u
#define VIDEO_TABS_DRAW 0x1203fu
static float video_tab_bounds[4];
static int video_tab_bounds_valid;
static uint32_t video_tab_measure;
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
static void video_tabs_draw(void)
{
    extern void sub_00041DC0(void),sub_0003FCF0(void),sub_0001EFB0(void),sub_0003F8D0(void);
    static const char *const names[]={"Display","Graphics","View & HUD"};
    GuestRegisters r;save_registers(&r);
    uint32_t menu=WORD_AT(g_esp+4),rect=WORD_AT(g_esp+12);
    if(menu==current_menu && page!=DISPLAY_CONFIRM && page!=DISPLAY_RESTART && guest_writable(menu,0xf8) && guest_writable(rect,16)) {
        memcpy(video_tab_bounds,(void *)((uintptr_t)rect+g_xbox_mem_offset),sizeof video_tab_bounds);
        video_tab_bounds_valid=1;
        float width=(video_tab_bounds[2]-video_tab_bounds[0])/3.f;
        float top=video_tab_bounds[1],height=video_tab_bounds[3]-top;
        /* Pause uses a narrower cell and a larger inherited font. Measure all
         * captions first, then use one common scale for drawing and centering. */
        if(!video_tab_measure)video_tab_measure=xbox_HeapAlloc(8,4);
        float scale=1.f;
        for(int i=0;i<3 && video_tab_measure;i++) {
            uint32_t caption=text_at(MODE_TEXT_BASE+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT+i,"%s",names[i]);
            guest_call(sub_0003FCF0,4,video_tab_measure,video_tab_measure+4,caption,0u);
            float tw,th;memcpy(&tw,(void *)((uintptr_t)video_tab_measure+g_xbox_mem_offset),4);
            memcpy(&th,(void *)((uintptr_t)video_tab_measure+4+g_xbox_mem_offset),4);
            scale=fminf(scale,pc_ui_caption_fit(width-12.f,height-4.f,tw,th));
        }
        uint32_t saved_x=WORD_AT(0x007E7270u),saved_y=WORD_AT(0x007E7280u);
        if(scale<1.f) {
            float x,y;memcpy(&x,&saved_x,4);memcpy(&y,&saved_y,4);
            x*=scale;y*=scale;guest_call(sub_00022FF0,0);
            memcpy(&WORD_AT(0x007E7270u),&x,4);memcpy(&WORD_AT(0x007E7280u),&y,4);
            guest_call(sub_0003F8D0,0);
        }
        uint32_t align=WORD_AT(0x007E7260u);WORD_AT(0x007E7260u)=0;
        for(int i=0;i<3;i++) {
            float x=video_tab_bounds[0]+width*i;
            int active=i==video_tab_index();
            uint32_t bg=guest_call(sub_00154FE0,2,active?0x34577f7fu:0x1423367fu,WORD_AT(menu+0xf4));
            uint32_t fg=guest_call(sub_00154FE0,2,active?0xddeeff7fu:0x91a8c17fu,WORD_AT(menu+0xf4));
            /* Non-overlapping horizontal strips retain alpha at rounded corners. */
            int h=(int)height,w=(int)(width-4.f);
            static const int insets[]={4,2,1,0};
            for(int y=0;y<h;y++) {
                int edge=y<h-1-y?y:h-1-y,cut=edge<4?insets[edge]:0;
                guest_call(sub_0001EFB0,5,(uint32_t)((int)x+cut),(uint32_t)((int)top+y),
                    (uint32_t)((int)x+w-cut),(uint32_t)((int)top+y+1),bg);
            }
            uint32_t caption=text_at(MODE_TEXT_BASE+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT+i,"%s",names[i]);
            if(!video_tab_measure)video_tab_measure=xbox_HeapAlloc(8,4);
            if(video_tab_measure) {
                guest_call(sub_0003FCF0,4,video_tab_measure,video_tab_measure+4,caption,0u);
                float tw,th;memcpy(&tw,(void *)((uintptr_t)video_tab_measure+g_xbox_mem_offset),4);
                memcpy(&th,(void *)((uintptr_t)video_tab_measure+4+g_xbox_mem_offset),4);
                guest_call(sub_00041DC0,4,bits(x+(w-tw)*.5f),bits(top+(height-th)*.5f),caption,fg);
            }
        }
        if(scale<1.f) {
            guest_call(sub_00022FF0,0);
            WORD_AT(0x007E7270u)=saved_x;WORD_AT(0x007E7280u)=saved_y;
            guest_call(sub_0003F8D0,0);
        }
        WORD_AT(0x007E7260u)=align;
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
