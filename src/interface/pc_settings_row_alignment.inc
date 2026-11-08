/* Native label40 and choiceE0 are cached using the construction font.
 * Re-centre them for the compact draw font, scoped to one menu paint. Native
 * bounds, focus/hit rectangles, horizontal alignment and callbacks stay native. */
extern void sub_0003FA10(void),sub_0003FCF0(void),sub_000718E0(void),sub_0003FB90(void);
typedef struct SettingsRowOffset {uint32_t row,label,value,font,choice_font,flags;} SettingsRowOffset;
typedef struct SettingsRowPaint {
    uint32_t menu;unsigned count;struct SettingsRowPaint *previous;
    SettingsRowOffset rows[64];
} SettingsRowPaint;
static RECOMP_TLS SettingsRowPaint *settings_row_paint;
static uint32_t settings_row_measure;
static float settings_row_float(uint32_t address)
{float value;uint32_t raw=WORD_AT(address);memcpy(&value,&raw,4);return value;}
static void settings_row_store(uint32_t address,float value)
{uint32_t raw;memcpy(&raw,&value,4);WORD_AT(address)=raw;}
/* Native option rows with different size flags inherit different font presets.
 * Use the skin's ordinary option font for every owned settings row. */
static uint32_t settings_row_font(uint32_t menu,uint32_t fallback)
{
    uint32_t skin=guest_writable(menu,0x20)?WORD_AT(menu+0x1c):0;
    return skin && guest_writable(skin,0x4c)?WORD_AT(skin+0x48):fallback;
}
static void settings_rows_begin(SettingsRowPaint *scope,uint32_t menu)
{scope->menu=menu;scope->count=0;scope->previous=settings_row_paint;settings_row_paint=scope;}
static void settings_rows_end(SettingsRowPaint *scope)
{
    for(unsigned i=0;i<scope->count;i++) {
        SettingsRowOffset *saved=&scope->rows[i];
        if(guest_writable(saved->row,0xe4) && WORD_AT(saved->row+0x88)==scope->menu) {
            WORD_AT(saved->row+0x40)=saved->label;WORD_AT(saved->row+0xe0)=saved->value;
            WORD_AT(saved->row+0x1c)=saved->font;WORD_AT(saved->row+0xbc)=saved->choice_font;
            WORD_AT(saved->row+0x28)=saved->flags;
        }
    }
    settings_row_paint=scope->previous;
}
/* 3FCF0 returns the maximum glyph bottom, measured from y=0. Native
 * glyph records also carry a top bearing (float+8, height u16+6). Include
 * that top when centring ordinary single-line captions; complex native
 * formatting retains the native line box rather than duplicating its parser. */
