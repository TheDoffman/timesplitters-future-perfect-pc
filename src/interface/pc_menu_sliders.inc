/* US 45410066 custom menu sliders. Share capture, hit testing and drag
 * ownership, while each setting retains its original draft/commit behavior. */
extern void sub_00045C60(void),sub_00021780(void);
typedef struct MenuSliderTrack {
    uint32_t menu,descriptor,item,id,player;
    float x,y,w,h;
    unsigned sw,sh;
    uint64_t time;
} MenuSliderTrack;
enum { MENU_MUSIC, MENU_SOUND, MENU_SENSITIVITY, MENU_FOV, MENU_KEYBOARD_SENSITIVITY, MENU_SLIDER_COUNT };
static MenuSliderTrack menu_slider_tracks[MENU_SLIDER_COUNT];
static uint32_t menu_slider_drag;
static MenuSliderTrack menu_slider_owner;

/* Capture the drawn bar, not the label or guessed row bounds. */
static void menu_slider_capture(unsigned kind,uint32_t menu,uint32_t item,
    float x,float y,float w,float h)
{
    MenuSliderTrack *t=&menu_slider_tracks[kind];
    memset(t,0,sizeof *t);
    if(!guest_writable(menu,0x23c) || !guest_writable(item,0x10c) ||
       WORD_AT(item+0x88)!=menu || !isfinite(x) || !isfinite(y) ||
       !isfinite(w) || !isfinite(h) || fabsf(x)>100000 || fabsf(y)>100000 ||
       w<=0 || h<=0 || w>100000 || h>100000) return;
    t->menu=menu;t->descriptor=WORD_AT(menu+4);t->item=item;t->id=WORD_AT(item+0x14);
    t->player=WORD_AT(0x007BA9E0u);
    t->x=x;t->y=y;t->w=w;t->h=h;
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&t->sw,&t->sh);
    t->time=GetTickCount64();
}

/* Audio painters use these exact integer offsets in both menu layouts. */
void pc_display_menu_audio_draw(void (*original)(void),int compact)
{
    uint32_t menu=WORD_AT(g_esp+4),item=WORD_AT(g_esp+8),rect=WORD_AT(g_esp+12);
    if(guest_writable(item,0x10c) && guest_writable(rect,16)) {
        uint32_t id=WORD_AT(item+0x14);
        if(id==1 || id==3) {
            float left=FLOAT_AT(rect),top=FLOAT_AT(rect+4);
            float right=FLOAT_AT(rect+8),bottom=FLOAT_AT(rect+12);
            memset(&menu_slider_tracks[id==3],0,sizeof(MenuSliderTrack));
            if(isfinite(left) && isfinite(top) && isfinite(right) && isfinite(bottom) &&
               fabsf(left)<100000 && fabsf(top)<100000 && fabsf(right)<100000 && fabsf(bottom)<100000 &&
               right-left>65 && bottom>top)
                menu_slider_capture(id==3,menu,item,(float)(int)(left+45.f),
                    (float)(int)(top+(bottom-top)*(compact?.65f:.5f)),
                    (float)(int)(right-left-65.f),compact?10.f:20.f);
        }
    }
    original();
}

/* Sensitivity width depends on translated label/font metrics. Capture its
 * actual call to the shared bar painter within this narrowly scoped wrapper.
 * Other callers (including the MapMaker capacity meter) stay display-only. */
