/* Production display model and guest callbacks with isolated settings and RAM.
 * Original menu drawing/input functions are stubs; no game data is needed. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static ULONGLONG clock_ms;
static ULONGLONG ticks(void) { return clock_ms; }
#define GetTickCount64 ticks
#include "../src/interface/pc_display.c"
#include "../src/interface/pc_display_menu.c"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
static int available=1,returned;
static int enhanced_audio,audio_save_ok=1;
int pc_audio_enhanced_enabled(void) {return enhanced_audio;}
int pc_audio_set_enhanced(int enabled) {if(!audio_save_ok)return 0;enhanced_audio=!!enabled;return 1;}
static unsigned built_rows, built_row_count, built_ids[32],separators;
static char runtime_path[MAX_PATH];
static int requested_rate=-1;
int recomp_config_request_native_rate(int rate) { requested_rate=rate;return 1; }
const char *recomp_config_lookup(const char *env,const char *key)
{ (void)env;(void)key;return requested_rate==144?"144":requested_rate==120?"120":requested_rate==180?"180":requested_rate==240?"240":"0"; }
uint32_t recomp_config_title_id(void) { return 0x45410066; }
int recomp_settings_path(uint32_t title,char *out,size_t size)
{ (void)title;return snprintf(out,size,"%s",runtime_path)>0; }
static int slider_capture_enabled;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
HWND xbox_D3D8Window(void) { return NULL; }
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) { (void)w;(void)h;return available; }
int pc_online_connect(void) { return 0; }
void pc_online_disconnect(void) {}
uint32_t xbox_HeapAlloc(uint32_t size,uint32_t alignment) { (void)size;(void)alignment;return 0x100000; }
#define STUB(name) void name(void) { g_esp+=4; }
/* The menu translation unit also contains tutorial hooks. Their drawing and
 * input behavior is covered by tutorial_layout.c; these paths must not run
 * during the FOV/settings test. */
int pc_prompts_keyboard_active(void) { CHECK(0); return 0; }
void pc_prompts_layout_begin(void) {}
void pc_prompts_layout_end(void) {}
#define TUTORIAL_STUB(name) void name(void) { CHECK(0); }
TUTORIAL_STUB(sub_00147080) TUTORIAL_STUB(sub_00042D10)
TUTORIAL_STUB(sub_0003FA10) TUTORIAL_STUB(sub_0003FCF0) TUTORIAL_STUB(sub_0003F8D0)
#undef TUTORIAL_STUB
/* Mouse-menu behavior is exercised by menu_mouse.c. Keep its dependencies
 * linked here, but fail if this settings-only fixture enters those paths. */
void pc_controls_text_mode(uint32_t owner) {(void)owner;CHECK(0);}
unsigned pc_controls_text_event(uint32_t owner) {(void)owner;CHECK(0);return 0;}
void pc_controls_menu_mode(int active) { (void)active; CHECK(0); }
PcMenuPointer pc_controls_menu_pointer(void) { CHECK(0); return (PcMenuPointer){0}; }
PcMenuPointer pc_controls_menu_hover_pointer(void) { CHECK(0); return (PcMenuPointer){0}; }
void d3d8_display_output_shape(unsigned w,unsigned h,unsigned *ow,unsigned *oh)
{ (void)w;(void)h;CHECK(slider_capture_enabled);*ow=1920;*oh=1080; }
#define MOUSE_STUB(name) void name(void) { CHECK(0); }
MOUSE_STUB(sub_00074760) MOUSE_STUB(sub_00076EF0) MOUSE_STUB(sub_00076510)
MOUSE_STUB(sub_002E6D40) MOUSE_STUB(sub_00079860)
MOUSE_STUB(sub_002C4310)
MOUSE_STUB(sub_001922D0)
MOUSE_STUB(sub_002E49F0) MOUSE_STUB(sub_002E4A30)
/* Audio-slider preview calls are covered by menu_mouse.c, not this fixture. */
MOUSE_STUB(sub_00045C60) MOUSE_STUB(sub_00021780)
MOUSE_STUB(sub_00045390) MOUSE_STUB(sub_00045820)
MOUSE_STUB(sub_0007DB20) MOUSE_STUB(sub_0007C410) MOUSE_STUB(sub_0007C300)
MOUSE_STUB(sub_0007C390) MOUSE_STUB(sub_0007C4F0) MOUSE_STUB(sub_0007CDE0)
MOUSE_STUB(sub_000746D0) MOUSE_STUB(sub_0007CCA0) MOUSE_STUB(sub_00071940)
#undef MOUSE_STUB
/* Bot helpers share the menu translation unit, but must never execute in
 * this isolated FOV fixture. Bot behavior has its own network regressions. */
