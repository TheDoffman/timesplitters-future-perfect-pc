/* US character carousel: custom portraits, not ordinary menu rows.
 * Capture the actual drawn quads; retain native preview/loading/confirmation. */
extern void sub_002C4310(void);
typedef struct MenuCharacterPortrait {
    uint32_t menu,group,variant,character,skin;
    float l,t,r,b;
    unsigned sw,sh;
    uint64_t time;
} MenuCharacterPortrait;
static MenuCharacterPortrait menu_character_portraits[64];
static unsigned menu_character_count;
static uint32_t menu_character_drawing;
static unsigned menu_character_byte(uint32_t a) {return WORD_AT(a&~3u)>>((a&3u)*8)&255u;}
static int menu_character_active(uint32_t menu)
{
    return guest_writable(menu,0x23c) && WORD_AT(menu+4)==0x005237E0u &&
        WORD_AT(menu+0x38)==0 && WORD_AT(menu+0x1b0)==3 && !WORD_AT(menu+0x170) &&
        menu_character_byte(0x0075FCF4u)==2;
}
static int menu_character_index(unsigned group,unsigned variant,unsigned *index)
{
    unsigned groups=WORD_AT(0x0078AC98u),count=WORD_AT(0x0078AC94u);
    if(!groups || groups>64 || count>150 || group>=groups ||
       variant>=menu_character_byte(0x0078ACDCu+group)) return 0;
    *index=menu_character_byte(0x0078AC9Cu+group)+variant;
    return *index<count;
}
void pc_display_menu_character_draw(void (*original)(void))
{
    uint32_t previous=menu_character_drawing,menu=WORD_AT(g_esp+4);
    menu_character_drawing=menu_character_active(menu)?menu:0;
    if(menu_character_drawing) menu_character_count=0;
    original();menu_character_drawing=previous;
}
static void menu_character_quad(void)
{
    uint32_t menu=menu_character_drawing,caller=WORD_AT(g_esp);
    if(!menu || (caller!=0x002C5B72u && caller!=0x002C5C32u) ||
       menu_character_count>=64 ||
       !((WORD_AT(g_esp+36)|WORD_AT(g_esp+40)|WORD_AT(g_esp+44)|WORD_AT(g_esp+48))&0xff000000u)) return;
    /* These are stack locals of sub_002C4D20, not generated register values.
     * The primitive has twelve stack arguments plus its return address. */
    unsigned group=WORD_AT(g_esp+0x5c),variant,index;
    if(group>=64) return;
    if(caller==0x002C5B72u) {
        int v=(int32_t)WORD_AT(g_esp+0x88);
        unsigned count=menu_character_byte(0x0078ACDCu+group);
        if(!count || v < -1 || v>(int)count) return;
        variant=(unsigned)(v<0?v+(int)count:v>=(int)count?v-(int)count:v);
    } else variant=WORD_AT(0x0078A148u+group*4);
    if(!menu_character_index(group,variant,&index)) return;
    float l=FLOAT_AT(g_esp+4),t=FLOAT_AT(g_esp+8),r=FLOAT_AT(g_esp+12),b=FLOAT_AT(g_esp+16);
    if(!isfinite(l)||!isfinite(t)||!isfinite(r)||!isfinite(b)||r<=l||b<=t) return;
    l=fmaxf(l,FLOAT_AT(menu+0xe4));t=fmaxf(t,FLOAT_AT(menu+0xe8));
    r=fminf(r,FLOAT_AT(menu+0xec));b=fminf(b,FLOAT_AT(menu+0xf0));
    if(r<=l || b<=t) return;
    MenuCharacterPortrait *p=&menu_character_portraits[menu_character_count++];
    p->menu=menu;p->group=group;p->variant=variant;
    p->character=WORD_AT(0x0078A720u+index*4);
    p->skin=(uint32_t)(int32_t)(int8_t)menu_character_byte(0x0078ABD0u+index);
    p->l=l;p->t=t;p->r=r;p->b=b;p->time=GetTickCount64();
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&p->sw,&p->sh);
}
static void menu_character_select(unsigned group,unsigned variant)
{
    unsigned index;
    if(!menu_character_index(group,variant,&index)) return;
    if(WORD_AT(0x0078A248u)==group && WORD_AT(0x0078A148u+group*4)==variant) return;
    WORD_AT(0x0078A248u)=group;WORD_AT(0x0078A148u+group*4)=variant;
    FLOAT_AT(0x0078A284u)=(float)variant;
    /* Native change-preview helper uses ECX=character, EDI=skin, EDX=player.
     * The outer input adapter saves every guest register around this call. */
    g_ecx=WORD_AT(0x0078A720u+index*4);
    g_edi=(uint32_t)(int32_t)(int8_t)menu_character_byte(0x0078ABD0u+index);g_edx=0;
    guest_call(sub_002C4310,1,1u);
    WORD_AT(0x0078A258u)=1; /* finish the native model-preview transition */
}
static void menu_character_pointer(MenuMouseDispatch *s)
{
    if(WORD_AT(s->menu+4)!=0x005237F0u || !(s->actions&26)) return;
    unsigned sw,sh;float x,y;
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&sw,&sh);
    if(!sh || !pc_menu_mouse_position(s->pointer,(float)sw/sh,
        (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&y)) return;
    /* Only portraits from the most recent active player-1 draw are targets. */
    for(unsigned i=0;i<menu_character_count;i++) {
        const MenuCharacterPortrait *p=&menu_character_portraits[i];unsigned index;
        if(!menu_character_active(p->menu) || p->sw!=sw || p->sh!=sh ||
           GetTickCount64()-p->time>100 || WORD_AT(p->menu+0x2c)!=s->menu ||
           WORD_AT(s->menu+0x16c)!=p->menu || WORD_AT(s->menu+0x170) ||
           !menu_character_index(p->group,p->variant,&index) ||
           WORD_AT(0x0078A720u+index*4)!=p->character ||
           (uint32_t)(int32_t)(int8_t)menu_character_byte(0x0078ABD0u+index)!=p->skin) continue;
        if(s->actions&24) {
            unsigned count=WORD_AT(0x0078AC98u),group=WORD_AT(0x0078A248u);
            if(group>=count) return;
            group=(s->actions&8)?(group?group-1:count-1):(group+1)%count;
            menu_character_select(group,WORD_AT(0x0078A148u+group*4));
            s->actions=0;return;
        }
        if(x>=p->l && x<p->r && y>=p->t && y<p->b) {
            menu_character_select(p->group,p->variant);
            s->actions=0;return; /* preview only; Select confirms */
        }
    }
}

