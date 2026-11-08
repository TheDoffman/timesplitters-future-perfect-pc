/* Flat section labels use the native option-row selection colours. */
static void settings_tabs_paint(uint32_t menu,const float bounds[4],
                                const uint32_t *captions,int count,int active_index)
{
    extern void sub_00041DC0(void),sub_0003FCF0(void),sub_0001EFB0(void),sub_0003F8D0(void),sub_0003FA10(void);
    float width=(bounds[2]-bounds[0])/(float)count;
    float top=bounds[1],height=bounds[3]-top;
    float fit_width=(bounds[2]-bounds[0])/4.f;
    /* Custom header callbacks inherit a different frontend font. Native label
     * painter76150 selects row+1c through3FA10; use the first visible setting's
     * font for both measuring and drawing, then restore the enclosing font. */
    uint32_t saved_font=WORD_AT(0x6b93b4),font=settings_tabs_font(menu,saved_font);
    if(font!=saved_font) {guest_call(sub_00022FF0,0);guest_call(sub_0003FA10,1,font);}
    /* Pause uses a narrower cell and a larger inherited font. Measure all
     * captions first, then use one common scale for drawing and centering. */
    if(!video_tab_measure)video_tab_measure=xbox_HeapAlloc(40,4);
    /* Fit both sections against the same longest tab, so shorter Video
     * captions do not become larger than the Controls captions. */
    if(video_tab_measure)memcpy((void *)(g_xbox_mem_offset+video_tab_measure+8),
                              "KEYBOARD & MOUSE",17);
    float scale=1.f;
    for(int i=0;i<=count && video_tab_measure;i++) {
        uint32_t caption=i<count?captions[i]:video_tab_measure+8;
        guest_call(sub_0003FCF0,4,video_tab_measure,video_tab_measure+4,caption,0u);
        float tw,th;memcpy(&tw,(void *)((uintptr_t)video_tab_measure+g_xbox_mem_offset),4);
        memcpy(&th,(void *)((uintptr_t)video_tab_measure+4+g_xbox_mem_offset),4);
        scale=fminf(scale,pc_ui_caption_fit(fit_width-12.f,height-4.f,tw,th));
    }
    uint32_t saved_x=WORD_AT(0x007E7270u),saved_y=WORD_AT(0x007E7280u);
    if(scale<1.f) {
        float x,y;memcpy(&x,&saved_x,4);memcpy(&y,&saved_y,4);
        x*=scale;y*=scale;guest_call(sub_00022FF0,0);
        memcpy(&WORD_AT(0x007E7270u),&x,4);memcpy(&WORD_AT(0x007E7280u),&y,4);
        guest_call(sub_0003F8D0,0);
    }
    uint32_t align=WORD_AT(0x007E7260u);WORD_AT(0x007E7260u)=0;
    int hovered=-1;
    if(menu_mouse_context() && pc_prompts_keyboard_active()) {
        PcMenuPointer pointer=pc_controls_menu_hover_pointer();
        float px,py,gw=(float)WORD_AT(0x4e8cf0),gh=(float)WORD_AT(0x4e8cf4);
        unsigned sw,sh;d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
        if(sh && pc_menu_mouse_position(pointer,(float)sw/sh,gw,gh,&px,&py) &&
           px>=bounds[0] && px<bounds[2] && py>=top && py<bounds[3])
            hovered=(int)((px-bounds[0])/width);
    }
    int focused=guest_writable(menu,0x190) &&
        (WORD_AT(menu+0x18c)==120u || WORD_AT(menu+0x18c)==130u || WORD_AT(menu+0x18c)==140u);
    int highlighted=hovered>=0?hovered:focused?active_index:-1;
    for(int i=0;i<count;i++) {
        float x=bounds[0]+width*i;
        int active=i==active_index;
        int w=(int)(width-4.f);
        /* Native glyph RGB is half intensity (0x7f is white). */
        uint32_t fg=guest_call(sub_00154FE0,2,active?0x405a7f7fu:0x7073777fu,WORD_AT(menu+0xf4));
        /* One flat highlight follows pointer/focus; inactive labels have no
         * background, border or bevel. The current section stays blue. */
        if(i==highlighted) {
            uint32_t bg=guest_call(sub_00154FE0,2,0x354b6448u,WORD_AT(menu+0xf4));
            int left=(int)x,right=(int)(x+w),y0=(int)top,h=(int)bounds[3]-y0;
            /* Match the native option-row six-unit rounded corners. Disjoint
             * strips keep the translucent fill even at the curved edges. */
            for(int y=0;y<h;y++) {
                static const unsigned inset[]={4,2,1,1,0,0};
                int edge=y<h-1-y?y:h-1-y;
                unsigned cut=edge<6?inset[edge]:0;
                int end=edge>=6?h-6:y+1;
                guest_call(sub_0001EFB0,5,(uint32_t)(left+cut),(uint32_t)(y0+y),
                    (uint32_t)(right-cut),(uint32_t)(y0+end),bg);
                y=end-1;
            }
        }
        uint32_t caption=captions[i];
        if(!video_tab_measure)video_tab_measure=xbox_HeapAlloc(40,4);
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
    if(font!=saved_font) {guest_call(sub_00022FF0,0);guest_call(sub_0003FA10,1,saved_font);}
}
