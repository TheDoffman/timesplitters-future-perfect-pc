#ifndef PC_KEYBOARD_ACTIONS_H
#define PC_KEYBOARD_ACTIONS_H
/* Translate keyboard action meanings through the active guest layout. Physical
 * controller input never enters this translation. No selectable presets. */
static inline const unsigned *pc_keyboard_native_masks(void) {static const unsigned masks[24]={0x80,0x20,0x10,0x40,0x4000,0x8000,0x200,0x100,
    0x400,0x800,4,8,1,2,0,0,0,0,0x1000,0,0,0,0,0x2000};return masks;}
#define pc_keyboard_masks (pc_keyboard_native_masks())
/* Compatibility keyboard rows: Y=next weapon, Black=mode, White=grenade. */
static inline unsigned pc_keyboard_action(unsigned action)
{static const unsigned aliases[]={13,10,8};return action>=3 && action<=5?aliases[action-3]:action;}
/* Original input consumers differ by context: B6A67/258BE2 use Reload
 * to dismount guns; D2557 emits the vehicle exit packet from physical Y. */
static inline unsigned pc_keyboard_context_masks(unsigned masks[24],unsigned mode)
{
    if(mode==0x40u || mode==0x80u)masks[0]=masks[2];
    else if(mode==0x400u) {
        /* D2470 consumes fixed physical buttons, not on-foot profile masks. */
        for(unsigned role=0;role<24;role++)
            if(pc_keyboard_masks[role])masks[role]=pc_keyboard_masks[role];
        masks[0]=0x40u;
    }
    return mode;
}
static inline unsigned pc_keyboard_context_action(unsigned action,unsigned mode)
{
    if(mode==0x400u && action>=3u && action<=5u)return action; /* Legacy vehicle buttons. */
    return pc_keyboard_action(action);
}
typedef struct PcKeyboardContextState {unsigned mode;int down,blocked;} PcKeyboardContextState;
static inline int pc_keyboard_context_sample(PcKeyboardContextState *state,unsigned mode,int magnitude)
{
    int down=magnitude>0;
    if(state->mode!=mode && state->down && down)state->blocked=1;
    if(!down)state->blocked=0;
    state->mode=mode;state->down=down;
    return state->blocked?0:magnitude;
}
static inline void pc_keyboard_axis_masks(unsigned masks[24],const unsigned axes[4])
{
    static const unsigned positive[4]={17,14,22,19};
    static const unsigned negative[4]={16,15,21,20};
    static const unsigned roles[8]={14,15,17,16,19,20,22,21};
    for(unsigned i=0;i<8;i++) {
        unsigned axis=axes[i/2];
        masks[roles[i]]=axis>=1 && axis<=4 ? 1u<<((i&1)?negative[axis-1]:positive[axis-1]):0;
    }
}
static inline void pc_keyboard_accumulate(int values[24],unsigned action,int magnitude,int translate,const unsigned masks[24])
{
    if(!magnitude || action>=24)return;
    if(translate && ((action>=14 && action<=17) || (action>=19 && action<=22))) {
        for(unsigned target=14;target<=22;target++)
            if(target!=18 && (masks[action]&(1u<<target)) && magnitude>values[target])values[target]=magnitude;
        return;
    }
    if(!translate || !pc_keyboard_masks[action]) {if(magnitude>values[action])values[action]=magnitude;return;}
    for(unsigned target=0;target<24;target++)
        if(pc_keyboard_masks[target] && (masks[action]&pc_keyboard_masks[target]) && magnitude>values[target])values[target]=magnitude;
}
#endif
