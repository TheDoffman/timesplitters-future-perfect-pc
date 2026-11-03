/* Shared native tab skin for Video Options and Controls. */
static void settings_tabs_paint(uint32_t menu,const float bounds[4],
                                const uint32_t *captions,int count,int active_index)
{
    extern void sub_00041DC0(void),sub_0003FCF0(void),sub_0001EFB0(void),sub_0003F8D0(void);
    float width=(bounds[2]-bounds[0])/(float)count;
    float top=bounds[1],height=bounds[3]-top;
    /* Pause uses a narrower cell and a larger inherited font. Measure all
     * captions first, then use one common scale for drawing and centering. */
    if(!video_tab_measure)video_tab_measure=xbox_HeapAlloc(8,4);
    float scale=1.f;
    for(int i=0;i<count && video_tab_measure;i++) {
        uint32_t caption=captions[i];
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
    for(int i=0;i<count;i++) {
        float x=bounds[0]+width*i;
        int active=i==active_index;
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
        uint32_t caption=captions[i];
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
