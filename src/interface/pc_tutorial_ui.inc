/* US tutorial cards (sub_001476F0): the console reserves 40 units for an
 * icon. Measure the currently assigned PC labels in each row's own font,
 * widen that column, and let the original engine wrap/size the descriptions.
 * All temporary geometry and guest registers are restored after drawing. */
#include "pc_prompts.h"
extern void sub_00147080(void),sub_00042D10(void),sub_0003FA10(void),sub_0003FCF0(void),sub_0003F8D0(void);
static RECOMP_TLS float tutorial_extra;
static RECOMP_TLS int tutorial_normal_row;
static float tutorial_float(uint32_t address)
{
    uint32_t value=WORD_AT(address);float f;memcpy(&f,&value,4);return f;
}
static void tutorial_set_float(uint32_t address,float value)
{ memcpy(&WORD_AT(address),&value,4); }

static void tutorial_columns(void (*original)(void))
{
    uint32_t frame=g_esp,index=WORD_AT(frame+4),box,rows,view;
    float widest=0,old_extra=tutorial_extra,column;
    int old_row=tutorial_normal_row;
    if(index>=4 || !pc_prompts_keyboard_active() || guest_call(sub_001CDAE0,0)!=1) {
        original();return;
    }
    box=0x007A5940u+index*0x114u;rows=WORD_AT(box+0xE0u);
    view=WORD_AT(0x007E71C8u);
    if(!WORD_AT(box+0xF4u) || rows>8 || !guest_writable(view,0x24) ||
       frame<512 || !guest_writable(frame-512,512)) { original();return; }
    /* Scratch storage is below the real caller, and above helper call frames. */
    g_esp=frame-512;
    for(uint32_t i=0;i<rows;i++) {
        uint32_t row=box+i*28,action=WORD_AT(row+12);
        if((int32_t)WORD_AT(row+24)!=-1 || action>=28)continue;
        uint32_t button=guest_call(sub_00147080,2,action,index);
        /* Stick explanation rows already measure their composite captions. */
        if(button==0x1000u || button==0x2000u)continue;
        uint32_t font=guest_call(sub_0003FA10,1,WORD_AT(row+4));
        WORD_AT(frame-256)=0;
        guest_call(sub_00042D10,2,button,frame-256);
        guest_call(sub_0003FCF0,4,frame-32,frame-28,frame-256,0u);
        float width=tutorial_float(frame-32);
        if(width>widest)widest=width;
        guest_call(sub_0003FA10,1,font);
    }
    g_esp=frame;
    column=tutorial_float(0x004C0460u)*40.f;
    tutorial_extra=widest+8.f-column;
    if(!(tutorial_extra>0.f) || tutorial_extra>512.f) { tutorial_extra=old_extra;original();return; }
    uint32_t old_width=WORD_AT(box+0xECu);
    int right=(int32_t)WORD_AT(view+4u)+1;
    int room=right-16-(int32_t)WORD_AT(box+0xE4u)-(int32_t)old_width;
    int growth=(int)(tutorial_extra+0.999f);
    if(growth>room)growth=room;
    if(growth<0)growth=0;
    WORD_AT(box+0xECu)=old_width+(uint32_t)growth;
    tutorial_normal_row=0;
    original();
    WORD_AT(box+0xECu)=old_width;
    tutorial_extra=old_extra;tutorial_normal_row=old_row;
}

void pc_display_tutorial(void (*original)(void))
{
    uint32_t index=WORD_AT(g_esp+4);
    if(index>=4 || !WORD_AT(0x007A5A34u+index*0x114u) ||
       guest_call(sub_001CDAE0,0)!=1) { original();return; }
    int old_placement=ui_placement;
    uint32_t old_tag=ui_tag;
    int anchor=pc_display_wide_now() && pc_display_hud_scale()<1.f;
    /* Scope the complete card, including its panel and key-label column. */
    if(anchor)ui_place(3,0x001476F0u);
    /* Ten percent smaller in both axes, including the title and controller
       glyphs. Apply before measurement so columns, wrapping and row heights
       use the same size as rendering; leave other HUD/menu text untouched. */
    uint32_t old_x=WORD_AT(0x007E7270u),old_y=WORD_AT(0x007E7280u);
    tutorial_set_float(0x007E7270u,tutorial_float(0x007E7270u)*.9f);
    tutorial_set_float(0x007E7280u,tutorial_float(0x007E7280u)*.9f);
    guest_call(sub_0003F8D0,0);
    tutorial_columns(original);
    WORD_AT(0x007E7270u)=old_x;WORD_AT(0x007E7280u)=old_y;
    guest_call(sub_0003F8D0,0);
    if(anchor)ui_place(old_placement,old_tag);
}

void pc_display_tutorial_wrap(void (*original)(void))
{
    uint32_t frame=g_esp,caller=WORD_AT(frame),saved=WORD_AT(frame+8);
    if(tutorial_extra>0.f) {
        tutorial_normal_row=caller==0x00147CE4u;
        if(tutorial_normal_row)
            tutorial_set_float(frame+8,tutorial_float(frame+8)-tutorial_extra);
    }
    /* The title's word wrapper repeatedly measures prefixes of the same text.
       Reuse validated read spans only until this layout operation returns. */
    pc_prompts_layout_begin();
    original();
    pc_prompts_layout_end();
    WORD_AT(frame+8)=saved;
}
void pc_display_tutorial_text(void (*original)(void))
{
    if(menu_mouse_profile_text(original)) return;
    uint32_t frame=g_esp,saved=WORD_AT(frame+4),color=WORD_AT(frame+16);
    /* The loading-picture label uses this shared font entry, independently of
       tutorial cards. Fit its glyphs with the normal UI scale, but anchor its
       authored inset to the output's left edge rather than the central 4:3 box.
       Its animated dots, font, colour, measurements and fade timing stay native. */
    uint32_t view=WORD_AT(0x007E71C8u);
    if(WORD_AT(frame)==0x001560E5u && pc_display_wide_now() &&
       pc_display_hud_scale()>0.f && guest_writable(view,0x24) &&
       WORD_AT(view)==0 && (WORD_AT(view+4)==639 || WORD_AT(view+4)==640) &&
       WORD_AT(view+0x14)==0 && (WORD_AT(view+0x18)==479 || WORD_AT(view+0x18)==480)) {
        int old=ui_placement;uint32_t old_tag=ui_tag;
        ui_place(3,0x001560E5u);original();ui_place(old,old_tag);
        return;
    }
    /* Guest UI RGB uses half intensity (0x7f is white); its text pass doubles
       it. The text ABI packs RGBA with alpha in the low byte. Keep menu fades,
       and restore the argument so other text/shadow passes are unaffected. */
    if(menu_mouse_footer_text()) WORD_AT(frame+16)=0x405a7f00u|(color&0xffu);
    if(tutorial_extra>0.f && tutorial_normal_row && WORD_AT(frame)==0x00147D17u)
        tutorial_set_float(frame+4,tutorial_float(frame+4)+tutorial_extra);
    original();WORD_AT(frame+4)=saved;WORD_AT(frame+16)=color;
}
