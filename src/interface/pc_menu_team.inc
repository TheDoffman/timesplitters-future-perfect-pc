/* The multiplayer team chooser is a custom player list, not a type-1 row.
 * Its parent tick consumes raw pad edges before dispatching the child menu. */
typedef struct MenuTeamArrow {
    uint32_t menu,player,team;
    float left,top,right,bottom;
    unsigned sw,sh;
    uint64_t time;
} MenuTeamArrow;
static MenuTeamArrow menu_team_arrows[2];
static uint32_t menu_team_drawing;

static uint32_t menu_team_child(uint32_t parent)
{
    if(!guest_writable(parent,0x23c) || WORD_AT(parent+4)!=0x00524AD8u ||
       WORD_AT(parent+0x1b0)!=3 || WORD_AT(parent+0x38)!=0) return 0;
    uint32_t child=WORD_AT(parent+0x170);
    return guest_writable(child,0x23c) && WORD_AT(child+4)==0x00524AC8u &&
        WORD_AT(child+0x1b0)==3 && !WORD_AT(child+0x170)?child:0;
}
static uint32_t menu_team_player(void)
{
    uint32_t list=WORD_AT(0x00774028u);
    if(!guest_writable(list,0x24)) return 0;
    unsigned count=WORD_AT(list+0x20);
    if(count<1 || count>4) return 0;
    for(unsigned i=0;i<count;i++) {
        uint32_t player=WORD_AT(list+12+i*4);
        if(guest_writable(player,0x124) && WORD_AT(player+0x3c)==0) return player;
    }
    return 0;
}
void pc_display_menu_team_draw(void (*original)(void))
{
    uint32_t previous=menu_team_drawing,pause=WORD_AT(0x00761C48u);
    menu_team_drawing=0;
    if(WORD_AT(g_esp)==0x002EE804u && guest_writable(pause,0x268) &&
       WORD_AT(g_esp+4)==pause+0x25c) {
        uint32_t root=WORD_AT(0x00761BBCu);
        if(guest_writable(root,0x23c))
            menu_team_drawing=menu_team_child(WORD_AT(root+0x170));
    }
    original();menu_team_drawing=previous;
}
static void menu_team_arrow(unsigned direction)
{
    uint32_t caller=WORD_AT(g_esp);
    if(!menu_team_drawing || !((direction==4 && caller==0x002E4EFBu) ||
         (direction==8 && caller==0x002E4F48u))) return;
    uint32_t player=menu_team_player();
    if(!player || WORD_AT(player+0x11c)) return;
    float l=FLOAT_AT(g_esp+4),t=FLOAT_AT(g_esp+8);
    float r=FLOAT_AT(g_esp+12),b=FLOAT_AT(g_esp+16);
    if(!isfinite(l) || !isfinite(t) || !isfinite(r) || !isfinite(b) ||
       r<=l || b<=t || r-l>1000 || b-t>1000) return;
    MenuTeamArrow *a=&menu_team_arrows[direction==8];
    a->menu=menu_team_drawing;a->player=player;a->team=WORD_AT(player+0x48);
    float margin=(b-t)*.4f;
    a->left=l-margin;a->top=t-margin;a->right=r+margin;a->bottom=b+margin;
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&a->sw,&a->sh);
    a->time=GetTickCount64();
}
void pc_display_menu_team_tick(void (*original)(void))
{
    uint32_t child=menu_team_child(WORD_AT(g_esp+4)),player=menu_team_player();
    MenuMouseDispatch edge={0};
    if(child && player && menu_mouse_context()) {
        PcMenuPointer pointer=pc_controls_menu_pointer();
        unsigned actions=pc_menu_mouse_actions(&menu_mouse,child,pointer,GetTickCount64());
        unsigned sw,sh;float x,y;
        d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&sw,&sh);
        if((actions&2) && sh && pc_menu_mouse_position(pointer,(float)sw/sh,
           (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&y)) {
            int footer=menu_mouse_footer_hit(child,x,y,sw,sh);
            if(footer==0 || footer==1) menu_mouse_press(&edge,footer?0x120u:0x80u);
            else if(!WORD_AT(player+0x11c)) for(unsigned i=0;i<2;i++) {
                const MenuTeamArrow *a=&menu_team_arrows[i];
                if(a->menu==child && a->player==player && a->team==WORD_AT(player+0x48) &&
                   a->sw==sw && a->sh==sh && GetTickCount64()-a->time<=100 &&
                   x>=a->left && x<a->right && y>=a->top && y<a->bottom) {
                    menu_mouse_press(&edge,i?2u:1u);break;
                }
            }
        }
    }
    original();
    if(edge.injected)
        WORD_AT(edge.pressed_address)=(WORD_AT(edge.pressed_address)&~edge.injected)|(edge.pressed&edge.injected);
}
