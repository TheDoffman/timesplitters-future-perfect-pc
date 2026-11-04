/* Use the native glyph callback so keycaps follow actual wrapped positions,
 * font size, fades and HUD placement rather than estimating text advances. */
typedef struct PcKeycapDraw {
    char text[2048];size_t next;
    int enabled,open,mouse,arrow;
    float left,top,bottom;
    struct {int x,y,r,b,radius,arrow;uint32_t colour;} caps[64];
    unsigned count;
} PcKeycapDraw;
static RECOMP_TLS PcKeycapDraw keycap_draw;
static void keycap_flush(void);
static void keycap_line(int x,int y,int right,int bottom,uint32_t colour);

void pc_display_keycap_text(void (*original)(void))
{
    PcKeycapDraw saved=keycap_draw;memset(&keycap_draw,0,sizeof keycap_draw);
    uint32_t text=WORD_AT(g_esp+16);
    if(pc_prompts_keyboard_active() && text) {
        size_t n=0;
        while(n<sizeof keycap_draw.text-1) {
            uintptr_t host=g_xbox_mem_offset+(uintptr_t)text+n;
            size_t room=pc_prompts_read_region(host);
            if(!room)break;
            if(room>sizeof keycap_draw.text-1-n)room=sizeof keycap_draw.text-1-n;
            const char *end=memchr((void *)host,0,room);
            size_t count=end?(size_t)(end-(char *)host):room;
            memcpy(keycap_draw.text+n,(void *)host,count);n+=count;
            if(end) {keycap_draw.enabled=strchr(keycap_draw.text,'[')!=NULL;break;}
        }
    }
    original();
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
    extern void sub_0001EFB0(void);
    if(right>x && bottom>y)guest_call(sub_0001EFB0,5,(uint32_t)x,(uint32_t)y,
                                     (uint32_t)right,(uint32_t)bottom,colour);
}
void pc_display_keycap_glyph(void (*original)(void))
{
    uint32_t state=WORD_AT(g_esp+4);
    if(!keycap_draw.enabled || !guest_writable(state,0x44) || WORD_AT(state+0x40)) {original();return;}
    unsigned c=*(unsigned char *)(g_xbox_mem_offset+(uintptr_t)state+0x38);
    if(c=='[') {
        const char *start=strchr(keycap_draw.text+keycap_draw.next,'[');
        const char *end=start?strchr(start+1,']'):NULL;
        keycap_draw.open=0;
        if(start && end) {
            keycap_draw.next=(size_t)(end-keycap_draw.text)+1;
            if(keycap_draw.count<64 && pc_prompts_keycap_label(start+1,(size_t)(end-start-1))) {
                keycap_draw.open=1;keycap_draw.mouse=!_strnicmp(start+1,"Mouse",5) || !_strnicmp(start+1,"Wheel",5);
                keycap_draw.arrow=end-start==2 && start[1]=='<'?-1:end-start==2 && start[1]=='>'?1:0;
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
        }
        keycap_draw.open=0;g_esp+=4;return;
    }
    if(keycap_draw.open && keycap_draw.arrow && (c=='<' || c=='>')) {g_esp+=4;return;}
    original();
}
static void keycap_alternate_glyph(void)
{
    extern void sub_000402D0(void);
    pc_display_keycap_glyph(sub_000402D0);
}
