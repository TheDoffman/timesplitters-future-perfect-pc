/* US tutorial cards (sub_001476F0): the console reserves 40 units for an
 * icon. Measure the currently assigned PC labels in each row's own font,
 * widen that column, and let the original engine wrap/size the descriptions.
 * All temporary geometry and guest registers are restored after drawing. */
#include "pc_prompts.h"
extern void sub_00147080(void),sub_00042D10(void),sub_0003FA10(void),sub_0003FCF0(void),sub_0003F8D0(void);
static RECOMP_TLS float tutorial_extra,tutorial_icon_column;
static RECOMP_TLS int tutorial_normal_row,tutorial_row_index;
static RECOMP_TLS float tutorial_row_gap;
static float tutorial_float(uint32_t address)
{
    uint32_t value=WORD_AT(address);float f;memcpy(&f,&value,4);return f;
}
static void tutorial_set_float(uint32_t address,float value)
{ memcpy(&WORD_AT(address),&value,4); }

/* The native painter uses cached height for the panel, then recomputes that
 * unpadded cache at its end. Add row gaps to the painted panel each time; never
 * feed the expanded height into the next frame's cache. */
static void tutorial_rows(void (*original)(void),uint32_t box,unsigned controls)
{
    float old_gap=tutorial_row_gap;int old_index=tutorial_row_index;
    tutorial_row_gap=controls>1?6.f:0.f;tutorial_row_index=0;
    if(controls>1)WORD_AT(box+0xf0u)+=(controls-1)*6u;
    original();
    tutorial_row_gap=old_gap;tutorial_row_index=old_index;
}

static void tutorial_columns(void (*original)(void))
{
    uint32_t frame=g_esp,index=WORD_AT(frame+4),box,rows,view;unsigned controls=0;
    float widest=0,old_extra=tutorial_extra,old_column=tutorial_icon_column,column;
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
        controls++;
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
    if(isfinite(widest) && widest>0.f && widest<512.f)
        tutorial_icon_column=fmaxf(column,widest+8.f)-8.f;
    if(!(tutorial_extra>0.f) || tutorial_extra>512.f) {
        tutorial_extra=old_extra;tutorial_rows(original,box,controls);tutorial_icon_column=old_column;return;
    }
    uint32_t old_width=WORD_AT(box+0xECu);
    int right=(int32_t)WORD_AT(view+4u)+1;
    int room=right-16-(int32_t)WORD_AT(box+0xE4u)-(int32_t)old_width;
    int growth=(int)(tutorial_extra+0.999f);
    if(growth>room)growth=room;
    if(growth<0)growth=0;
    WORD_AT(box+0xECu)=old_width+(uint32_t)growth;
    tutorial_normal_row=0;
    tutorial_rows(original,box,controls);
    WORD_AT(box+0xECu)=old_width;
    tutorial_extra=old_extra;tutorial_normal_row=old_row;tutorial_icon_column=old_column;
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
    if(pc_story_lobby_title(original)) return;
    if(menu_mouse_profile_text(original)) return;
    uint32_t frame=g_esp,saved=WORD_AT(frame+4),saved_y=WORD_AT(frame+8),color=WORD_AT(frame+16);
    uint32_t icon_x=WORD_AT(0x7e7270),icon_y=WORD_AT(0x7e7280);int icon_scaled=0;
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
    /* Normal tutorial action icons share a column. Centre their measured PC
       captions rather than aligning unequal mouse/keycap widths at the left. */
    if(tutorial_icon_column>0.f && WORD_AT(frame)==0x00147CAEu && frame>=128 &&
       guest_writable(frame-128,128)) {
        uint32_t caption=WORD_AT(frame+12);
        g_esp=frame-128;
        guest_call(sub_0003FCF0,4,frame-32,frame-28,caption,0u);
        float old_height=tutorial_float(frame-28);
        /* Keycap artwork extends beyond the text's measured line. Reduce only
           action-icon captions, leaving a gap inside the native row pitch. */
        tutorial_set_float(0x7e7270,tutorial_float(0x7e7270)*.8f);
        tutorial_set_float(0x7e7280,tutorial_float(0x7e7280)*.8f);
        guest_call(sub_0003F8D0,0);icon_scaled=1;
        guest_call(sub_0003FCF0,4,frame-32,frame-28,caption,0u);
        g_esp=frame;
        float width=tutorial_float(frame-32),height=tutorial_float(frame-28);
        if(isfinite(width) && width>=0.f && width<=tutorial_icon_column)
            tutorial_set_float(frame+4,tutorial_float(frame+4)+(tutorial_icon_column-width)*.5f);
        if(isfinite(old_height) && isfinite(height) && old_height>=height && height>0.f)
            tutorial_set_float(frame+8,tutorial_float(frame+8)+(old_height-height)*.5f);
    }
    if(tutorial_extra>0.f && tutorial_normal_row && WORD_AT(frame)==0x00147D17u)
        tutorial_set_float(frame+4,tutorial_float(frame+4)+tutorial_extra);
    uint32_t caller=WORD_AT(frame);
    if(tutorial_row_gap>0.f && (caller==0x00147CAEu || caller==0x00147B0Bu ||
       caller==0x00147BDBu || caller==0x00147D17u))
        tutorial_set_float(frame+8,tutorial_float(frame+8)+tutorial_row_gap*tutorial_row_index);
    original();WORD_AT(frame+4)=saved;WORD_AT(frame+8)=saved_y;WORD_AT(frame+16)=color;
    if(tutorial_row_gap>0.f && caller==0x00147D17u)tutorial_row_index++;
    if(icon_scaled) {
        WORD_AT(0x7e7270)=icon_x;WORD_AT(0x7e7280)=icon_y;
        guest_call(sub_0003F8D0,0);
    }
}
