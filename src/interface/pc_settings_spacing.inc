/* Native79710 reads separator height from menu style+30 and73110 paints
 * the silver line at its vertical centre. Reserve equal space on both sides
 * during construction, restoring the shared skin before another menu uses it. */
typedef struct SettingsDividerStyle {uint32_t address,value;} SettingsDividerStyle;
static SettingsDividerStyle settings_divider_begin(uint32_t menu)
{
    SettingsDividerStyle saved={0,0};
    uint32_t style=guest_writable(menu,0x20)?WORD_AT(menu+0x1c):0;
    if(guest_writable(style,0x34)) {
        float height=FLOAT_AT(style+0x30);
        if(isfinite(height) && height>=0.f && height<=20.f) {
            saved.address=style+0x30;saved.value=WORD_AT(saved.address);
            /* 1.2% of the native canvas adds about three logical pixels/side. */
            WORD_AT(saved.address)=bits(height+1.2f);
        }
    }
    return saved;
}
static void settings_divider_end(SettingsDividerStyle saved)
{if(saved.address)WORD_AT(saved.address)=saved.value;}

/* A quarter-canvas top inset matches Video and scales with every output.
 * Keep the footer above84%; unusually tall pages retain native row geometry
 * and move up only as far as needed. No cached row/font/slider data is resized. */
static int settings_panel_anchor(uint32_t menu)
{
    if(!guest_writable(menu,0x6cu))return 0;
    float height=FLOAT_AT(menu+0x68u);
    if(!isfinite(height) || height<=0.f || height>84.f)return 0;
    WORD_AT(menu+0x64u)=bits(fminf(24.f,84.f-height));
    return 1;
}
