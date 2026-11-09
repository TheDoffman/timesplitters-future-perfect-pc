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
