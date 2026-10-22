/* Production display model and guest callbacks with isolated settings and RAM.
 * Original menu drawing/input functions are stubs; no game data is needed. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static ULONGLONG clock_ms;
static ULONGLONG ticks(void) { return clock_ms; }
#define GetTickCount64 ticks
#include "../src/pc_display.c"
#include "../src/pc_display_menu.c"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
static int available=1,returned;
static unsigned built_rows, built_row_count;
static char runtime_path[MAX_PATH];
uint32_t recomp_config_title_id(void) { return 0x45410066; }
int recomp_settings_path(uint32_t title,char *out,size_t size)
{ (void)title;return snprintf(out,size,"%s",runtime_path)>0; }
static int slider_capture_enabled;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
HWND xbox_D3D8Window(void) { return NULL; }
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) { (void)w;(void)h;return available; }
int pc_online_connect(void) { return 0; }
int pc_online_connect_mode(PcOnlineMode mode) { (void)mode;return 0; }
PcOnlineMode pc_online_mode(void) { return PC_ONLINE_NONE; }
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
/* Native builder/centring contract: width is a percentage, not pixels.
 * An out-of-range width renders a panel with its rows outside the canvas. */
void sub_00075B70(void) {
    built_rows=built_row_count=0;
    uint32_t word=WORD_AT(g_esp+12);float width;memcpy(&width,&word,4);
    float left=(100.f-width)*.5f;
    CHECK(isfinite(width) && width>0 && left>=0 && left+width<=100.f);
    g_esp+=4;
}
STUB(sub_00071B10) STUB(sub_00071C50)
STUB(sub_000771D0) STUB(sub_000770E0)
void sub_00078980(void) {
    unsigned first=WORD_AT(0x200000+0xc8),second=WORD_AT(0x200200+0xc8),
        textures=WORD_AT(0x200e00+0xc8);build_menu();
    /* Native refresh restores choices by row ID, even across different pages. */
    WORD_AT(0x200000+0xc8)=first;WORD_AT(0x200200+0xc8)=second;
    WORD_AT(0x200e00+0xc8)=textures;
}
STUB(sub_0007D480) STUB(sub_000717D0)
void sub_00075430(void) { WORD_AT(WORD_AT(g_esp+4)+0xc8)=WORD_AT(g_esp+8);g_esp+=4; }
STUB(sub_00022FF0) STUB(sub_0001C5D0) STUB(sub_0001EFB0) STUB(sub_00019650) STUB(sub_00019D70)
void sub_001CDAE0(void) { g_eax=1;g_esp+=4; }
void host_SetTwoDPlacement(int placement,uint32_t tag) { (void)placement;(void)tag; }
STUB(sub_00079710) STUB(sub_00074910) STUB(sub_001A0A60) STUB(sub_002D2C70)
  STUB(sub_001A8D30) STUB(sub_001A8F30) STUB(sub_002CB090) STUB(sub_002CAFE0) STUB(sub_002D62F0)
