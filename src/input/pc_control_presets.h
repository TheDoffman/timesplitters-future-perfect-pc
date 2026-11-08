#ifndef PC_CONTROL_PRESETS_H
#define PC_CONTROL_PRESETS_H
/* US 18FFF0/190010 translate raw Xbox controls to these title masks. */
static inline const unsigned *pc_preset_native_masks(void) {static const unsigned masks[24]={0x80,0x20,0x10,0x40,0x4000,0x8000,0x200,0x100,
    0x400,0x800,4,8,1,2,0,0,0,0,0x1000,0,0,0,0,0x2000};return masks;}
#define pc_preset_masks (pc_preset_native_masks())
/* Physical source -> semantic action; menus never use this table. */
static inline const unsigned char *pc_preset_pad_actions(void) {static const unsigned char actions[24]={0,18,2,13,10,8,6,7,23,9,10,11,12,13,14,15,16,17,18,19,20,21,22,1};return actions;}
#define pc_modern_pad_actions (pc_preset_pad_actions())
/* Physical masks for a semantic action, from the same Modern Controller table. */
static inline unsigned pc_preset_custom_pad_mask(unsigned action)
{
    unsigned result=0;
    for(unsigned source=0;source<24;source++)
        if(pc_modern_pad_actions[source]==action)result|=pc_preset_masks[source];
    return result;
}

/* Native fonts contain physical button tokens. Resolve them back through the
 * active native profile before displaying a Modern semantic binding. */
static inline unsigned pc_preset_semantic(unsigned raw,const unsigned masks[24])
{
    static const unsigned char roles[]={0,1,2,8,9,10,11,12,13,18,23};
    if(raw>=24 || !pc_preset_masks[raw])return raw;
    for(unsigned j=0;j<sizeof roles/sizeof roles[0];j++)
        if(masks[roles[j]]&pc_preset_masks[raw])return roles[j];
    return raw;
}
static inline unsigned pc_preset_controller_token(unsigned token,const unsigned masks[24])
{
    unsigned raw;
    switch(token) {
    case '1':raw=1;break;case '2':raw=0;break;case '3':raw=3;break;case '4':raw=2;break;
    case '5':raw=4;break;case '6':raw=5;break;
    case 'G':case 'J':case 'K':raw=8;break;case 'I':raw=9;break;
    case 'L':raw=18;break;case 'M':raw=23;break;case 'N':raw=10;break;case 'O':raw=11;break;
    default:return token;
    }
    switch(pc_preset_semantic(raw,masks)) {
    case 0:return '2';case 1:return 'M';case 2:return '4';case 8:return '6';case 9:return 'I';
    case 10:return '5';case 11:return 'O';case 13:return '3';case 18:return '1';case 23:return 'G';
    default:return token;
    }
}
static inline void pc_preset_accumulate(int values[24],unsigned action,int magnitude,int modern,const unsigned masks[24])
{
    if(!magnitude || action>=24)return;
    if(!modern || !pc_preset_masks[action]) {if(magnitude>values[action])values[action]=magnitude;return;}
    for(unsigned target=0;target<24;target++)
        if(pc_preset_masks[target] && (masks[action]&pc_preset_masks[target]) && magnitude>values[target])values[target]=magnitude;
}
#endif