/* The carousel's custom child fails the ordinary row-menu active test.
 * Validate its current container/child relationship instead. Sample once at the container dispatch, before either tick;
 * keep Confirm/Back edges alive until all nested player callbacks return. */
static PcMenuMouse menu_character_mouse;
static void menu_character_input(MenuMouseDispatch *s)
{
    if(!guest_writable(s->menu,0x23c) || WORD_AT(s->menu+4)!=0x005237F0u ||
       WORD_AT(s->menu+0x38)!=0 || WORD_AT(s->menu+0x1b0)!=3 || !menu_mouse_context()) return;
    uint32_t root=WORD_AT(0x007613E4u);
    if(!guest_writable(root,0x23c) || WORD_AT(root+0x170)!=s->menu ||
       WORD_AT(s->menu+0x170)) return;
    GuestRegisters r;save_registers(&r);
    s->pointer=pc_controls_menu_pointer();
    s->actions=pc_menu_mouse_actions(&menu_character_mouse,s->menu,s->pointer,GetTickCount64());
    s->sampled=1;
    menu_character_pointer(s);
    unsigned sw,sh;float x,y;
    d3d8_display_output_shape(WORD_AT(0x004E8CF0u),WORD_AT(0x004E8CF4u),&sw,&sh);
    if((s->actions&2) && sh && pc_menu_mouse_position(s->pointer,(float)sw/sh,
        (float)WORD_AT(0x004E8CF0u),(float)WORD_AT(0x004E8CF4u),&x,&y)) {
        int footer=menu_mouse_footer_hit(s->menu,x,y,sw,sh);
        if(footer==0 || footer==1) menu_mouse_press(s,footer?0x120u:0x80u);
    }
    if(s->actions&4) menu_mouse_press(s,0x120u);
    s->actions=0;
    restore_registers(&r);
}
