/* Keep the original Controller descriptor and handlers, so Customize,
 * Accept/Cancel, selected player and native profile saves retain their ABI. */
#define CONTROLS_DESCRIPTOR 0x00524A40u
#define CONTROLS_TABS_DRAW 0x0001205eu
#define CONTROLS_TABS_SELECT 0x0001205fu
#define CONTROLS_TABS_ID 130u
extern void sub_00079710(void);
extern void sub_002EA3F0(void),sub_002EAB30(void),sub_002E9FC0(void);
extern void sub_002CBE50(void),sub_002EC1A0(void),sub_002EA8E0(void);
extern void sub_002CBE00(void),sub_002E9E50(void),sub_002D0970(void),sub_002E9EA0(void),sub_002EA210(void);
static uint32_t controls_menu;
static uint32_t controls_rows[48],controls_next[48],controls_sibling[48];
static unsigned controls_row_count,controls_native_count,controls_native_selectable;
static int controls_anchor_ready;
static float controls_anchor_y;
/* Native readers still see every original record; layout/input see this tab. */
static void controls_link_rows(int all)
{
    if(!controls_font_menu(controls_menu) || !controls_row_count)return;
    uint32_t first=0,last=0;unsigned count=0,selectable=0;
    for(unsigned i=0;i<controls_row_count;i++) {
        uint32_t item=controls_rows[i],flags=WORD_AT(item+0x28),type=WORD_AT(item);
        if(!all && (flags&0x20u))continue;
        if(all) {WORD_AT(item+4)=controls_next[i];WORD_AT(item+8)=controls_sibling[i];}
        else if(last) {WORD_AT(last+4)=item;WORD_AT(last+8)=item;}
        if(!first)first=item;
        last=item;count++;
        if(type==4 || type==7 || type==8)selectable++;
    }
    if(last && !all) {WORD_AT(last+4)=0;WORD_AT(last+8)=0;}
    WORD_AT(controls_menu+0x178)=first;WORD_AT(controls_menu+0x17c)=all?controls_native_count:count;
    WORD_AT(controls_menu+0x180)=all?controls_native_selectable:selectable;
}
static void controls_collect_rows(void)
{
    controls_native_count=WORD_AT(controls_menu+0x17c);
    controls_native_selectable=WORD_AT(controls_menu+0x180);
    controls_row_count=0;uint32_t item=WORD_AT(controls_menu+0x178);
    while(item && controls_row_count<48 && guest_writable(item,0x10c)) {
        controls_rows[controls_row_count]=item;
        controls_next[controls_row_count]=WORD_AT(item+4);
        controls_sibling[controls_row_count++]=WORD_AT(item+8);
        item=WORD_AT(item+4);
    }
    controls_link_rows(0);
}
/* Native opens copy 31 dwords into a per-player draft (stride 0x1e0). */
static unsigned char controls_original[0x7c];
static int controls_original_ready;
static uint32_t controls_draft(void)
{
    unsigned player=WORD_AT(0x7ba9e0);
    uint32_t address=0x7ba364u+player*0x1e0u;
    return player<4 && guest_writable(address,sizeof controls_original)?address:0;
}
static int controls_dirty(void)
{
    uint32_t draft=controls_draft();
    return !draft || !controls_original_ready ||
        memcmp(controls_original,(void *)(g_xbox_mem_offset+draft),sizeof controls_original)!=0;
}
static void controls_accept_state(void)
{
    for(unsigned i=0;i<controls_row_count;i++) {
        uint32_t row=controls_rows[i];
        if(WORD_AT(row+0x14)==10) {
            if(controls_dirty())WORD_AT(row+0x28)&=~1u;else WORD_AT(row+0x28)|=1u;
            break;
        }
    }
}
static uint32_t controls_text_storage;
static int controls_tab,controls_building,controls_bounds_valid,controls_left=1;
static int controls_pending=-1;
static uint32_t controls_pending_menu;
static ULONGLONG controls_pending_at;
static float controls_bounds[4];
static uint32_t controls_text(int slot,const char *text)
{
    if(!controls_text_storage)controls_text_storage=xbox_HeapAlloc(8u*192u,4);
    if(!controls_text_storage)return 0;
    uint32_t address=controls_text_storage+(uint32_t)slot*192u;
    snprintf((char *)((uintptr_t)address+g_xbox_mem_offset),192,"%s",text);
    return address;
}
static uint32_t controls_caption(void) {return controls_text(7,"Controls");}
static int controls_font_menu(uint32_t menu)
{
    return controls_menu && menu==controls_menu && guest_writable(menu,8) &&
        WORD_AT(menu+4)==CONTROLS_DESCRIPTOR;
}
static int controls_tabs_visible(void)
{return controls_tab!=2 || kb_page==KB_HOME || kb_page==KB_MOUSE;}
static int controls_hit(float x,float y)
{
    float width=controls_bounds[2]-controls_bounds[0];
    if(!controls_bounds_valid || !(width>0) || x<controls_bounds[0] || x>=controls_bounds[2] ||
       y<controls_bounds[1] || y>=controls_bounds[3])return -1;
    return (int)((x-controls_bounds[0])*3.f/width);
}
static int controls_pointer_tab(PcMenuPointer pointer)
{
    float x,y,gw=(float)WORD_AT(0x4e8cf0),gh=(float)WORD_AT(0x4e8cf4);unsigned sw,sh;
    d3d8_display_output_shape((unsigned)gw,(unsigned)gh,&sw,&sh);
    if(!sh || !pc_menu_mouse_position(pointer,(float)sw/sh,gw,gh,&x,&y))return -1;
    return controls_hit(x,y);
}
static int controls_frame(uint32_t menu,const char *subtitle)
{
    if(!controls_font_menu(menu))return 0;
    int tabs=controls_tabs_visible();
    guest_call(sub_00075B70,5,0xa000u,controls_text(0,tabs?"Controls":subtitle),
        bits(tabs || kb_page==KB_LIST?78.f:60.f),0u,0x002E9FC0u);
    guest_call(sub_00071B10,4,1u,bits(1.2f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    if(tabs) {
        guest_call(sub_00079D60_gen,6,controls_text(3," "),CONTROLS_TABS_ID,0x400u,0u,
            CONTROLS_TABS_SELECT,CONTROLS_TABS_DRAW);
        guest_call(sub_00079710,1,0u);
    } else {controls_bounds_valid=0;controls_pending=-1;}
    return 1;
}
static void controls_padding(int count)
{
    for(int i=0;i<count;i++)guest_call(sub_00079D60_gen,6,controls_text(6," "),
        (uint32_t)(140+i),4u,0u,0u,0u);
}
static int controls_native_button(void (*original)(void))
{
    if(!controls_building)return 0;
    /* Keep every native update/save record, hiding only the other section. */
    uint32_t frame=g_esp,id=WORD_AT(frame+8),flags=WORD_AT(frame+12);
    int controller=id==0 || id==3 || id==8 || id==9;
    if(id<10 && controller!=(controls_tab==1))WORD_AT(frame+12)=flags|0x21u;
    guest_call(sub_00071B10,4,1u,bits(1.2f),1u,1u);
    if(id==10 || id==11)guest_call(sub_00071C50,1,0x26200u);
    original();WORD_AT(frame+12)=flags;
    /* Native layout measures rows incrementally before the next constructor.
     * Hidden records must not advance its vertical cursor. */
    if(id<10 && controller!=(controls_tab==1) && guest_writable(g_eax,0xb4))
        WORD_AT(g_eax+0xb0)=0;
    return 1;
}
static void controls_prepare_keyboard(void)
{
    char error[320];kb_caption();
    *kb_status=0;kb_page=KB_HOME;kb_mouse_changed=0;kb_capture_pending=0;
    kb_group=kb_offset=kb_action=kb_slot=0;
    if(!pc_settings_read(&kb_settings,error,sizeof error)) {
        kb_page=KB_ERROR;snprintf(kb_status,sizeof kb_status,"Select a keyboard profile in PC Settings first.");
    }
    kb_saved=kb_settings;
}
static void controls_open_tab(int tab)
{
    GuestRegisters r;save_registers(&r);uint32_t source=WORD_AT(g_esp+4);
    controls_tab=tab;controls_pending=-1;controls_bounds_valid=0;controls_left=1;controls_original_ready=0;controls_anchor_ready=0;
    controls_prepare_keyboard();
    guest_call((int32_t)WORD_AT(0x79094c)>100?sub_002CBE50:sub_002EC1A0,1,source);
    restore_registers(&r);g_esp+=4;
}
static void controls_open(void) {controls_open_tab(0);}
static void controls_open_keyboard(void) {controls_open_tab(2);}
static void controls_controller_build(void)
{
    if(controls_frame(controls_menu,"Controls")) {
        WORD_AT(controls_menu+0x10)=0;
        guest_call(sub_00071C50,1,0x2000u);
        controls_building=1;guest_call(sub_002EA3F0,1,controls_menu);controls_building=0;
        uint32_t draft=controls_draft();
        if(!controls_original_ready && draft) {
            memcpy(controls_original,(void *)(g_xbox_mem_offset+draft),sizeof controls_original);
            controls_original_ready=1;
        }
        controls_collect_rows();controls_accept_state();
        guest_call(sub_000771D0,1,controls_menu);
        guest_call(sub_000770E0,2,controls_menu,0u);
    } else guest_call(sub_002EA8E0,1,controls_menu);
}
/* Retain the first finalized tabbed page's native Y position. Its footer
 * still follows its own content; no blank rows are needed to hold the header. */
static void controls_anchor(void)
{
    if(!controls_tabs_visible())return;
    float y=FLOAT_AT(controls_menu+0x64);
    if(!isfinite(y))return;
    if(!controls_anchor_ready) {controls_anchor_y=y;controls_anchor_ready=1;}
    /* 77320 writes this field but also remeasures pending builder rows. */
    WORD_AT(controls_menu+0x64)=bits(controls_anchor_y);
}
static void controls_build(void)
{
    GuestRegisters r;save_registers(&r);controls_menu=WORD_AT(g_esp+4);controls_row_count=0;
    if(controls_tab==2) {kb_menu=controls_menu;guest_call(kb_build,1,controls_menu);}
    else controls_controller_build();
    controls_anchor();
    restore_registers(&r);g_esp+=4;
}
static void controls_change_tab(int tab)
{
    if(tab==controls_tab)return;
    if(controls_tab==2)kb_flush_mouse();
    controls_link_rows(1);
    controls_tab=tab;guest_call(sub_00078980,1,controls_menu);
    guest_call(sub_000770E0,2,controls_menu,0u);
}
static void controls_leave(void)
{
    controls_pending=-1;controls_bounds_valid=0;
    controls_link_rows(1);guest_call(sub_002E9FC0,1,controls_menu);
}
static void controls_back(void)
{
    GuestRegisters r;save_registers(&r);
    if(controls_tab==2)kb_back_action();else controls_leave();
    restore_registers(&r);g_esp+=4;
}
static void controls_native_action(void (*action)(void))
{
    GuestRegisters r;save_registers(&r);uint32_t menu=WORD_AT(g_esp+4);
    controls_link_rows(1);guest_call(action,1,menu);
    if(controls_font_menu(menu) && guest_call(sub_00074760,1,menu))controls_link_rows(0);
    restore_registers(&r);g_esp+=4;
}
static void controls_accept(void)
{
    if(!controls_dirty()){g_esp+=4;return;}
    controls_native_action((int32_t)WORD_AT(0x79094c)>100?sub_002CBE00:sub_002E9E50);
}
static void controls_cancel(void) {controls_native_action((int32_t)WORD_AT(0x79094c)>100?sub_002D0970:sub_002E9EA0);}
static void controls_customize(void) {controls_native_action(sub_002EA210);}
static void controls_tick(void)
{
    GuestRegisters r;save_registers(&r);uint32_t menu=WORD_AT(g_esp+4);
    if(controls_tabs_visible() && WORD_AT(menu+0x18c)==CONTROLS_TABS_ID) {
        uint32_t buttons=WORD_AT(menu+0xc4);
        if(buttons&0xc0000u) {
            guest_call(sub_00074910,3,menu,0u,1u);
            controls_change_tab((controls_tab+((buttons&0x40000u)?2:1))%3);
        }
    } else if(controls_tab==2)guest_call(kb_tick,1,menu);
    else {controls_link_rows(1);guest_call(sub_002EAB30,1,menu);controls_link_rows(0);}
    if(controls_tab!=2)controls_accept_state();
    restore_registers(&r);g_esp+=4;
}
static void controls_tabs_select(void)
{
    GuestRegisters r;save_registers(&r);int target=(controls_tab+1)%3;
    if(menu_mouse_dispatch && menu_mouse_dispatch->menu==controls_menu &&
       ((menu_mouse_dispatch->actions&2) || menu_mouse_dispatch->pointer.left ||
        (menu_mouse_dispatch->injected&0x80u)))
        target=controls_pointer_tab(menu_mouse_dispatch->pointer);
    if(target>=0)controls_change_tab(target);
    restore_registers(&r);g_esp+=4;
}
static void controls_tabs_draw(void)
{
    GuestRegisters r;save_registers(&r);
    uint32_t menu=WORD_AT(g_esp+4),rect=WORD_AT(g_esp+12);
    if(controls_font_menu(menu) && controls_tabs_visible() && guest_writable(rect,16)) {
        memcpy(controls_bounds,(void *)((uintptr_t)rect+g_xbox_mem_offset),sizeof controls_bounds);
        controls_bounds_valid=1;
        uint32_t captions[]={controls_text(1,"GENERAL CONTROLS"),controls_text(2,"CONTROLLER"),controls_text(4,"KEYBOARD & MOUSE")};
        settings_tabs_paint(menu,controls_bounds,captions,3,controls_tab);
    }
    restore_registers(&r);g_esp+=4;
}
static int controls_tabs_poll(uint32_t menu)
{
    if(!controls_font_menu(menu) || !controls_tabs_visible() || !menu_mouse_context())return 0;
    PcMenuPointer pointer=pc_controls_menu_hover_pointer();
    int pressed=pointer.valid && pointer.left && !controls_left;controls_left=pointer.left;
    if(!pressed)return 0;
    int hit=controls_pointer_tab(pointer);if(hit<0)return 0;
    controls_pending=hit;controls_pending_menu=menu;controls_pending_at=GetTickCount64();
    pc_menu_mouse_actions(&menu_mouse,menu,pointer,controls_pending_at);return 1;
}
static void controls_tabs_flush(void)
{
    if(controls_pending<0)return;
    if(!menu_mouse_context() || !controls_font_menu(controls_pending_menu) || !controls_tabs_visible() ||
       GetTickCount64()-controls_pending_at>1000) {controls_pending=-1;return;}
    if(WORD_AT(controls_pending_menu+0x38)!=0 || WORD_AT(controls_pending_menu+0x1b0)!=3)return;
    GuestRegisters r;save_registers(&r);
    if(guest_call(sub_00074760,1,controls_pending_menu)) {
        int target=controls_pending;controls_pending=-1;controls_change_tab(target);
    }
    restore_registers(&r);
}
