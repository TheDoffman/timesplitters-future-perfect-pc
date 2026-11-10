#include "input/pc_control_presets.h"
/* Use the native glyph callback so keycaps follow actual wrapped positions,
 * font size, fades and HUD placement rather than estimating text advances. */
#if !defined(TSFP_KEYCAP_TEST) && defined(__has_include)
#if __has_include("pc_input_icon_data.inc")
#include "pc_input_icon_data.inc"
#endif
#endif
#ifndef TSFP_INPUT_ICONS
#define TSFP_INPUT_ICONS 0
#endif
typedef struct PcKeycapCap {
    int x,y,r,b,radius,arrow;uint32_t colour;
#if TSFP_INPUT_ICONS
    int styled,icon,pressed,controller;
#endif
} PcKeycapCap;
typedef struct PcKeycapDraw {
    char text[2048];size_t next;
    int enabled,open,mouse,arrow,keyboard;
    float left,top,bottom;
    PcKeycapCap caps[64];
#if TSFP_INPUT_ICONS
    int styled,icon,pressed;unsigned char keys[256];
#endif
    unsigned count;
} PcKeycapDraw;
static RECOMP_TLS PcKeycapDraw keycap_draw;
static void keycap_flush(void);
static void keycap_line(int x,int y,int right,int bottom,uint32_t colour);
#if TSFP_INPUT_ICONS
#include "pc_input_icons.inc"
#endif

void pc_display_keycap_text(void (*original)(void))
{
    PcKeycapDraw saved=keycap_draw;memset(&keycap_draw,0,sizeof keycap_draw);
    const uint32_t frame=g_esp;
    uint32_t text=WORD_AT(g_esp+16);
    keycap_draw.keyboard=pc_prompts_keyboard_active();
    if(text) {
        size_t n=0;
        while(n<sizeof keycap_draw.text-1) {
            uintptr_t host=g_xbox_mem_offset+(uintptr_t)text+n;
            size_t room=pc_prompts_read_region(host);
            if(!room)break;
            if(room>sizeof keycap_draw.text-1-n)room=sizeof keycap_draw.text-1-n;
            const char *end=memchr((void *)host,0,room);
            size_t count=end?(size_t)(end-(char *)host):room;
            memcpy(keycap_draw.text+n,(void *)host,count);n+=count;
            if(end) {keycap_draw.enabled=strchr(keycap_draw.text,'[')!=NULL || strchr(keycap_draw.text,'\v')!=NULL;break;}
        }
    }
#ifdef TSFP_ARCADE_MIGRATION
    extern uint32_t pc_multiplayer_migration_prompt(const char *text);
    const uint32_t replacement=pc_multiplayer_migration_prompt(keycap_draw.text);
    if(replacement)WORD_AT(frame+16)=replacement;
#endif
#if TSFP_INPUT_ICONS
    if(keycap_draw.enabled && keycap_draw.keyboard)pc_controls_binding_keys(keycap_draw.keys);
#endif
    original();
#ifdef TSFP_ARCADE_MIGRATION
    if(replacement)WORD_AT(frame+16)=text;
#else
    (void)frame;
#endif
    if(keycap_draw.count) {
        GuestRegisters registers;save_registers(&registers);
        keycap_flush();restore_registers(&registers);
    }
    keycap_draw=saved;
}
static void keycap_flush(void)
{
    /* Finish the font batch before drawing borders, avoiding material changes
     * between individual glyphs. Borders never cover the key-label interiors. */
    guest_call(sub_00022FF0,0);
    for(unsigned n=0;n<keycap_draw.count;n++) {
#if TSFP_INPUT_ICONS
        if(keycap_draw.caps[n].styled) {input_icon_paint(&keycap_draw.caps[n]);continue;}
#endif
        int x=keycap_draw.caps[n].x,y=keycap_draw.caps[n].y;
        int r=keycap_draw.caps[n].r,b=keycap_draw.caps[n].b,radius=keycap_draw.caps[n].radius;
        uint32_t colour=keycap_draw.caps[n].colour;
        keycap_line(x+radius,y,r-radius,y+1,colour);
        keycap_line(x+radius,b-1,r-radius,b,colour);
        keycap_line(x,y+radius,x+1,b-radius,colour);
        keycap_line(r-1,y+radius,r,b-radius,colour);
        for(int i=1;i<radius;i++) {
            keycap_line(x+radius-i,y+i,x+radius-i+1,y+i+1,colour);
            keycap_line(r-radius+i-1,y+i,r-radius+i,y+i+1,colour);
            keycap_line(x+radius-i,b-i-1,x+radius-i+1,b-i,colour);
            keycap_line(r-radius+i-1,b-i-1,r-radius+i,b-i,colour);
        }
        if(keycap_draw.caps[n].arrow) {
            int direction=keycap_draw.caps[n].arrow;
            int mid=(y+b)/2,pad=3;
            int tip=direction<0?x+pad:r-pad-1;
            int tail=direction<0?r-pad:x+pad;
            keycap_line(direction<0?tip:tail,mid,direction<0?tail:tip+1,mid+1,colour);
            int size=(b-y)/4;if(size>4)size=4;
            for(int step=1;step<=size;step++) {
                int ax=tip-direction*step;
                keycap_line(ax,mid-step,ax+1,mid-step+1,colour);
                keycap_line(ax,mid+step,ax+1,mid+step+1,colour);
            }
        }
    }
}

