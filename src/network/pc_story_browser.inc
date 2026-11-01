/* Selected-match presentation. Read the advertised scene, never this peer's
 * lobby selection: the browser has not joined that host yet. */
#ifdef _WIN32
extern void sub_00080F20(void);
typedef struct StoryBrowserDraw {
    unsigned active,mission;
    uint32_t rectangle,opacity;
} StoryBrowserDraw;
static StoryBrowserDraw story_browser_draw;

static void story_browser_level_name(uint32_t frame,uint32_t picture)
{
    if(frame<1024u || !guest_writable(frame-512u,512u) ||
       !guest_writable(story_browser_draw.rectangle,16u) ||
       !guest_writable(picture,16u)) return;
    float bounds[4],image[4],opacity;
    memcpy(bounds,(const void *)((uintptr_t)story_browser_draw.rectangle+g_xbox_mem_offset),16u);
    memcpy(image,(const void *)((uintptr_t)picture+g_xbox_mem_offset),16u);
    memcpy(&opacity,&story_browser_draw.opacity,4u);
    if(!isfinite(opacity) || opacity<=0.f) return;
    if(opacity>1.f) opacity=1.f;
    for(unsigned i=0;i<4u;i++) if(!isfinite(bounds[i]) || !isfinite(image[i])) return;
    const float x=bounds[0]+8.f,available=image[0]-x-8.f;
    if(available<24.f || bounds[3]<=bounds[1]) return;
    GuestRegisters saved;save_registers(&saved);g_esp=frame-512u;
    const uint32_t old_sx=WORD_AT(0x7e7270u),old_sy=WORD_AT(0x7e7280u);
    const uint32_t old_font=guest_call(sub_0003FA10,1,3u);
    int16_t id;
    memcpy(&id,(const void *)((uintptr_t)(0x4d1ac2u+story_browser_draw.mission*0x44u)+g_xbox_mem_offset),2u);
    const uint32_t text=guest_call(sub_0003D7B0,1,(uint32_t)(int32_t)id);
    if(text) {
        guest_call(sub_0003FCF0,4,frame-32u,frame-28u,text,0u);
        float width,height;
        memcpy(&width,&WORD_AT(frame-32u),4u);memcpy(&height,&WORD_AT(frame-28u),4u);
        if(isfinite(width) && width>available) {
            float sx,sy;memcpy(&sx,&old_sx,4u);memcpy(&sy,&old_sy,4u);
            sx*=available/width;sy*=available/width;
            memcpy(&WORD_AT(0x7e7270u),&sx,4u);memcpy(&WORD_AT(0x7e7280u),&sy,4u);
            guest_call(sub_0003F8D0,0);
        }
        if(!isfinite(height) || height<=0.f) height=12.f;
        const float y=bounds[1]+height*2.f+4.f;
        uint32_t bx,by;memcpy(&bx,&x,4u);memcpy(&by,&y,4u);
        if(y+height<bounds[3])
            guest_call(sub_00041DC0,4,bx,by,text,0x7f7f7f00u|(uint32_t)(opacity*255.f));
    }
    guest_call(sub_0003FA10,1,old_font);
    WORD_AT(0x7e7270u)=old_sx;WORD_AT(0x7e7280u)=old_sy;
    guest_call(sub_0003F8D0,0);
    restore_registers(&saved);
}
#endif

void pc_story_browser_match(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    const StoryBrowserDraw previous=story_browser_draw;
    story_browser_draw.active=0;
    /* System Link list draw1CBDA0 receives (menu,item,rectangle). Each
       item's advertised-match index selects a CC-byte record at790960. */
    const uint32_t menu=WORD_AT(frame+4u),item=WORD_AT(frame+8u);
    if(pc_online_mode()==PC_ONLINE_STORY && WORD_AT(0x79094cu)==101u &&
       guest_writable(frame,16u) && guest_writable(menu,0xf8u) &&
       guest_writable(item,0x18u) && WORD_AT(item+0x14u)<20u &&
       guest_writable(WORD_AT(frame+12u),16u)) {
        const uint32_t record=0x790960u+WORD_AT(item+0x14u)*0xccu;
        const unsigned scene=WORD_AT(record+0x14u);
        if(WORD_AT(record+0x18u)==2u)
        for(unsigned i=0;i<13u;i++) {
            int16_t level_scene;
            memcpy(&level_scene,(const void *)((uintptr_t)(0x4d1ac6u+i*0x44u)+g_xbox_mem_offset),2u);
            if(level_scene>=0 && (unsigned)level_scene==scene) {
                story_browser_draw=(StoryBrowserDraw){1u,i,WORD_AT(frame+12u),WORD_AT(menu+0xf4u)};
                break;
            }
        }
    }
    original();
    story_browser_draw=previous;
#else
    original();
#endif
}

void pc_story_browser_thumbnail(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t frame=g_esp;
    if(story_browser_draw.active && guest_writable(frame,20u) &&
       WORD_AT(frame)==0x1cbfc4u && guest_writable(WORD_AT(frame+12u),16u)) {
        const uint32_t sprite=WORD_AT(0x4d1ad4u+story_browser_draw.mission*0x44u);
        if(sprite!=UINT32_MAX) {
            GuestRegisters saved;save_registers(&saved);
            const uint32_t texture=guest_call(sub_00080F20,1,sprite);
            restore_registers(&saved);
            const uint32_t old_texture=WORD_AT(frame+16u),rectangle=WORD_AT(frame+12u);
            WORD_AT(frame+16u)=texture;
            original();
            WORD_AT(frame+16u)=old_texture;
            story_browser_level_name(frame,rectangle);
            return;
        }
    }
#endif
    original();
}