STUB(sub_00197890) STUB(sub_002CA9F0) STUB(sub_00359EB0) STUB(sub_00198E20) STUB(sub_0003D7B0) STUB(sub_00040F00) STUB(sub_00041DC0)
static float drawn;
void sub_000814B0(void) { uint32_t b=WORD_AT(g_esp+4);memcpy(&drawn,&b,4);g_esp+=4; }
void sub_00154FE0(void) { g_eax=WORD_AT(g_esp+4);g_esp+=4; }
void sub_00079B70(void) { returned++;g_esp+=4; }
void sub_00079D60_gen(void) {
    unsigned id=WORD_AT(g_esp+8);CHECK(id<32);
    built_rows|=1u<<id;++built_row_count;
    g_eax=0x200000+id*0x200;g_esp+=4;
}
static void call(uint32_t address,int selected,unsigned buttons)
{
    g_esp=0x700000;WORD_AT(g_esp)=0;WORD_AT(g_esp+4)=0x300000;
    WORD_AT(0x30018c)=selected;WORD_AT(0x3000c4)=buttons;
    g_esi=0xABC;g_ebx=0xDEF;g_fp_top=3;
    void (*fn)(void)=pc_display_menu_lookup(address); CHECK(fn);fn();
    CHECK(g_esp==0x700004 && g_esi==0xABC && g_ebx==0xDEF && g_fp_top==3);
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
    call(PC_DISPLAY_MENU_CALLBACK,0,0);CHECK(staged_fov==55);
    call(BUILD_CB,0,0);
    CHECK(!strcmp((char *)(g_xbox_mem_offset+TEXT_ADDRESS(7)),"Aspect ratio"));
    /* Aspect, resolution, FOV, textures, Apply, Reset and Back only.
     * The retired Smoothness row (6) must not be offered. */
    CHECK(built_row_count==7 && built_rows==0xbf);
    CHECK(!strcmp((char *)(g_xbox_mem_offset+TEXT_ADDRESS(2)),"Textures"));
    CHECK(!strcmp((char *)(g_xbox_mem_offset+TEXT_ADDRESS(3)),"Classic"));
    CHECK(!strcmp((char *)(g_xbox_mem_offset+TEXT_ADDRESS(10)),"Enhanced"));
    CHECK(texture_row==0x200e00);
    /* Actual native choice arrows change the live mode, independently of
       resolution Apply. Host toggles cannot be undone by an unchanged row. */
    tsfp_texture_set_4k(0);call(TICK_CB,7,0);CHECK(WORD_AT(texture_row+0xc8)==0);
    WORD_AT(texture_row+0xc8)=1;call(TICK_CB,7,0);
    CHECK(tsfp_texture_is_4k() && texture_choice==1);
    content("TSFP_DISPLAY_V1 1280 720\n");
    tsfp_texture_toggle();call(TICK_CB,7,0);
    CHECK(!tsfp_texture_is_4k() && WORD_AT(texture_row+0xc8)==0);
    tsfp_texture_toggle();call(TICK_CB,0,0);
    CHECK(tsfp_texture_is_4k() && WORD_AT(texture_row+0xc8)==1);
    WORD_AT(texture_row+0xc8)=99;call(TICK_CB,7,0);
    CHECK(tsfp_texture_is_4k() && WORD_AT(texture_row+0xc8)==1);
    WORD_AT(texture_row+0xc8)=0;tsfp_texture_set_4k(1);refresh(0,0);
    CHECK(WORD_AT(texture_row+0xc8)==1); /* refresh's stale row restore */
    call(BACK_CB,0,0);call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    CHECK(tsfp_texture_is_4k() && WORD_AT(texture_row+0xc8)==1);
    WORD_AT(texture_row+0xc8)=0;call(TICK_CB,7,0);CHECK(!tsfp_texture_is_4k());
    for(unsigned a=1;a<PC_DISPLAY_ASPECT_COUNT;a++) {
        WORD_AT(aspect_row+0xc8)=a;call(TICK_CB,0,0);
        CHECK(selected_aspect==(int)a && mode_count>0);
        CHECK(WORD_AT(resolution_row+0xc8)==(unsigned)selected_mode);
        CHECK(pc_display_aspect(staged)==(int)a);
        for(int i=0;i<mode_count;i++)CHECK(pc_display_aspect(modes[i])==(int)a);
    }
    /* Group selection is a draft, never a desktop mode change or implicit save. */
    CHECK(current.width==1280 && current.height==720);
    content("TSFP_DISPLAY_V1 1280 720\n");
    call(BACK_CB,0,0);call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    for(int i=0;i<100;i++)call(TICK_CB,2,0x80000);CHECK(staged_fov==100 && pc_display_fov()==55);
    for(int i=0;i<100;i++)call(TICK_CB,2,0x40000);CHECK(staged_fov==40);
    WORD_AT(0x70000c)=0x400000;
    { float rectangle[4]={0,0,400,20};memcpy((void *)(g_xbox_mem_offset+0x400000),rectangle,sizeof rectangle); }
    call(FOV_DRAW_CB,1,0);CHECK(drawn==0.f);
    staged_fov=100;call(FOV_DRAW_CB,1,0);CHECK(drawn==1.f);
    /* The production FOV painter registers its final bar; dragging changes
     * only the draft and label. Back discards it; Apply below saves it. */
    slider_capture_enabled=1;
    WORD_AT(0x700008)=0x200400;WORD_AT(0x200414)=2;WORD_AT(0x200488)=0x300000;
    WORD_AT(0x300198)=0x200400;
    FLOAT_AT(0x3000e4)=0;FLOAT_AT(0x3000e8)=0;
    FLOAT_AT(0x3000ec)=640;FLOAT_AT(0x3000f0)=480;
    call(FOV_DRAW_CB,2,0);CHECK(drawn==1.f);
    MenuSliderTrack *fov_track=&menu_slider_tracks[MENU_FOV];
    CHECK(fov_track->x==298 && fov_track->y==6 && fov_track->w==100 && fov_track->h==8);
    PcMenuPointer p={0,0,1920,1080,1,1,0,0};
    memset(&menu_mouse,0,sizeof menu_mouse);
    CHECK(menu_slider_pointer(0x300000,p,2,348,8,1920,1080));
    CHECK(staged_fov==70 && pc_display_fov()==55);
    CHECK(!strcmp((char *)(g_xbox_mem_offset+TEXT_ADDRESS(8)),"Vertical FOV: 70"));
    content("TSFP_DISPLAY_V1 1280 720\n");
    p.left=0;CHECK(!menu_slider_pointer(0x300000,p,0,348,8,1920,1080));
    slider_capture_enabled=0;
    /* Leaving discards the draft. No write on slider movement. */
    call(BACK_CB,1,0);content("TSFP_DISPLAY_V1 1280 720\n");
    call(PC_DISPLAY_MENU_CALLBACK,0,0);CHECK(staged_fov==55);
    for(int i=0;i<20;i++)call(TICK_CB,2,0x80000);
    call(SELECT_CB,3,0);CHECK(pc_display_fov()==75);content("TSFP_DISPLAY_V2 1280 720 75\n");
    call(SELECT_CB,4,0);CHECK(staged_fov==55 && pc_display_fov()==75);
    /* Combined resolution/FOV preview rolls both back, including global exit. */
    resolution_row=0;staged=(PcDisplayMode){1920,1080};
    call(SELECT_CB,3,0);CHECK(page==1 && pc_display_fov()==55);
    call(BACK_CB,0,0);CHECK(page==0 && current.width==1280 && pc_display_fov()==75);
    resolution_row=0;staged=(PcDisplayMode){1920,1080};staged_fov=80;
    call(SELECT_CB,3,0);clock_ms=16000;pc_display_frame_done();
    CHECK(current.width==1280 && pc_display_fov()==75);content("TSFP_DISPLAY_V2 1280 720 75\n");
    call(TICK_CB,0,0);CHECK(page==0 && staged_fov==75);
    resolution_row=0;staged=(PcDisplayMode){1920,1080};staged_fov=90;
    call(SELECT_CB,3,0);call(SELECT_CB,0,0);
    CHECK(page==0 && current.width==1920 && pc_display_fov()==90);content("TSFP_DISPLAY_V2 1920 1080 90\n");
    resolution_row=0;staged=current;staged_fov=80;CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_READONLY));
    call(SELECT_CB,3,0);CHECK(pc_display_fov()==90 && page==2);
    CHECK(SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL));content("TSFP_DISPLAY_V2 1920 1080 90\n");
    CHECK(pc_display_set_fov(55) && pc_display_save());content("TSFP_DISPLAY_V1 1920 1080\n");
    /* Returning from feedback rebuilds the same seven-row menu. Back still
     * exits normally after removal of the Smoothness submenu. */
    call(BACK_CB,0,0);CHECK(page==0 && built_row_count==7 && built_rows==0xbf);
    {int before=returned;call(SELECT_CB,5,0);CHECK(returned==before+1);}
    content("TSFP_DISPLAY_V1 1920 1080\n");
    /* Display/FOV edits must leave unrelated runtime preferences untouched. */
    {
        char text[256]={0};FILE *f=fopen(runtime_path,"rb");CHECK(f);
        fread(text,1,sizeof text-1,f);CHECK(!ferror(f) && feof(f));CHECK(!fclose(f));
        CHECK(!strcmp(text,"# preserve\nframe_interp = 3\nframe_cap = 30\nfuture_key = keep\n"));
    }
    VirtualFree(mem,0,MEM_RELEASE);
    puts("PASS: FOV config, bounds, staging, reset, cancellation, atomic failure, preview rollback, keep, live texture choice/synchronization and guest ABI");
    return 0;
}