#define BOT_STUB(name) void name(void) { CHECK(0); }
BOT_STUB(sub_000DAC30) BOT_STUB(sub_000D7570) BOT_STUB(sub_0009FBE0)
BOT_STUB(sub_000966E0) BOT_STUB(sub_001BE870) BOT_STUB(sub_0035C870)
BOT_STUB(sub_000CF440) BOT_STUB(sub_001BB5C0)
BOT_STUB(sub_002C2C20) BOT_STUB(sub_002C0490) BOT_STUB(sub_002C19F0)
BOT_STUB(sub_002C1570) BOT_STUB(sub_002C1490)
#undef BOT_STUB
/* Unlock Content has its own profile/menu fixture. This settings-only test
 * must link those dependencies without accidentally entering their paths. */
#define UNLOCK_STUB(name) void name(void) { CHECK(0); }
UNLOCK_STUB(sub_00356870) UNLOCK_STUB(sub_000696F0)
UNLOCK_STUB(sub_001CD040) UNLOCK_STUB(sub_00075E70)
#undef UNLOCK_STUB
/* Other shared translation-unit hooks must not run in this display fixture. */
int pc_weapon_slot_take(void) { CHECK(0);return 0; }
STUB(sub_001BAA70) STUB(sub_001BA410) STUB(sub_001BA130) STUB(sub_00142820)
STUB(sub_00071B60) STUB(sub_00041DC0)
static int storage_continued,storage_chosen,storage_choose_ok,storage_scanned;
void sub_00024EC0(void) {
    CHECK(WORD_AT(g_esp+4)==0xffffffffu&&WORD_AT(g_esp+8)==0x190f70u&&WORD_AT(g_esp+12)==0);
    CHECK(WORD_AT(0x74f9fc)==0);storage_scanned++;g_esp+=4;
}
void sub_00192E60(void) {
    CHECK(WORD_AT(WORD_AT(g_esp+4)+0x18c)==0 && WORD_AT(0x7a1564)==0);
    storage_continued++;g_esp+=4;
}
void sub_00192770(void) { WORD_AT(0x7a1564)=WORD_AT(WORD_AT(g_esp+4)+0x18c);g_esp+=4; }
void sub_00191470(void) { CHECK(WORD_AT(WORD_AT(g_esp+8)+0x14)==0);g_esp+=4; }
static int picker_result,picker_polls,folder_chosen;
int pc_storage_picker_start(void){return 1;}
int pc_storage_picker_poll(char *out,size_t n){picker_polls++;if(picker_result>0)snprintf(out,n,"C:/test-save-folder");int result=picker_result;picker_result=0;return result;}
int pc_storage_folder(const char *path,char *error,size_t n){CHECK(!strcmp(path,"C:/test-save-folder"));folder_chosen++;if(!storage_choose_ok)snprintf(error,n,"Folder is not writable.");return storage_choose_ok;}
int pc_storage_locations(PcStorageDrive *d,int n){
    CHECK(n>=3);memset(d,0,3*sizeof *d);
    strcpy(d[0].name,"Current save folder");strcpy(d[1].name,"Game folder / Saves");strcpy(d[2].name,"Choose folder...");
    strcpy(d[0].target,"D:/Saves");strcpy(d[1].target,"C:/Game/Saves");d[0].current=1;
    for(int i=0;i<3;i++){d[i].available=(uint64_t)(i+1)*1024*1024*1024;d[i].total=d[i].available*2;d[i].known=1;}
    return 3;
}
int pc_storage_choose(const PcStorageDrive *d,char *e,size_t n){
    CHECK(d->target[0]);storage_chosen++;
    if(!storage_choose_ok)snprintf(e,n,"Test session: save location is locked.");
    return storage_choose_ok;
}
const char *const pc_action_labels[PC_ACTION_COUNT]={0};
static int capture_active,capture_begins,capture_ends;
void pc_settings_capture_begin(void) { capture_active=1;capture_begins++; }
void pc_settings_capture_end(void) { capture_active=0;capture_ends++; }
int pc_settings_capture_event(void) { CHECK(capture_active);return 0; }
void pc_settings_defaults(PcSettings *s) { (void)s;CHECK(0); }
int pc_settings_read(PcSettings *s,char *e,size_t n) { (void)s;(void)e;(void)n;CHECK(0);return 0; }
int pc_settings_save(const PcSettings *s,char *e,size_t n) { (void)s;(void)e;(void)n;CHECK(0);return 0; }
void pc_settings_key_name(int k,char *t,size_t n) { (void)k;(void)t;(void)n;CHECK(0); }
int pc_settings_conflict(const PcSettings *s,int a,int k) { (void)s;(void)a;(void)k;CHECK(0);return 0; }
int pc_settings_key_allowed(int k) { (void)k;CHECK(0);return 0; }
int pc_settings_assign(PcSettings *s,int a,int slot,int k,int move) { (void)s;(void)a;(void)slot;(void)k;(void)move;CHECK(0);return 0; }
void pc_controls_set_mouse(double s,int enabled,int invert) { (void)s;(void)enabled;(void)invert;CHECK(0); }
/* Native builder/centring contract: width is a percentage, not pixels.
 * An out-of-range width renders a panel with its rows outside the canvas. */