static int settings_row_plain_caption(uint32_t caption)
{
    if(!guest_writable(caption,256))return 0;
    for(unsigned i=0;i<256;i++) {
        unsigned c=(WORD_AT(caption+(i&~3u))>>((i&3u)*8u))&255u;
        if(!c)return 1;
        if(c<32 || c>126)return 0;
    }
    return 0;
}
static float settings_row_text_top(uint32_t caption)
{
    if(!guest_writable(caption,256))return 0.f;
    uint32_t face=WORD_AT(0x6b93b8),glyphs=guest_writable(face,12)?WORD_AT(face+8):0;
    if(WORD_AT(0x6b93a8)) {
        uint32_t selector=WORD_AT(0x6b93bc);
        if(!guest_writable(selector,4) || WORD_AT(selector)>=64)return 0.f;
        glyphs=WORD_AT(0x4c8c30+WORD_AT(selector)*0x38u);
    }
    float scale=settings_row_float(0x7e7290),top=INFINITY;
    if(!isfinite(scale) || scale<=0.f)return 0.f;
    for(unsigned i=0;i<256;i++) {
        unsigned c=(WORD_AT(caption+(i&~3u))>>((i&3u)*8u))&255u;
        if(!c)return isfinite(top)?top:0.f;
        if(c<32 || c>126)return 0.f;
        if(c==32)continue;
        unsigned index=c==124?32u:c-32u;
        uint32_t glyph=glyphs+index*12u;
        if(!glyphs || !guest_writable(glyph,12))return 0.f;
        if(!(WORD_AT(glyph+4)>>16))continue;
        float bearing=settings_row_float(glyph+8)*scale;
        if(!isfinite(bearing) || fabsf(bearing)>10000.f)return 0.f;
        top=fminf(top,bearing);
    }
    return 0.f;
}
static float settings_row_text_height(uint32_t font,uint32_t caption,uint32_t flags,float *top)
{
    if(!guest_writable(caption,1))return 0.f;
    guest_call(sub_0003FA10,1,font);
    if(flags&0x2000u)caption=guest_call(sub_0003FB90,1,caption);
    *top=settings_row_text_top(caption);
    guest_call(sub_0003FCF0,4,settings_row_measure,settings_row_measure+4,caption,0u);
    float height=settings_row_float(settings_row_measure+4);
    return isfinite(height) && height>0.f && height<10000.f?height:0.f;
}
static void settings_row_center(uint32_t menu,uint32_t row,uint32_t rect)
{
    SettingsRowPaint *scope=settings_row_paint;
    if(!scope || scope->menu!=menu || !guest_writable(row,0x10c) ||
       WORD_AT(row+0x88)!=menu || !guest_writable(rect,16))return;
    unsigned type=WORD_AT(row),id=WORD_AT(row+0x14);
    if((type!=4 && type!=7 && type!=8) || (WORD_AT(row+0x28)&0x20u) ||
       id==120 || id==130 || id==140)return;
    uint32_t caption=WORD_AT(row+0x20);if(!guest_writable(caption,1))return;
    float top=settings_row_float(rect+4),bottom=settings_row_float(rect+12);
    float scale=settings_row_float(0x7ba188);
    if(!isfinite(top) || !isfinite(bottom) || bottom<=top || bottom-top>10000.f ||
       !isfinite(scale) || scale<=0.f || scale>10000.f)return;
    if(!settings_row_measure)settings_row_measure=xbox_HeapAlloc(8,4);
    if(!settings_row_measure)return;
    unsigned slot=0;for(;slot<scope->count;slot++)if(scope->rows[slot].row==row)break;
    if(slot==scope->count) {
        if(scope->count==64)return;
        scope->rows[scope->count++]=(SettingsRowOffset){row,WORD_AT(row+0x40),WORD_AT(row+0xe0),WORD_AT(row+0x1c),WORD_AT(row+0xbc),WORD_AT(row+0x28)};
    }
    WORD_AT(row+0x1c)=settings_row_font(menu,WORD_AT(row+0x1c));
    WORD_AT(row+0xbc)=settings_row_font(menu,WORD_AT(row+0xbc));
    /* Match the native Video/Controls option lettering in Audio sidecar rows. */
    if(settings_row_plain_caption(caption))WORD_AT(row+0x28)|=0x2000u;
    uint32_t font=WORD_AT(0x6b93b4),sx=WORD_AT(0x7e7270),sy=WORD_AT(0x7e7280),align=WORD_AT(0x7e7260);
    guest_call(sub_00022FF0,0);
    float top_bearing=0.f;
    float height=settings_row_text_height(WORD_AT(row+0x1c),caption,WORD_AT(row+0x28),&top_bearing);
    if(height>0.f)settings_row_store(row+0x40,fmaxf(0.f,(bottom-top-height-top_bearing)*.5f)/scale);
    unsigned kind=WORD_AT(row+0xb4);
    if(kind>=1 && kind<=4) {
        uint32_t choice=guest_call(sub_000718E0,2,row,WORD_AT(row+0xc8));
        if(guest_writable(choice,8)) {
            top_bearing=0.f;
            height=settings_row_text_height(WORD_AT(row+0xbc),WORD_AT(choice+4),WORD_AT(row+0xb8),&top_bearing);
            float delta=settings_row_float(row+0xe8)-settings_row_float(row+0x68);
            if(height>0.f && isfinite(delta))
                settings_row_store(row+0xe0,fmaxf(0.f,(bottom-top-height-top_bearing)*.5f)/scale-delta);
        }
    }
    WORD_AT(0x7e7270)=sx;WORD_AT(0x7e7280)=sy;WORD_AT(0x7e7260)=align;
    guest_call(sub_0003FA10,1,font);
}