static void keycap_line(int x,int y,int right,int bottom,uint32_t colour)
{
    extern void sub_0001A2E0(void);
    /* Use the rectangle emitter below1EFB0's world-view clamp. Pause text can
     * extend into the footer outside that world view; host scissoring still
     * applies exactly as it does to the native glyph quads. */
    if(right>x && bottom>y)guest_call(sub_0001A2E0,5,bits((float)x),bits((float)y),
                                     bits((float)right),bits((float)bottom),colour);
}
#if TSFP_INPUT_ICONS && defined(TSFP_CONTROLLER_ICONS)
static int keycap_queue_icon(uint32_t state,int icon,int controller)
{
    float x=FLOAT_AT(state+0x18),y=FLOAT_AT(state+0x1c);
    float r=FLOAT_AT(state+0x20),b=FLOAT_AT(state+0x24);
    if(!isfinite(x) || !isfinite(y) || !isfinite(r) || !isfinite(b) ||
       r<=x || b<=y || r-x>2048.f || fabsf(x)>8192.f || fabsf(y)>8192.f ||
       fabsf(r)>8192.f || fabsf(b)>8192.f)return 0;
    PcKeycapCap *cap=&keycap_draw.caps[keycap_draw.count++];
    memset(cap,0,sizeof *cap);cap->x=(int)floorf(x);cap->y=(int)floorf(y);
    cap->r=(int)ceilf(r);cap->b=(int)ceilf(b);cap->colour=WORD_AT(state);
    cap->styled=1;cap->icon=icon;cap->controller=controller;return 1;
}
#endif
void pc_display_keycap_glyph(void (*original)(void))
{
    uint32_t state=WORD_AT(g_esp+4);
    if(!keycap_draw.enabled || !guest_writable(state,0x44)) {original();return;}
    unsigned c=*(unsigned char *)(g_xbox_mem_offset+(uintptr_t)state+0x38);
#if TSFP_INPUT_ICONS && defined(TSFP_CONTROLLER_ICONS)
    if(WORD_AT(state+0x40)) {
        /* Native40FA0 has two icon banks with overlapping zero-based indices.
         * Digit icons use the texture table7E72A0; letter icons use the atlas.
         * Inspect the actual glyph bank, so clipped/wrapped earlier tokens
         * cannot shift which button this glyph represents. */
        uint32_t table=0x7e72a0u+c*4u;
        int digit=c<12 && guest_writable(table,4) && WORD_AT(table) &&
                  WORD_AT(state+0x3c)==WORD_AT(table);
        unsigned token=c+(digit?'1':'A');
#ifndef TSFP_KEYCAP_TEST
        if(!pc_controls_in_menu()) {
            unsigned port=controls_context_port(WORD_AT(0x7b0c7c));
            if(port<4 && pc_settings_controller_preset(port)) {
                unsigned masks[24];int mounted;memcpy(masks,pc_preset_masks,sizeof masks);
                if(controls_gameplay_masks(port,masks,&mounted))token=pc_preset_controller_token(token,masks);
            }
        }
#endif
        int icon=token<256?input_controller_lookup[token]:-1;
        if(icon>=0 && keycap_draw.count<64 && keycap_queue_icon(state,icon,1)) {g_esp+=4;return;}
        original();return;
    }
#endif
    if(WORD_AT(state+0x40)) {original();return;}
    if(!keycap_draw.keyboard) {original();return;}
    if(c=='[') {
        const char *start=strchr(keycap_draw.text+keycap_draw.next,'[');
        const char *end=start?strchr(start+1,']'):NULL;
        keycap_draw.open=0;
        if(start && end) {
            keycap_draw.next=(size_t)(end-keycap_draw.text)+1;
            if(keycap_draw.count<64 && pc_prompts_keycap_label(start+1,(size_t)(end-start-1))) {
                keycap_draw.open=1;keycap_draw.mouse=!_strnicmp(start+1,"Mouse",5) || !_strnicmp(start+1,"Wheel",5);
                keycap_draw.arrow=end-start==2 && start[1]=='<'?-1:end-start==2 && start[1]=='>'?1:0;
#if TSFP_INPUT_ICONS
                int key=pc_prompts_keycap_key(start+1,(size_t)(end-start-1));
                /* Manifest slot zero is the generic mouse-motion prompt, never
                 * the unbound binding value. Physical keys occupy slots1..255. */
                keycap_draw.icon=input_icon_id(key>0?key:
                    end-start==6 && !_strnicmp(start+1,"Mouse",5)?0:-1);
                keycap_draw.styled=keycap_draw.icon>=0;
                keycap_draw.pressed=key>0 && key<256 && keycap_draw.keys[key] && pc_controls_key_down(key);
#endif
                keycap_draw.left=FLOAT_AT(state+0x18);keycap_draw.top=FLOAT_AT(state+0x1c);
                keycap_draw.bottom=FLOAT_AT(state+0x24);g_esp+=4;return;
            }
        }
    } else if(c==']' && keycap_draw.open) {
        float right=FLOAT_AT(state+0x20),top=keycap_draw.top,bottom=keycap_draw.bottom;
        /* A composite can wrap on narrow cards: don't bridge separate lines. */
        if(isfinite(right) && isfinite(keycap_draw.left) && isfinite(top) && isfinite(bottom) &&
           right>keycap_draw.left && bottom>top && right-keycap_draw.left<2048.f &&
           fabsf(top)<8192.f && fabsf(bottom)<8192.f && fabsf(right)<8192.f &&
           fabsf(keycap_draw.left)<8192.f && fabsf(FLOAT_AT(state+0x1c)-top)<2.f) {
            int x=(int)floorf(keycap_draw.left),y=(int)floorf(top)-2,r=(int)ceilf(right),b=(int)ceilf(bottom)+2;
            unsigned n=keycap_draw.count++;
            keycap_draw.caps[n].x=x;keycap_draw.caps[n].y=y;
            keycap_draw.caps[n].r=r;keycap_draw.caps[n].b=b;
            keycap_draw.caps[n].radius=keycap_draw.mouse?3:2;
            keycap_draw.caps[n].arrow=keycap_draw.arrow;
            keycap_draw.caps[n].colour=WORD_AT(state);
#if TSFP_INPUT_ICONS
            keycap_draw.caps[n].styled=keycap_draw.styled;keycap_draw.caps[n].icon=keycap_draw.icon;
            keycap_draw.caps[n].pressed=keycap_draw.pressed;
#endif
        }
        keycap_draw.open=0;g_esp+=4;return;
    }
#if TSFP_INPUT_ICONS
    if(keycap_draw.open && keycap_draw.styled) {g_esp+=4;return;}
#endif
    if(keycap_draw.open && keycap_draw.arrow && (c=='<' || c=='>')) {g_esp+=4;return;}
    original();
}
static void keycap_alternate_glyph(void)
{
    extern void sub_000402D0(void);
    pc_display_keycap_glyph(sub_000402D0);
}