void sub_00075B70(void) {
    built_rows=built_row_count=separators=0;
    uint32_t word=WORD_AT(g_esp+12);float width;memcpy(&width,&word,4);
    float left=(100.f-width)*.5f;
    CHECK(isfinite(width) && width>0 && left>=0 && left+width<=100.f);
    g_esp+=4;
}
STUB(sub_00071B10) STUB(sub_00071C50)
STUB(sub_000771D0)
void sub_000770E0(void) {
    unsigned index=WORD_AT(g_esp+8);CHECK(index<built_row_count);
    WORD_AT(WORD_AT(g_esp+4)+0x18c)=built_ids[index];g_esp+=4;
}
void sub_00078980(void) {
    unsigned first=WORD_AT(0x200000+0xc8),second=WORD_AT(0x200200+0xc8),
        textures=WORD_AT(0x200e00+0xc8);build_menu();
    /* Native refresh restores choices by row ID, even across different pages. */
    WORD_AT(0x200000+0xc8)=first;WORD_AT(0x200200+0xc8)=second;
    WORD_AT(0x200e00+0xc8)=textures;
}
STUB(sub_0007D480) STUB(sub_000717D0)
void sub_00075430(void) { WORD_AT(WORD_AT(g_esp+4)+0xc8)=WORD_AT(g_esp+8);g_esp+=4; }
STUB(sub_0003D7B0) STUB(sub_001CD120) STUB(sub_00069720) STUB(sub_00071CF0)
STUB(sub_002C3E20) STUB(sub_002D9390) STUB(sub_00072510) STUB(sub_00022FF0) STUB(sub_0001C5D0) STUB(sub_00019650) STUB(sub_00019D70)
static unsigned highlight_rects,highlight_fallback;
static int highlight_bottom;
void sub_0001EFB0(void) {
    int left=(int)WORD_AT(g_esp+4),top=(int)WORD_AT(g_esp+8);
    int right=(int)WORD_AT(g_esp+12),bottom=(int)WORD_AT(g_esp+16);
    CHECK(top==highlight_bottom && bottom>top && left>=20 && right<=220);
    if(!highlight_rects)CHECK(left>20 && right<220);
    if(top==46)CHECK(left>20 && right<220);
    highlight_bottom=bottom;highlight_rects++;g_esp+=4;
}
static void original_highlight(void) { highlight_fallback++;g_esp+=4; }
void sub_00069640(void) { g_eax=0x400000;g_esp+=4; }
void sub_001CDAE0(void) { g_eax=1;g_esp+=4; }
void host_SetTwoDPlacement(int placement,uint32_t tag) { (void)placement;(void)tag; }
void sub_00079710(void) { separators++;g_esp+=4; }
STUB(sub_00074910) STUB(sub_001A0A60)
static float drawn;
void sub_000814B0(void) { uint32_t b=WORD_AT(g_esp+4);memcpy(&drawn,&b,4);g_esp+=4; }
void sub_00154FE0(void) { g_eax=WORD_AT(g_esp+4);g_esp+=4; }
void sub_00079B70(void) { returned++;g_esp+=4; }
void sub_00079D60_gen(void) {
    unsigned id=WORD_AT(g_esp+8);CHECK(id<32);
    if(page==DISPLAY_HOME && id>=20 && id<=21)CHECK(!strncmp((char *)(g_xbox_mem_offset+WORD_AT(g_esp+4)),"\vs75",4));
    built_rows|=1u<<id;built_ids[built_row_count++]=id;
    g_eax=0x200000+id*0x200;WORD_AT(g_eax+0x28)=WORD_AT(g_esp+12)|0x2000u;g_esp+=4;
}
static void call(uint32_t address,int selected,unsigned buttons)
{
    g_esp=0x700000;WORD_AT(g_esp)=0;WORD_AT(g_esp+4)=0x300000;
    WORD_AT(0x30018c)=selected;WORD_AT(0x3000c4)=buttons;
    g_esi=0xABC;g_ebx=0xDEF;g_fp_top=3;
    void (*fn)(void)=pc_display_menu_lookup(address); CHECK(fn);fn();
    CHECK(g_esp==0x700004 && g_esi==0xABC && g_ebx==0xDEF && g_fp_top==3);
}
static uint32_t observed_builder_flags,observed_row_flags,observed_beta_height;
static uint32_t observed_callbacks[3],observed_callback_count;
static void button_probe(void)
{
    if(observed_callback_count<3)observed_callbacks[observed_callback_count++]=WORD_AT(g_esp+20);
    if(WORD_AT(g_esp+8)==15)observed_beta_height=WORD_AT(g_esp+16);
    observed_builder_flags=WORD_AT(0x500028);
    observed_row_flags=WORD_AT(g_esp+12);
    g_eax=0x510000;g_esp+=4;
}
static void reload(const char *text)
{
    FILE *f=fopen(path,"wb");CHECK(f);CHECK(fputs(text,f)>=0);CHECK(!fclose(f));
    initialized=0;current=(PcDisplayMode){0,0};vertical_fov=PC_FOV_DEFAULT;
    pc_display_current();
}
static void content(const char *expected)
{
    FILE *f=fopen(path,"rb");char text[128]={0};CHECK(f);fread(text,1,sizeof text-1,f);fclose(f);
    CHECK(!strcmp(text,expected));
}
int main(int argc,char **argv)
{
    CHECK(argc==2); CHECK(!_putenv_s("TSFP_DISPLAY_CONFIG",argv[1]));
    snprintf(runtime_path,sizeof runtime_path,"%s.runtime",argv[1]);
    {FILE *f=fopen(runtime_path,"wb");CHECK(f);fputs("# preserve\nframe_interp = 3\nframe_cap = 30\nfuture_key = keep\n",f);fclose(f);}
    void *mem=VirtualAlloc(NULL,0x800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(mem);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)mem;
    { /* Descriptor +12 is a per-frame callback, not a destructor. */
        CHECK(kb_caption());kb_page=KB_CAPTURE;
        call(KB_BUILD,0,0);
        CHECK(!(WORD_AT(0x200028)&5u)); /* Selectable instruction keeps ticks active. */
        kb_capture_pending=1;
        for(int frame=0;frame<2;frame++) {
            call(WORD_AT(kb_storage+4),0,0);
            if(WORD_AT(kb_storage+12))call(WORD_AT(kb_storage+12),0,0);
            CHECK(capture_active && capture_begins==1 && capture_ends==0);
        }
        pc_settings_capture_end();kb_page=KB_HOME;
    }
    { /* Original and explicit 640x480 share dimensions but not saved identity. */
        PcDisplayMode choices[]={{0,0},{640,480}},filtered[2];int selected;
        CHECK(pc_display_filter_modes(choices,2,0,choices[0],filtered,2,&selected)==2 && selected==0);
        CHECK(pc_display_filter_modes(choices,2,0,choices[1],filtered,2,&selected)==2 && selected==1);
    }
    pc_display_current();reload("TSFP_DISPLAY_V1 1280 720\n");
    CHECK(pc_display_fov()==55 && pc_display_fov_scale()==1.f);
    CHECK(!pc_display_set_fov(39) && !pc_display_set_fov(101));
    CHECK(pc_display_set_fov(75) && pc_display_save());content("TSFP_DISPLAY_V2 1280 720 75\n");
    initialized=0;vertical_fov=55;CHECK(pc_display_fov()==75);
    CHECK(fabsf(pc_display_fov_scale()-tanf(75*.00872664626f)/tanf(55*.00872664626f))<.00001f);
    reload("TSFP_DISPLAY_V2 1280 720 999\n");CHECK(pc_display_fov()==55 && !current.width);
    reload("TSFP_DISPLAY_V2 1280 720\n");CHECK(pc_display_fov()==55 && !current.width);
    reload("TSFP_DISPLAY_V1 1280 720 75\n");CHECK(pc_display_fov()==55 && !current.width);
    reload("TSFP_DISPLAY_V1 1280 720\n");
    tsfp_texture_set_4k(0);
    call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    CHECK(page==DISPLAY_HOME && built_row_count==7 && separators==0);
    { /* Shared focus bars stay rounded regardless of the menu descriptor. */
        float bounds[4]={20,20,220,47};
        WORD_AT(current_menu+4)=storage;WORD_AT(current_menu+0x20)=0x310000;
        WORD_AT(current_menu+0xf4)=bits(1.f);
        memcpy((void *)(g_xbox_mem_offset+0x320000),bounds,sizeof bounds);
        g_esp=0x700000;WORD_AT(g_esp+4)=current_menu;WORD_AT(g_esp+12)=0x320000;
        highlight_bottom=20;pc_display_menu_highlight(original_highlight);
        CHECK(g_esp==0x700004 && highlight_rects==13 && highlight_bottom==47 && !highlight_fallback);
        WORD_AT(current_menu+4)=0;g_esp=0x700000;
        highlight_bottom=20;highlight_rects=0;
        pc_display_menu_highlight(original_highlight);
        CHECK(g_esp==0x700004 && highlight_rects==13 && highlight_bottom==47 && !highlight_fallback);
        bounds[3]=25;memcpy((void *)(g_xbox_mem_offset+0x320000),bounds,sizeof bounds);
        g_esp=0x700000;pc_display_menu_highlight(original_highlight);
        CHECK(g_esp==0x700004 && highlight_fallback==1 && highlight_rects==13);
    }

    CHECK(!(built_rows&(1u<<5)) && !(built_rows&(1u<<4))); /* No Back or reset row. */
    CHECK(!(built_rows&(1u<<20))); /* FOV has no default-value helper row. */
    CHECK(!display_dirty() && (WORD_AT(apply_row+0x28)&1));
    CHECK(built_rows&(1u<<9));
    WORD_AT(subtitle_row+0xc8)=1;call(TICK_CB,9,0);
    CHECK(display_dirty() && !WORD_AT(0x400130) && !WORD_AT(0x7613dc));
    call(SELECT_CB,3,0);
    CHECK(WORD_AT(0x400130)==1 && WORD_AT(0x7613dc)==1 && (WORD_AT(0x78ad24)&0x10000));
    CHECK(!display_dirty() && (WORD_AT(apply_row+0x28)&1));

    call(SELECT_CB,3,0);CHECK(requested_rate==-1 && pc_frame_rate_read()==-1);
    for(unsigned a=1;a<PC_DISPLAY_ASPECT_COUNT;a++) {
        WORD_AT(aspect_row+0xc8)=a;call(TICK_CB,0,0);
        CHECK(selected_aspect==(int)a && mode_count>0);
        CHECK(WORD_AT(resolution_row+0xc8)==(unsigned)selected_mode);
        for(int i=0;i<mode_count;i++)CHECK(pc_display_aspect(modes[i])==(int)a);
        if(a==PC_DISPLAY_ASPECT_COUNT-1) {
            CHECK(mode_count==1 && pc_display_is_auto(staged));
            CHECK(!strcmp((char *)(g_xbox_mem_offset+TEXT_ADDRESS(MODE_TEXT_BASE)),"Auto"));
            CHECK(!pc_display_is_auto(pc_display_current()) && display_dirty());
        }
    }
    CHECK(current.width==1280 && current.height==720);
    /* Back key discards all staged settings without a dedicated Back row. */
    WORD_AT(texture_row+0xc8)=1;WORD_AT(rate_row+0xc8)=1;
    for(int i=0;i<20;i++)call(TICK_CB,2,0x80000);
    CHECK(staged_fov==75 && pc_display_fov()==55 && !tsfp_texture_is_4k());
    CHECK(display_dirty() && !(WORD_AT(apply_row+0x28)&1));
    int left=returned;call(BACK_CB,0,0);CHECK(returned==left+1);
    call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    CHECK(!display_dirty() && staged_fov==55 && staged_rate==0 && texture_choice==0);
    content("TSFP_DISPLAY_V1 1280 720\n");
    for(int i=0;i<100;i++)call(TICK_CB,2,0x80000);CHECK(staged_fov==100 && pc_display_fov()==55);
    for(int i=0;i<100;i++)call(TICK_CB,2,0x40000);CHECK(staged_fov==40);
    WORD_AT(0x70000c)=0x400000;
    {float rectangle[4]={0,0,400,20};memcpy((void *)(g_xbox_mem_offset+0x400000),rectangle,sizeof rectangle);}
    call(FOV_DRAW_CB,1,0);CHECK(drawn==0.f);
    staged_fov=100;call(FOV_DRAW_CB,1,0);CHECK(drawn==1.f);
    slider_capture_enabled=1;
    WORD_AT(0x700008)=0x200400;WORD_AT(0x200414)=2;WORD_AT(0x200488)=0x300000;
    WORD_AT(0x300198)=0x200400;
    FLOAT_AT(0x3000e4)=0;FLOAT_AT(0x3000e8)=0;FLOAT_AT(0x3000ec)=640;FLOAT_AT(0x3000f0)=480;
    call(FOV_DRAW_CB,2,0);CHECK(drawn==1.f);
    MenuSliderTrack *track=&menu_slider_tracks[MENU_FOV];
    CHECK(track->x==298 && track->y==6 && track->w==100 && track->h==8);
    PcMenuPointer pointer={0,0,1920,1080,1,1,0,0};memset(&menu_mouse,0,sizeof menu_mouse);
    CHECK(menu_slider_pointer(0x300000,pointer,2,348,8,1920,1080));
    CHECK(staged_fov==70 && pc_display_fov()==55);
    pointer.left=0;CHECK(!menu_slider_pointer(0x300000,pointer,0,348,8,1920,1080));slider_capture_enabled=0;
    WORD_AT(texture_row+0xc8)=1;WORD_AT(rate_row+0xc8)=1;
    call(TICK_CB,8,0);CHECK(!tsfp_texture_is_4k() && requested_rate==-1);
    call(SELECT_CB,3,0);
    CHECK(page==DISPLAY_HOME && pc_display_fov()==70 && tsfp_texture_is_4k());
    CHECK(requested_rate==120 && pc_frame_rate_read()==120);
    CHECK(!display_dirty() && (WORD_AT(apply_row+0x28)&1));
    content("TSFP_DISPLAY_V2 1280 720 70\n");
    call(TICK_CB,2,0x80000);CHECK(!(WORD_AT(apply_row+0x28)&1));
    call(TICK_CB,2,0x40000);CHECK(WORD_AT(apply_row+0x28)&1);
    /* Failed display save must not publish FPS or texture changes. */
    staged_fov=75;WORD_AT(rate_row+0xc8)=2;WORD_AT(texture_row+0xc8)=0;WORD_AT(subtitle_row+0xc8)=0;
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));call(SELECT_CB,3,0);
    CHECK(pc_display_fov()==70 && requested_rate==120 && tsfp_texture_is_4k() && WORD_AT(0x400130)==1);
    CHECK(display_dirty() && !(WORD_AT(apply_row+0x28)&1) && *status);
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));
    char rate_path[32768];CHECK(pc_frame_rate_path(rate_path,sizeof rate_path));
    /* If the second save fails, restore the first file and all live settings. */
    CHECK(SetFileAttributesA(rate_path,FILE_ATTRIBUTE_READONLY));call(SELECT_CB,3,0);
    CHECK(pc_display_fov()==70 && requested_rate==120 && tsfp_texture_is_4k() && WORD_AT(0x400130)==1);
    content("TSFP_DISPLAY_V2 1280 720 70\n");CHECK(pc_frame_rate_read()==120);
    CHECK(SetFileAttributesA(rate_path,FILE_ATTRIBUTE_NORMAL));call(SELECT_CB,3,0);
    CHECK(pc_display_fov()==75 && requested_rate==180 && !tsfp_texture_is_4k() && !WORD_AT(0x400130));
    CHECK(!display_dirty() && (WORD_AT(apply_row+0x28)&1));
    /* Resolution preview defers the complete batch until Keep. */
    resolution_row=0;staged=(PcDisplayMode){1920,1080};staged_fov=80;
    WORD_AT(rate_row+0xc8)=0;WORD_AT(texture_row+0xc8)=1;call(SELECT_CB,3,0);
    CHECK(page==DISPLAY_CONFIRM && current.width==1920 && pc_display_fov()==75);
    CHECK(requested_rate==180 && !tsfp_texture_is_4k());call(BACK_CB,0,0);
    CHECK(page==DISPLAY_HOME && current.width==1280 && pc_display_fov()==75 && display_dirty());
    resolution_row=0;staged=(PcDisplayMode){1920,1080};call(SELECT_CB,3,0);
    clock_ms=16000;pc_display_frame_done();call(TICK_CB,0,0);
    CHECK(page==DISPLAY_HOME && current.width==1280 && pc_display_fov()==75 && requested_rate==180);
    resolution_row=0;staged=(PcDisplayMode){1920,1080};call(SELECT_CB,3,0);call(SELECT_CB,0,0);
    CHECK(page==DISPLAY_HOME && current.width==1920 && pc_display_fov()==80);
    CHECK(requested_rate==0 && tsfp_texture_is_4k() && !display_dirty());
    CHECK(WORD_AT(apply_row+0x28)&1);content("TSFP_DISPLAY_V2 1920 1080 80\n");
    /* External F10 follows the model unless this menu has an unapplied choice. */
    tsfp_texture_toggle();call(TICK_CB,7,0);
    CHECK(!tsfp_texture_is_4k() && texture_choice==0 && !display_dirty());
    WORD_AT(texture_row+0xc8)=1;call(TICK_CB,7,0);CHECK(!tsfp_texture_is_4k() && display_dirty());
    left=returned;call(BACK_CB,0,0);CHECK(returned==left+1);
    /* Display/FOV edits must leave unrelated runtime preferences untouched. */
    {
        char text[256]={0};FILE *f=fopen(runtime_path,"rb");CHECK(f);
        fread(text,1,sizeof text-1,f);CHECK(!ferror(f) && feof(f));CHECK(!fclose(f));
        CHECK(!strcmp(text,"# preserve\nframe_interp = 3\nframe_cap = 30\nfuture_key = keep\n"));
    }
    /* Audio entry inherits layout before construction; the invisible native
     * subtitle record remains available to its original tick/save routines. */
    WORD_AT(0x7bb1e0)=0x500000;WORD_AT(0x500028)=0x32200;
    g_esp=0x700000;WORD_AT(g_esp+20)=0x2cbe10;WORD_AT(g_esp+4)=0x510000;
    pc_display_menu_button(button_probe);
    CHECK(observed_builder_flags==0x26200 && WORD_AT(0x500028)==0x32200 && g_esp==0x700004);
    /* Both native insertion sites put Video immediately before Controls,
     * preserving the Controls call frame and its result. */
    for(unsigned i=0;i<2;i++) {
        uint32_t callback=i?0x2ec1a0u:0x2cbe50u;
        g_esp=0x700000;WORD_AT(g_esp)=i?0x2f561cu:0x2cc116u;
        WORD_AT(g_esp+4)=0x510000;WORD_AT(g_esp+20)=callback;
        observed_callback_count=0;
        pc_display_menu_button(button_probe);
        CHECK(observed_callback_count==3 && observed_callbacks[0]==PC_DISPLAY_MENU_CALLBACK);
        CHECK(observed_callbacks[1]==KB_OPEN && observed_callbacks[2]==callback && WORD_AT(0x700014)==callback);
        CHECK(WORD_AT(0x700004)==0x510000 && g_esp==0x700004 && g_eax==0x510000);
    }
    /* Pause Options supplies its own centered (or split-screen) style.
     * The Audio rename must retain it, including caller-provided disable bits. */
    for(unsigned i=0;i<2;i++) {
        uint32_t style=i?0x32000u:0x32200u;
        WORD_AT(0x500028)=style;
        g_esp=0x700000;WORD_AT(g_esp)=0x2f5666;
        WORD_AT(g_esp+20)=0x2ec1f0;WORD_AT(g_esp+4)=0x510000;
        WORD_AT(g_esp+12)=1u;
        pc_display_menu_button(button_probe);
        CHECK(observed_builder_flags==style && WORD_AT(0x500028)==style);
        CHECK(observed_row_flags==1u && WORD_AT(0x700004)==0x510000 && g_esp==0x700004);
    }
    g_esp=0x700000;WORD_AT(g_esp)=0x2c4042;WORD_AT(g_esp+20)=0;WORD_AT(g_esp+12)=0x20000;WORD_AT(g_esp+16)=bits(54.f);
    pc_display_menu_button(button_probe);
    CHECK((observed_row_flags&0x21)==0x21 && WORD_AT(0x70000c)==0x20000 && g_esp==0x700004);
    CHECK(audio_beta_row==0x510000 && observed_beta_height==bits(54.f));
    WORD_AT(audio_beta_row+0xc8)=1;call(AUDIO_BETA_CHANGE,0,0);CHECK(enhanced_audio==1);
    audio_save_ok=0;WORD_AT(audio_beta_row+0xc8)=0;call(AUDIO_BETA_CHANGE,0,0);
    CHECK(enhanced_audio==1 && WORD_AT(audio_beta_row+0xc8)==1);
    audio_save_ok=1;WORD_AT(audio_beta_row+0xc8)=0;call(AUDIO_BETA_CHANGE,0,0);CHECK(enhanced_audio==0);
    /* Drives use alphabetical letters/names, independent of the active drive,
       and keep host IDs away from the console device table. */
    {
        int opened=returned;built_rows=built_row_count=0;
        g_esp=0x700000;WORD_AT(g_esp)=0x1944c3;
        WORD_AT(g_esp+4)=0x510000;WORD_AT(g_esp+8)=0;
        WORD_AT(g_esp+12)=0x24c00;WORD_AT(g_esp+16)=0;
        WORD_AT(g_esp+20)=0x192e60;WORD_AT(g_esp+24)=0x191470;
        pc_display_menu_button(sub_00079D60_gen);
        CHECK(built_row_count==3 && built_ids[0]==0 && built_ids[1]==1 && built_ids[2]==2);
        CHECK(returned==opened && g_esp==0x700004 && g_eax==0x200000);
        CHECK(WORD_AT(0x700004)==0x510000 && WORD_AT(0x700014)==0x192e60 && WORD_AT(0x700018)==0x191470);
        CHECK(!strcmp((char *)(g_xbox_mem_offset+SD_TEXT(0)),"\vs85Current save folder\vs100"));
        CHECK(strstr((char *)(g_xbox_mem_offset+SD_TEXT(1)),"Game folder / Saves"));
        CHECK(strstr((char *)(g_xbox_mem_offset+SD_TEXT(2)),"Choose folder..."));
        WORD_AT(0x300038)=0;WORD_AT(0x74f9ec)=6;
        call(0x192770,1,0);CHECK(WORD_AT(0x7a1564)==0 && WORD_AT(0x30018c)==1);
        CHECK(strstr((char *)(g_xbox_mem_offset+SD_TEXT(SD_DETAILS)),"2.0 GiB free / 4.0 GiB total"));
        call(SD_SELECT,1,0);CHECK(storage_chosen==1 && !storage_continued && returned==opened);
        CHECK(strstr((char *)(g_xbox_mem_offset+SD_TEXT(SD_DETAILS)),"locked"));
        storage_choose_ok=1;call(SD_SELECT,1,0);
        CHECK(storage_chosen==2 && storage_continued==1 && storage_scanned==1 && sd_drives[1].current && returned==opened);
        call(SD_SELECT,1,0);CHECK(storage_continued==2 && storage_chosen==2);
        call(SD_SELECT,0,0);CHECK(storage_continued==3 && storage_chosen==3 && sd_drives[0].current && returned==opened);
        WORD_AT(0x700008)=0x200200;WORD_AT(0x200214)=2;call(SD_DRAW,2,0);
        CHECK(WORD_AT(0x200214)==2);
        call(SD_SELECT,2,0);CHECK(sd_picker_menu==0x300000);
        picker_result=1;int continued=storage_continued;
        /* A second profile ticks first; its state must not consume the result. */
        g_esp=0x700000;WORD_AT(g_esp+4)=0x320000;WORD_AT(0x320038)=1;WORD_AT(0x74f9f0)=6;
        sd_profile_tick();CHECK(picker_result==1 && picker_polls==0 && folder_chosen==0);
        call(0x192770,2,0);CHECK(picker_polls==1 && folder_chosen==1 && storage_continued==continued+1 && !sd_picker_menu);
        call(SD_SELECT,2,0);picker_result=-1;call(0x192770,2,0);CHECK(folder_chosen==1 && !sd_picker_menu);
        call(SD_SELECT,2,0);picker_result=1;storage_choose_ok=0;call(0x192770,2,0);
        CHECK(strstr(sd_notice,"not writable") && storage_continued==continued+1);
        WORD_AT(0x74f9ec)=7;call(0x192770,3,0);CHECK(WORD_AT(0x7a1564)==3);
    }
    VirtualFree(mem,0,MEM_RELEASE);
    puts("PASS: unified display, disabled Apply, staged batch, FOV bounds/mouse, Back discard, failed-save rollback, resolution confirmation/timeout, texture synchronization and guest ABI");
    return 0;
}