static uint32_t menu_slider_draw_menu,menu_slider_draw_item;
void pc_display_menu_sensitivity_draw(void (*original)(void))
{
    uint32_t previous_menu=menu_slider_draw_menu,previous_item=menu_slider_draw_item;
    menu_slider_draw_menu=WORD_AT(g_esp+4);menu_slider_draw_item=WORD_AT(g_esp+8);
    memset(&menu_slider_tracks[MENU_SENSITIVITY],0,sizeof(MenuSliderTrack));
    original();
    menu_slider_draw_menu=previous_menu;menu_slider_draw_item=previous_item;
}
void pc_display_menu_slider_bar(void (*original)(void))
{
    if(menu_slider_draw_menu && WORD_AT(g_esp)==0x002EA3E0u &&
       guest_writable(menu_slider_draw_item,0x10c) &&
       WORD_AT(menu_slider_draw_item+0x14)==9 && WORD_AT(0x007BA9E0u)<4)
        menu_slider_capture(MENU_SENSITIVITY,menu_slider_draw_menu,menu_slider_draw_item,
            (float)(int32_t)WORD_AT(g_esp+8),(float)(int32_t)WORD_AT(g_esp+12),
            (float)(int32_t)WORD_AT(g_esp+16),(float)(int32_t)WORD_AT(g_esp+20));
    original();
}
static void menu_slider_fov_pointer(float x,float left,float width);
static void menu_slider_keyboard_pointer(float x,float left,float width);
extern void sub_00045390(void),sub_00045820(void);
static void menu_slider_set(const MenuSliderTrack *track,unsigned kind,float x)
{
    if(kind==MENU_KEYBOARD_SENSITIVITY) {
        menu_slider_keyboard_pointer(x,track->x,track->w);
    } else if(kind==MENU_FOV) {
        menu_slider_fov_pointer(x,track->x,track->w);
    } else if(kind==MENU_SENSITIVITY) {
        uint32_t address=0x007BA3DCu+track->player*0x1E0u;
        float value=(float)pc_menu_slider_value(x,track->x,track->w,20)*.05f;
        if(FLOAT_AT(address)!=value) {
            FLOAT_AT(address)=value;
            /* Original sub_002EAB30 preview; Accept/Cancel still own saving. */
            if(WORD_AT(0x007BA16Cu) && guest_writable(WORD_AT(0x007BA178u),12)) {
                uint32_t preview=WORD_AT(WORD_AT(0x007BA178u)+8),sp=g_esp;
                g_esp-=0x30;
                guest_call(sub_00045390,1,g_esp);
                WORD_AT(g_esp)=preview;
                guest_call(sub_00045820,1,g_esp);
                g_esp=sp;
            }
        }
    } else {
        unsigned value=pc_menu_slider_value(x,track->x,track->w,16);
        uint32_t volume=kind==MENU_MUSIC?0x005236BCu:0x005236C0u;
        if(WORD_AT(volume)!=value) {
            WORD_AT(volume)=value;
            if(kind==MENU_MUSIC) guest_call(sub_00045C60,0);
            if(WORD_AT(0x007BA16Cu) && guest_writable(WORD_AT(0x007BA178u),12))
                guest_call(sub_00021780,1,WORD_AT(WORD_AT(0x007BA178u)+8));
        }
    }
}

static int menu_slider_track_active(const MenuSliderTrack *t,uint32_t menu,unsigned sw,unsigned sh)
{
    return t->menu==menu && t->descriptor==WORD_AT(menu+4) &&
        t->player==WORD_AT(0x007BA9E0u) &&
        t->sw==sw && t->sh==sh && GetTickCount64()-t->time<=100 &&
        guest_writable(t->item,0x10c) && WORD_AT(t->item+0x88)==menu &&
        WORD_AT(t->item+0x14)==t->id &&
        !(WORD_AT(t->item+0x28)&5u) && t->w>0 && t->h>0;
}

static int menu_slider_pointer(uint32_t menu,PcMenuPointer p,unsigned actions,
    float x,float y,unsigned sw,unsigned sh)
{
    MenuSliderTrack *track=NULL;
    if(!p.left || p.right || menu_mouse.blocked) menu_slider_drag=0;
    for(unsigned i=0;i<MENU_SLIDER_COUNT;i++) {
        MenuSliderTrack *t=&menu_slider_tracks[i];
        if(!menu_slider_track_active(t,menu,sw,sh)) continue;
        if(menu_slider_drag==t->item && menu_slider_owner.menu==t->menu &&
           menu_slider_owner.descriptor==t->descriptor && menu_slider_owner.id==t->id &&
           menu_slider_owner.player==t->player && menu_slider_owner.sw==sw &&
           menu_slider_owner.sh==sh) {track=t;break;}
        if((actions&2) && !p.right &&
           x>=t->x && x<=t->x+t->w && y>=t->y && y<t->y+t->h &&
           x>=FLOAT_AT(menu+0xe4) && x<FLOAT_AT(menu+0xec) &&
           y>=FLOAT_AT(menu+0xe8) && y<FLOAT_AT(menu+0xf0)) {
            menu_slider_drag=t->item;menu_slider_owner=*t;track=t;break;
        }
    }
    if(!track) {menu_slider_drag=0;return 0;}
    /* Capture lasts only while held in this active menu. Moving over labels,
     * the other slider or footer cannot retarget it or synthesize Confirm. */
    menu_mouse.mouse_navigation=1;
    menu_mouse.wheel_pending=0;menu_mouse.scrolling=0;
    WORD_AT(menu+0xc4)&=~0x001F001Fu;
    WORD_AT(menu+0xc8)&=~0x1Fu;
    if(WORD_AT(menu+0x198)!=track->item) guest_call(sub_00076EF0,2,menu,track->item);
    menu_slider_set(track,(unsigned)(track-menu_slider_tracks),x);
    return 1;
}
