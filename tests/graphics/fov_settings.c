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
const char *pc_build_version(void) { return "development"; }
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
#include "graphics/graphics_options.h"
static TsfpGraphicsOptions graphics_options={1,0,1,0,1,0};
static int graphics_save_ok=1;
static int cache_clear_calls;
void host_AmbientOcclusion(const float *projection){(void)projection;}
int pc_graphics_clear_cache(void){cache_clear_calls++;return 4;}
TsfpGraphicsOptions pc_graphics_get(void){return graphics_options;}
int pc_graphics_set(const TsfpGraphicsOptions *o){if(!graphics_save_ok)return 0;graphics_options=*o;return 1;}
static int counter_enabled,counter_save_ok=1;
int pc_counter_enabled(void) { return counter_enabled; }
int pc_counter_set_enabled(int value) { if(!counter_save_ok)return 0;counter_enabled=!!value;return 1; }
static int smaa_preference,smaa_save_ok=1;
int pc_smaa_enabled(void){return smaa_preference;}
int pc_smaa_set_enabled(int enabled){if(!smaa_save_ok)return 0;smaa_preference=enabled;return 1;}
static unsigned built_rows, built_row_count, built_ids[32],separators,separator_positions[2];
static uint32_t last_built_item;
static void fixture_link(uint32_t item) {WORD_AT(item+4)=0;WORD_AT(item+8)=0;if(last_built_item){WORD_AT(last_built_item+4)=item;WORD_AT(last_built_item+8)=item;}else WORD_AT(0x300178)=item;last_built_item=item;}
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
int pc_online_connect_mode(PcOnlineMode mode) { (void)mode;return 0; }
PcOnlineMode pc_online_mode(void) { return PC_ONLINE_NONE; }
static unsigned online_disconnects,lan_browser_entries;
void pc_online_disconnect(void) { online_disconnects++; }
uint32_t xbox_HeapAlloc(uint32_t size,uint32_t alignment) {static uint32_t next=0x100000;next=(next+alignment-1)&~(alignment-1);uint32_t result=next;next+=size;CHECK(next<0x1f0000);return result;}
#define STUB(name) void name(void) { g_esp+=4; }
/* The menu translation unit also contains tutorial hooks. Their drawing and
 * input behavior is covered by tutorial_layout.c; these paths must not run
 * during the FOV/settings test. */
static int settings_keyboard=1,settings_pad_allowed;
static unsigned char settings_keys[256];
int pc_prompts_keyboard_active(void) { return settings_keyboard; }
void sub_000402D0(void) {CHECK(0);}
void sub_0001A2E0(void) {CHECK(0);}
int pc_prompts_keycap_label(const char *label,size_t length) {(void)label;(void)length;CHECK(0);return 0;}
size_t pc_prompts_read_region(uintptr_t address) {(void)address;CHECK(0);return 0;}
int pc_controls_accept_pad(void) {return settings_pad_allowed;}
int pc_controls_binding_keys(unsigned char keys[256]) {memcpy(keys,settings_keys,256);return 1;}
int pc_controls_key_down(int key) {return key>0 && key<256?settings_keys[key]:0;}
void pc_prompts_layout_begin(void) {}
void pc_prompts_layout_end(void) {}
#define TUTORIAL_STUB(name) void name(void) { CHECK(0); }
TUTORIAL_STUB(sub_00147080) TUTORIAL_STUB(sub_00042D10)
TUTORIAL_STUB(sub_0003FA10) TUTORIAL_STUB(sub_0003FCF0) TUTORIAL_STUB(sub_0003F8D0)
/* Kill-feed drawing is linked through the same menu unit, but must not
 * execute in this settings-only fixture. */
TUTORIAL_STUB(sub_00041E30) TUTORIAL_STUB(sub_00021450)
TUTORIAL_STUB(sub_0001ED50) TUTORIAL_STUB(sub_0001F090)
#undef TUTORIAL_STUB
/* Mouse-menu behavior is exercised by menu_mouse.c. Keep its dependencies
 * linked here, but fail if this settings-only fixture enters those paths. */
void pc_controls_text_mode(uint32_t owner) {(void)owner;CHECK(0);}
unsigned pc_controls_text_event(uint32_t owner) {(void)owner;CHECK(0);return 0;}
void pc_controls_menu_mode(int active) { (void)active; CHECK(0); }
PcMenuPointer pc_controls_menu_pointer(void) { CHECK(0); return (PcMenuPointer){0}; }
static int tab_pointer_enabled;
static PcMenuPointer tab_pointer;
PcMenuPointer pc_controls_menu_hover_pointer(void) { CHECK(tab_pointer_enabled);return tab_pointer; }
void d3d8_display_output_shape(unsigned w,unsigned h,unsigned *ow,unsigned *oh)
{ (void)w;(void)h;CHECK(slider_capture_enabled);*ow=1920;*oh=1080; }
#define MOUSE_STUB(name) void name(void) { CHECK(0); }
void sub_00074760(void) {CHECK(tab_pointer_enabled);g_eax=1;g_esp+=4;}
MOUSE_STUB(sub_00076EF0) MOUSE_STUB(sub_00076510)
MOUSE_STUB(sub_002E6D40) MOUSE_STUB(sub_00079860)
MOUSE_STUB(sub_002C4310)
MOUSE_STUB(sub_001922D0)
MOUSE_STUB(sub_002E49F0) MOUSE_STUB(sub_002E4A30)
/* Audio-slider preview calls are covered by menu_mouse.c, not this fixture. */
MOUSE_STUB(sub_00045C60)
static unsigned video_sounds,highlight_sounds;
void sub_00021780(void){uint32_t sound=WORD_AT(g_esp+4);CHECK(sound==0x1234u || sound==0x4321u);if(sound==0x4321u)highlight_sounds++;else video_sounds++;g_esp+=4;}
MOUSE_STUB(sub_00045390) MOUSE_STUB(sub_00045820)
MOUSE_STUB(sub_0007DB20) MOUSE_STUB(sub_0007C410) MOUSE_STUB(sub_0007C300)
MOUSE_STUB(sub_0007C390) MOUSE_STUB(sub_0007C4F0) MOUSE_STUB(sub_0007CDE0)
MOUSE_STUB(sub_000746D0) MOUSE_STUB(sub_0007CCA0) MOUSE_STUB(sub_00071940)
#undef MOUSE_STUB
/* Bot helpers share the menu translation unit, but must never execute in
 * this isolated FOV fixture. Bot behavior has its own network regressions. */
#define BOT_STUB(name) void name(void) { CHECK(0); }
BOT_STUB(sub_000DAC30) BOT_STUB(sub_000D7570) BOT_STUB(sub_0009FBE0)
BOT_STUB(sub_001BE870) BOT_STUB(sub_0035C870)
BOT_STUB(sub_000A0860) BOT_STUB(sub_000A2B50)
BOT_STUB(sub_0011F7E0) BOT_STUB(sub_0011D510)
BOT_STUB(sub_000B3630) BOT_STUB(sub_000C1950) BOT_STUB(sub_001B67E0)
BOT_STUB(sub_000CF440) BOT_STUB(sub_001BB5C0)
BOT_STUB(sub_001BA2B0) BOT_STUB(sub_001BFCA0) BOT_STUB(sub_001BA130)
BOT_STUB(sub_0025EE60) BOT_STUB(sub_002EB0A0) BOT_STUB(sub_002EBE60) BOT_STUB(sub_00065420) BOT_STUB(sub_00065470_gen) BOT_STUB(sub_00065C70)
BOT_STUB(sub_002EAF60_gen) BOT_STUB(sub_002EAFC0_gen) BOT_STUB(sub_002EB030_gen) BOT_STUB(sub_002EC090_gen) BOT_STUB(sub_002EC650_gen)
BOT_STUB(sub_002CBBA0) BOT_STUB(sub_002CBAF0) BOT_STUB(sub_002CBB60) BOT_STUB(sub_002CB550)
BOT_STUB(sub_002C2C20) BOT_STUB(sub_002C0490) BOT_STUB(sub_002C19F0)
BOT_STUB(sub_002C1570) BOT_STUB(sub_002C1490)
#undef BOT_STUB
static unsigned pose_clip_bindings;
void sub_000966E0(void)
{
    CHECK(WORD_AT(g_esp+4)==0x540000 && WORD_AT(g_esp+8)==43);
    WORD_AT(0x5400d4)=43;++pose_clip_bindings;g_esp+=4;
}


/* Unlock Content has its own profile/menu fixture. This settings-only test
 * must link those dependencies without accidentally entering their paths. */
#define UNLOCK_STUB(name) void name(void) { CHECK(0); }
UNLOCK_STUB(sub_00356870) UNLOCK_STUB(sub_000696F0)
UNLOCK_STUB(sub_001CD040) UNLOCK_STUB(sub_00075E70)
#undef UNLOCK_STUB
/* Other shared translation-unit hooks must not run in this display fixture. */
int pc_weapon_slot_take(void) { CHECK(0);return 0; }
STUB(sub_001BAA70) STUB(sub_001BA410) STUB(sub_00142820)
STUB(sub_00071B60)
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
static int controls_test_enabled,controls_test_save_ok=1;
static unsigned controls_test_saves,controls_test_previews;
static PcSettings controls_test_settings;
void pc_settings_defaults(PcSettings *s) {CHECK(controls_test_enabled);*s=controls_test_settings;s->sensitivity=.1;s->enabled=1;s->invert_y=0;}
int pc_settings_read(PcSettings *s,char *e,size_t n) { (void)e;(void)n;CHECK(controls_test_enabled);*s=controls_test_settings;return 1; }
static int test_player_devices[4]={-2,-1,-1,-1},test_player_saves;
void pc_settings_players_read(int d[4]) {memcpy(d,test_player_devices,sizeof test_player_devices);}
int pc_settings_players_save(const int d[4],char *e,size_t n) {(void)e;(void)n;memcpy(test_player_devices,d,sizeof test_player_devices);test_player_saves++;return 1;}
static unsigned test_pad_buttons;
static int test_auto_players=1;
int pc_settings_players_auto(void){return test_auto_players;}
int pc_settings_players_auto_save(int on,char *e,size_t n){(void)e;(void)n;test_auto_players=on;return 1;}
void pc_settings_players_join(void){}
void pc_settings_pad_status(int slot,char *name,size_t n,int *pressed) {snprintf(name,n,"Test controller %d",slot+1);*pressed=(test_pad_buttons>>slot)&1;}
int pc_settings_save(const PcSettings *s,char *e,size_t n) {CHECK(controls_test_enabled);if(!controls_test_save_ok){snprintf(e,n,"Test save failed");return 0;}controls_test_settings=*s;controls_test_saves++;return 1; }
void pc_settings_key_name(int k,char *t,size_t n) {CHECK(controls_test_enabled);snprintf(t,n,"Key %d",k);}
int pc_settings_conflict(const PcSettings *s,int a,int k) { (void)s;(void)a;(void)k;CHECK(0);return 0; }
int pc_settings_key_allowed(int k) { (void)k;CHECK(0);return 0; }
int pc_settings_assign(PcSettings *s,int a,int slot,int k,int move) { (void)s;(void)a;(void)slot;(void)k;(void)move;CHECK(0);return 0; }
void pc_controls_set_mouse(double s,int enabled,int invert) { (void)s;(void)enabled;(void)invert;CHECK(controls_test_enabled);controls_test_previews++; }
/* Native builder/centring contract: width is a percentage, not pixels.
 * An out-of-range width renders a panel with its rows outside the canvas. */
void sub_00075B70(void) {
    built_rows=built_row_count=separators=0;last_built_item=0;WORD_AT(0x300178)=0;
    uint32_t word=WORD_AT(g_esp+12);float width;memcpy(&width,&word,4);
    float left=(100.f-width)*.5f;
    CHECK(isfinite(width) && width>0 && left>=0 && left+width<=100.f);
    g_esp+=4;
}
static unsigned native_control_ticks,native_control_backs,native_control_opens[2];
static void fixture_controller_button(void) {pc_display_menu_button(sub_00079D60_gen);}
void sub_002EA3F0(void) {
    for(unsigned id=0;id<12;id++) {
        if(id==10)guest_call(sub_00079710,1,0u);
        guest_call(fixture_controller_button,6,controls_text(5,"Native control"),id,0u,0u,0u,0u);
    }
    g_esp+=4;
}
static unsigned fixture_native_rows(void) {
    unsigned mask=0;
    for(uint32_t row=WORD_AT(0x300178);row;row=WORD_AT(row+4)) {
        CHECK(WORD_AT(row+8)==WORD_AT(row+4));
        unsigned id=WORD_AT(row+0x14);if(id<12)mask|=1u<<id;
    }
    return mask;
}
void sub_002EAB30(void) {CHECK(fixture_native_rows()==0xfff);native_control_ticks++;g_esp+=4;}
void sub_002E9FC0(void) {if(controls_tab!=2)CHECK(fixture_native_rows()==0xfff);native_control_backs++;g_esp+=4;}
void sub_002EA8E0(void) {CHECK(0);}
static unsigned native_control_actions;
static void fixture_native_action(void) {CHECK(fixture_native_rows()==0xfff);native_control_actions++;g_esp+=4;}
void sub_002CBE00(void) {fixture_native_action();}
void sub_002E9E50(void) {fixture_native_action();}
void sub_002D0970(void) {fixture_native_action();}
void sub_002E9EA0(void) {fixture_native_action();}
void sub_002EA210(void) {fixture_native_action();}
void sub_002CBE50(void) {native_control_opens[0]++;g_esp+=4;}
void sub_002EC1A0(void) {native_control_opens[1]++;g_esp+=4;}
STUB(sub_00071B10) STUB(sub_00071C50) STUB(sub_00071BE0)
STUB(sub_000771D0)
void sub_000770E0(void) {
    unsigned index=WORD_AT(g_esp+8);CHECK(index<built_row_count);
    uint32_t menu=WORD_AT(g_esp+4),id=built_ids[index];
    if(controls_font_menu(menu)) {
        uint32_t row=WORD_AT(menu+0x178);
        for(;row;row=WORD_AT(row+8)) {
            uint32_t type=WORD_AT(row);
            if(type!=4 && type!=7 && type!=8)continue;
            if(!index){id=WORD_AT(row+0x14);break;}index--;
        }
        CHECK(row);
    }
    WORD_AT(menu+0x18c)=id;g_esp+=4;
}
void sub_00078980(void) {
    unsigned first=WORD_AT(0x200000+0xc8),second=WORD_AT(0x200200+0xc8),
        textures=WORD_AT(0x200e00+0xc8);
    if(controls_font_menu(WORD_AT(g_esp+4)))controls_build();else build_menu();
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
void sub_00079710(void) {
    CHECK(WORD_AT(g_esp+4)==0 && separators<2 && built_row_count<32);
    separator_positions[separators++]=built_row_count;
    built_ids[built_row_count++]=UINT32_MAX;
    uint32_t item=0x230000+separators*0x200;WORD_AT(item)=9;WORD_AT(item+0x14)=UINT32_MAX;fixture_link(item);g_esp+=4;
}
STUB(sub_00074910)
void sub_001A0A60(void) {
    CHECK(getenv("TSFP_NETWORK") && !strcmp(getenv("TSFP_NETWORK"),"1"));
    CHECK(!getenv("TSFP_ONLINE_CONFIG") || !*getenv("TSFP_ONLINE_CONFIG"));
    lan_browser_entries++;g_esp+=4;
}
#ifdef TSFP_TEST_ORIGINAL_SYSTEM_LINK
#define eax g_eax
#define esp g_esp
#include "system_link_original.inc"
#undef eax
#undef esp
#else
void sub_001A2700(void) {
    if(WORD_AT(WORD_AT(g_esp+4)+0x18c)==8u) sub_001A0A60();
    else g_esp+=4;
}
#endif
    STUB(sub_000653B0) STUB(sub_00065940) STUB(sub_00352640) STUB(sub_00352650) STUB(sub_00356490) STUB(sub_002EB030)
STUB(sub_00170ED0) STUB(sub_00165D80) STUB(sub_00165A40) STUB(sub_00167CA0) STUB(sub_00167CE0) STUB(sub_00167BC0)
STUB(sub_00351580) STUB(sub_003515A0) STUB(sub_00352200) STUB(sub_00351F60) STUB(sub_003513C0) STUB(sub_001677E0) STUB(sub_000B2E50) STUB(sub_0034FF30)
STUB(sub_000D7550) STUB(sub_000A9070) STUB(sub_0015BCA0) STUB(sub_0015BCC0)
STUB(sub_002402B0) STUB(sub_00353A90)
STUB(sub_0025EAD0) STUB(sub_0025EC70) STUB(sub_000C6100) STUB(sub_000D4B00) STUB(sub_000D4BE0) STUB(sub_000D1EC0) STUB(sub_000D2DE0) STUB(sub_000A6F90)
STUB(sub_001A8D30) STUB(sub_001A8F30) STUB(sub_002CB090) STUB(sub_002CAFE0) STUB(sub_002D62F0)
STUB(sub_00197890) STUB(sub_002CA9F0) STUB(sub_003535C0) STUB(sub_003544B0) STUB(sub_00359EB0) STUB(sub_00198E20) STUB(sub_00040F00) STUB(sub_00041DC0)
STUB(sub_00080F20)
STUB(sub_002CAD80)
static float drawn;
void sub_000814B0(void) { uint32_t b=WORD_AT(g_esp+4);memcpy(&drawn,&b,4);g_esp+=4; }
void sub_00154FE0(void) { g_eax=WORD_AT(g_esp+4);g_esp+=4; }
void sub_00079B70(void) { returned++;g_esp+=4; }
void sub_00079D60_gen(void) {
    unsigned id=WORD_AT(g_esp+8);CHECK(id<256);
    if(page==DISPLAY_HOME && id>=20 && id<=21)CHECK(!strncmp((char *)(g_xbox_mem_offset+WORD_AT(g_esp+4)),"\vs75",4));
    if(id<32)built_rows|=1u<<id;CHECK(built_row_count<32);built_ids[built_row_count++]=id;
    g_eax=0x200000+id*0x200;WORD_AT(g_eax)=4;WORD_AT(g_eax+0xb0)=1;WORD_AT(g_eax+0x14)=id;fixture_link(g_eax);WORD_AT(g_eax+0x28)=WORD_AT(g_esp+12)|0x2000u;WORD_AT(g_eax+0x88)=current_menu;g_esp+=4;
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
    /* Shared selector keeps legacy files, precedence and failed-save isolation. */
    graphics_options.edge_smoothing=1;smaa_preference=1;
    CHECK(pc_post_aa_get()==2 && !pc_post_aa_set(3));
    smaa_preference=0;smaa_save_ok=0;
    CHECK(!pc_post_aa_set(2) && graphics_options.edge_smoothing==1 && !smaa_preference);
    smaa_save_ok=1;graphics_save_ok=0;
    CHECK(!pc_post_aa_set(2) && graphics_options.edge_smoothing==1 && !smaa_preference);
    graphics_save_ok=1;CHECK(pc_post_aa_set(2) && smaa_preference && !graphics_options.edge_smoothing);
    CHECK(pc_post_aa_set(1) && !smaa_preference && graphics_options.edge_smoothing==1);
    CHECK(pc_post_aa_set(0) && !smaa_preference && !graphics_options.edge_smoothing);
    void *mem=VirtualAlloc(NULL,0x800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(mem);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)mem;
    { /* No startup network overrides: the real row callback must enable LAN. */
        CHECK(!_putenv_s("TSFP_NETWORK",""));
        CHECK(!_putenv_s("TSFP_ONLINE_CONFIG","keep-online"));
        call(0x001A2700u,0,0);call(0x001A2700u,7,0);
        CHECK(!online_disconnects && !lan_browser_entries);
        CHECK(!strcmp(getenv("TSFP_ONLINE_CONFIG"),"keep-online"));
        call(0x001A2700u,8,0);
        CHECK(online_disconnects==1 && lan_browser_entries==1);
        call(0x001A2700u,8,0);
        CHECK(online_disconnects==2 && lan_browser_entries==2);
    }
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
    CHECK(page==DISPLAY_HOME && built_row_count==7 && separators==2 && separator_positions[0]==1 && separator_positions[1]==5 && aspect_row && rate_row && !subtitle_row && !texture_row && !counter_row);
    page=DISPLAY_GRAPHICS;built_rows=built_row_count=separators=0;labels();
    CHECK(!graphics_rows[0] && !graphics_rows[4] && texture_row && post_aa_row && !counter_row && !aspect_row);
    CHECK(built_ids[2]==102 && built_ids[3]==103 && built_ids[4]==7 && built_ids[5]==11 && built_ids[6]==UINT32_MAX && built_ids[7]==108 && built_ids[8]==3);
    WORD_AT(post_aa_row+0xc8)=1;read_choices();
    CHECK(staged_aa==1 && !graphics_options.edge_smoothing);
    call(SELECT_CB,3,0);CHECK(graphics_options.edge_smoothing==1 && !pc_smaa_enabled());
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(post_aa_row+0xc8)=0;call(SELECT_CB,3,0);
    refresh(DISPLAY_GRAPHICS,0);
    graphics_save_ok=0;WORD_AT(graphics_rows[2]+0xc8)=4;read_choices();
    CHECK(graphics_options.anisotropy==1 && staged_anisotropy==16 && display_dirty());
    call(SELECT_CB,3,0);CHECK(page==DISPLAY_GRAPHICS && graphics_options.anisotropy==1 && staged_anisotropy==16 && display_dirty());
    graphics_save_ok=1;call(SELECT_CB,3,0);CHECK(page==DISPLAY_GRAPHICS && graphics_options.anisotropy==16 && !display_dirty());
    WORD_AT(graphics_rows[3]+0xc8)=1;read_choices();
    CHECK(!graphics_options.ambient_occlusion && staged_ao && display_dirty());
    refresh(DISPLAY_VIEW,0);refresh(DISPLAY_GRAPHICS,0);CHECK(WORD_AT(graphics_rows[3]+0xc8)==1);
    call(SELECT_CB,3,0);CHECK(page==DISPLAY_GRAPHICS && graphics_options.ambient_occlusion && !display_dirty());
    WORD_AT(graphics_rows[3]+0xc8)=0;WORD_AT(graphics_rows[2]+0xc8)=1;read_choices();
    call(BACK_CB,0,0);call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);refresh(DISPLAY_GRAPHICS,0);
    CHECK(staged_ao && staged_anisotropy==16 && graphics_options.ambient_occlusion && !display_dirty());
    WORD_AT(graphics_rows[3]+0xc8)=0;read_choices();call(SELECT_CB,3,0);
    CHECK(!graphics_options.ambient_occlusion && !display_dirty());
    call(SELECT_CB,108,0);CHECK(page==DISPLAY_ADVANCED && !post_aa_row && graphics_rows[0] && graphics_rows[4]);
    CHECK(!strcmp((char *)((uintptr_t)TEXT_ADDRESS(32)+g_xbox_mem_offset),"MSAA"));
    CHECK(!strcmp((char *)((uintptr_t)TEXT_ADDRESS(37)+g_xbox_mem_offset),"Supersampling"));
    CHECK(!strstr((char *)((uintptr_t)TEXT_ADDRESS(81)+g_xbox_mem_offset),"require restart"));
    staged_supersampling=1;graphics_restart_labels();
    CHECK(strstr((char *)((uintptr_t)TEXT_ADDRESS(37)+g_xbox_mem_offset),"Requires restart"));
    staged_supersampling=0;graphics_restart_labels();
    CHECK(!strcmp((char *)((uintptr_t)TEXT_ADDRESS(37)+g_xbox_mem_offset),"Supersampling"));
    WORD_AT(graphics_rows[4]+0xc8)=0;read_choices();
    CHECK(graphics_options.shader_cache==1 && staged_cache==0 && display_dirty());
    call(SELECT_CB,3,0);CHECK(page==DISPLAY_ADVANCED && graphics_options.shader_cache==0 && !display_dirty());
    /* Restart fields stay staged through edits/ticks/tabs until Apply. */
    WORD_AT(graphics_rows[0]+0xc8)=1;call(TICK_CB,100,0);
    CHECK(page==DISPLAY_ADVANCED && graphics_options.msaa==1 && staged_msaa==2);
    CHECK(strstr((char *)((uintptr_t)TEXT_ADDRESS(32)+g_xbox_mem_offset),"Requires restart"));
    CHECK(!strstr((char *)((uintptr_t)TEXT_ADDRESS(37)+g_xbox_mem_offset),"Requires restart"));
    CHECK(display_dirty() && !(WORD_AT(apply_row+0x28)&1));
    graphics_save_ok=0;call(SELECT_CB,3,0);
    CHECK(page!=DISPLAY_RESTART && graphics_options.msaa==1 && staged_msaa==2 && display_dirty());
    graphics_save_ok=1;refresh(DISPLAY_ADVANCED,0);
    CHECK(WORD_AT(graphics_rows[0]+0xc8)==1);
    call(SELECT_CB,3,0);CHECK(page==DISPLAY_RESTART && graphics_options.msaa==2);
    call(SELECT_CB,0,0);CHECK(page==DISPLAY_ADVANCED && !display_dirty() && WORD_AT(current_menu+0x18c)==100);
    call(TICK_CB,100,0);CHECK(page==DISPLAY_ADVANCED); /* Later does not nag. */
    CHECK(strstr((char *)((uintptr_t)TEXT_ADDRESS(32)+g_xbox_mem_offset),"Requires restart"));
    /* Reverting a choice before Apply leaves no dirty state or prompt. */
    WORD_AT(graphics_rows[0]+0xc8)=0;call(TICK_CB,100,0);
    CHECK(graphics_options.msaa==2 && staged_msaa==1 && page==DISPLAY_ADVANCED);
    CHECK(!strcmp((char *)((uintptr_t)TEXT_ADDRESS(32)+g_xbox_mem_offset),"MSAA"));
    WORD_AT(graphics_rows[0]+0xc8)=1;call(TICK_CB,100,0);CHECK(!display_dirty());
    /* Multiple restart changes produce one prompt, only after Apply. */
    WORD_AT(graphics_rows[0]+0xc8)=0;WORD_AT(graphics_rows[5]+0xc8)=2;
    call(TICK_CB,105,0);CHECK(page==DISPLAY_ADVANCED && graphics_options.msaa==2 && !graphics_options.supersampling);
    refresh(DISPLAY_GRAPHICS,0);CHECK(display_dirty());
    call(SELECT_CB,3,0);CHECK(page==DISPLAY_RESTART && graphics_options.msaa==1 && graphics_options.supersampling==2);
    call(SELECT_CB,0,0);CHECK(page==DISPLAY_GRAPHICS && staged_supersampling==2 && !display_dirty());
    refresh(DISPLAY_ADVANCED,0);
    /* Leaving without Apply discards the uncommitted restart choice. */
    WORD_AT(graphics_rows[5]+0xc8)=3;read_choices();CHECK(staged_supersampling==3);
    call(BACK_CB,0,0);call(BACK_CB,0,0);
    call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    CHECK(staged_supersampling==2 && !display_dirty());refresh(DISPLAY_ADVANCED,0);
    call(BACK_CB,0,0);CHECK(page==DISPLAY_GRAPHICS);
    refresh(DISPLAY_VIEW,0);CHECK(subtitle_row && counter_row && !texture_row && !rate_row);
    WORD_AT(counter_row+0xc8)=1;read_choices();CHECK(counter_enabled);
    counter_save_ok=0;WORD_AT(counter_row+0xc8)=0;read_choices();
    CHECK(counter_enabled && WORD_AT(counter_row+0xc8)==1);
    counter_save_ok=1;WORD_AT(counter_row+0xc8)=0;read_choices();CHECK(!counter_enabled);
    page=DISPLAY_HOME;built_rows=built_row_count=separators=0;labels();
    call(SELECT_CB,VIDEO_TABS_ID,0);CHECK(page==DISPLAY_GRAPHICS);
    call(TICK_CB,VIDEO_TABS_ID,0x80000u);CHECK(page==DISPLAY_VIEW);
    call(TICK_CB,VIDEO_TABS_ID,0x40000u);CHECK(page==DISPLAY_GRAPHICS);
    video_tab_bounds_valid=1;
    video_tab_bounds[0]=20;video_tab_bounds[1]=30;video_tab_bounds[2]=320;video_tab_bounds[3]=54;
    CHECK(video_tab_hit(20,30)==0 && video_tab_hit(120,40)==1 && video_tab_hit(319,53)==2);
    CHECK(video_tab_hit(320,40)==-1 && video_tab_hit(21,54)==-1);
    { /* A mouse Confirm chooses its tab, even after its edge was consumed. */
        MenuMouseDispatch click={0};click.menu=current_menu;click.sampled=1;
        click.pointer=(PcMenuPointer){397.5f,90.f,1920,1080,1,1,0,0};
        menu_mouse_dispatch=&click;slider_capture_enabled=1;
        WORD_AT(0x004E8CF0u)=640;WORD_AT(0x004E8CF4u)=480;
        CHECK(video_clicked_tab()==0);
        page=DISPLAY_GRAPHICS;call(SELECT_CB,VIDEO_TABS_ID,0);CHECK(page==DISPLAY_HOME);
        video_tab_bounds_valid=1;
        call(SELECT_CB,VIDEO_TABS_ID,0);CHECK(page==DISPLAY_HOME && video_tab_bounds_valid);
        call(SELECT_CB,VIDEO_TABS_ID,0);CHECK(page==DISPLAY_HOME && video_tab_bounds_valid);
        /* Rapid different-tab clicks remain valid before another draw. */
        click.pointer.x=622.5f;call(SELECT_CB,VIDEO_TABS_ID,0);
        CHECK(page==DISPLAY_GRAPHICS && video_tab_bounds_valid);
        click.pointer.x=847.5f;call(SELECT_CB,VIDEO_TABS_ID,0);
        CHECK(page==DISPLAY_VIEW && video_tab_bounds_valid);
        click.pointer.x=397.5f;call(SELECT_CB,VIDEO_TABS_ID,0);
        CHECK(page==DISPLAY_HOME && video_tab_bounds_valid);
        /* Stale/no hit is a no-op, never Graphics -> View & HUD. */
        page=DISPLAY_GRAPHICS;video_tab_bounds_valid=0;
        call(SELECT_CB,VIDEO_TABS_ID,0);CHECK(page==DISPLAY_GRAPHICS);
        menu_mouse_dispatch=0;slider_capture_enabled=0;
    }
    { /* Queue through native row gating; newest click wins, in both contexts. */
        ULONGLONG old_clock=clock_ms;
        uint32_t old_phase=WORD_AT(0x79094c),old_pause=WORD_AT(0x761c48);
        WORD_AT(0x761c48)=0x410000;WORD_AT(0x410000)=1;
        WORD_AT(current_menu+4)=storage;WORD_AT(current_menu+0x38)=0;
        WORD_AT(0x4e8cf0)=640;WORD_AT(0x4e8cf4)=480;
        tab_pointer_enabled=slider_capture_enabled=1;
        for(int paused=0;paused<2;paused++) {
            WORD_AT(0x79094c)=paused?1:101;
            page=DISPLAY_GRAPHICS;video_tab_bounds_valid=1;
            WORD_AT(current_menu+0x1b0)=1;video_pending_tab=-1;video_tab_left=1;
            tab_pointer=(PcMenuPointer){397.5f,90,1920,1080,1,0,0,0};
            CHECK(!menu_video_tabs_poll(current_menu));tab_pointer.left=1;
            CHECK(menu_video_tabs_poll(current_menu) && video_pending_tab==0);
            menu_video_tabs_flush();CHECK(page==DISPLAY_GRAPHICS && video_pending_tab==0);
            tab_pointer.left=0;CHECK(!menu_video_tabs_poll(current_menu));
            tab_pointer.x=847.5f;tab_pointer.left=1;
            CHECK(menu_video_tabs_poll(current_menu) && video_pending_tab==2);
            WORD_AT(current_menu+0x1b0)=3;menu_video_tabs_flush();
            CHECK(page==DISPLAY_VIEW && video_pending_tab==-1);
            CHECK(!menu_video_tabs_poll(current_menu)); /* Held click cannot repeat. */
            tab_pointer.left=0;menu_video_tabs_poll(current_menu);
            tab_pointer.x=397.5f;tab_pointer.left=1;CHECK(menu_video_tabs_poll(current_menu));
            menu_video_tabs_flush();CHECK(page==DISPLAY_HOME);
            tab_pointer.left=0;menu_video_tabs_poll(current_menu);
            tab_pointer.x=622.5f;tab_pointer.left=1;CHECK(menu_video_tabs_poll(current_menu));
            clock_ms+=1001;menu_video_tabs_flush();CHECK(page==DISPLAY_HOME && video_pending_tab==-1);
        }
        WORD_AT(0x79094c)=1;WORD_AT(0x410000)=0;
        video_pending_tab=1;video_pending_menu=current_menu;video_pending_at=clock_ms;
        menu_video_tabs_flush();CHECK(video_pending_tab==-1 && page==DISPLAY_HOME);
        WORD_AT(0x79094c)=old_phase;WORD_AT(0x761c48)=old_pause;clock_ms=old_clock;
        tab_pointer_enabled=slider_capture_enabled=0;
    }
    staged_fov=67;texture_choice=1;refresh(DISPLAY_VIEW,0);refresh(DISPLAY_GRAPHICS,0);
    CHECK(staged_fov==67 && texture_choice==1 && WORD_AT(texture_row+0xc8)==1);
    staged_fov=previous_fov;texture_choice=applied_texture;refresh(DISPLAY_GRAPHICS,0);
    call(SELECT_CB,108,0);CHECK(page==DISPLAY_ADVANCED);
    call(SELECT_CB,106,0);CHECK(cache_clear_calls==1 && page==DISPLAY_ADVANCED);
    call(BACK_CB,0,0);CHECK(page==DISPLAY_GRAPHICS);
    {int before=returned;call(BACK_CB,0,0);CHECK(returned==before+1);}
    refresh(DISPLAY_VIEW,0);
    status[0]=0;
    { /* Use the native switch sound for all Video setting rows, not Apply. */
        WORD_AT(current_menu+4)=storage;WORD_AT(0x7ba16c)=1;WORD_AT(0x7ba178)=0x510000;
        WORD_AT(0x510008)=0x1234;
        refresh(DISPLAY_GRAPHICS,0);
        unsigned ids[]={102,103,7,11};
        for(unsigned i=0;i<sizeof ids/sizeof ids[0];i++)
            menu_video_click_sound(current_menu,0x200000+ids[i]*0x200);
        CHECK(video_sounds==4);
        refresh(DISPLAY_ADVANCED,0);
        menu_video_click_sound(current_menu,graphics_rows[0]);
        menu_video_click_sound(current_menu,graphics_rows[4]);
        CHECK(video_sounds==6);refresh(DISPLAY_GRAPHICS,0);
        menu_video_click_sound(current_menu,apply_row);CHECK(video_sounds==6);
        WORD_AT(post_aa_row+0x28)|=1;menu_video_click_sound(current_menu,post_aa_row);CHECK(video_sounds==6);
        WORD_AT(post_aa_row+0x28)&=~1u;
        WORD_AT(current_menu+4)=0;menu_video_click_sound(current_menu,post_aa_row);CHECK(video_sounds==6);
        WORD_AT(current_menu+4)=storage;
        page=DISPLAY_CONFIRM;menu_video_click_sound(current_menu,post_aa_row);CHECK(video_sounds==6);
        page=DISPLAY_GRAPHICS;WORD_AT(0x7ba178)=0;
        menu_video_click_sound(current_menu,post_aa_row);CHECK(video_sounds==6);
        WORD_AT(0x7ba16c)=0;menu_video_click_sound(current_menu,post_aa_row);CHECK(video_sounds==6);
    }
    refresh(DISPLAY_VIEW,0);
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
    refresh(DISPLAY_GRAPHICS,0);
    /* SMAA is staged, survives a menu rebuild, and Back discards it. */
    CHECK(!staged_aa && !pc_smaa_enabled());
    WORD_AT(post_aa_row+0xc8)=2;call(TICK_CB,11,0);
    CHECK(display_dirty() && !pc_smaa_enabled());
    refresh(DISPLAY_GRAPHICS,0);CHECK(WORD_AT(post_aa_row+0xc8)==2 && display_dirty());
    call(BACK_CB,0,0);call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    refresh(DISPLAY_GRAPHICS,0);
    CHECK(!staged_aa && !display_dirty());
    WORD_AT(post_aa_row+0xc8)=2;call(TICK_CB,11,0);smaa_save_ok=0;
    call(SELECT_CB,3,0);CHECK(!pc_smaa_enabled() && display_dirty() && *status);
    smaa_save_ok=1;call(SELECT_CB,3,0);CHECK(page==DISPLAY_GRAPHICS && pc_smaa_enabled() && !display_dirty());
    call(BACK_CB,0,0);call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    refresh(DISPLAY_GRAPHICS,0);
    CHECK(staged_aa && applied_aa && WORD_AT(post_aa_row+0xc8)==2 && !display_dirty());
    WORD_AT(post_aa_row+0xc8)=0;call(TICK_CB,11,0);call(SELECT_CB,3,0);
    CHECK(!pc_smaa_enabled() && !display_dirty());
    refresh(DISPLAY_VIEW,0);
    CHECK(built_rows&(1u<<9));
    WORD_AT(subtitle_row+0xc8)=1;call(TICK_CB,9,0);
    CHECK(display_dirty() && !WORD_AT(0x400130) && !WORD_AT(0x7613dc));
    call(SELECT_CB,3,0);
    CHECK(page==DISPLAY_VIEW && WORD_AT(0x400130)==1 && WORD_AT(0x7613dc)==1 && (WORD_AT(0x78ad24)&0x10000));
    CHECK(!display_dirty() && (WORD_AT(apply_row+0x28)&1));

    refresh(DISPLAY_HOME,0);
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
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(texture_row+0xc8)=1;read_choices();
    refresh(DISPLAY_HOME,0);WORD_AT(rate_row+0xc8)=1;read_choices();
    refresh(DISPLAY_VIEW,0);
    for(int i=0;i<20;i++)call(TICK_CB,2,0x80000);
    CHECK(staged_fov==75 && pc_display_fov()==55 && !tsfp_texture_is_4k());
    CHECK(display_dirty() && !(WORD_AT(apply_row+0x28)&1));
    int left=returned;call(BACK_CB,0,0);CHECK(returned==left+1);
    call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
    CHECK(!display_dirty() && staged_fov==55 && staged_rate==0 && texture_choice==0);
    content("TSFP_DISPLAY_V1 1280 720\n");
    refresh(DISPLAY_VIEW,0);
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
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(texture_row+0xc8)=1;read_choices();
    refresh(DISPLAY_HOME,0);WORD_AT(rate_row+0xc8)=1;read_choices();
    refresh(DISPLAY_VIEW,0);
    call(TICK_CB,8,0);CHECK(!tsfp_texture_is_4k() && requested_rate==-1);
    call(SELECT_CB,3,0);
    CHECK(page==DISPLAY_VIEW && pc_display_fov()==70 && tsfp_texture_is_4k());
    CHECK(requested_rate==120 && pc_frame_rate_read()==120);
    CHECK(!display_dirty() && (WORD_AT(apply_row+0x28)&1));
    content("TSFP_DISPLAY_V2 1280 720 70\n");
    /* A failed SMAA save also rolls back every Apply-only graphics field. */
    TsfpGraphicsOptions before_failed_batch=pc_graphics_get();
    staged_anisotropy=4;staged_ao=!before_failed_batch.ambient_occlusion;staged_cache=!before_failed_batch.shader_cache;
    staged_msaa=4;
    refresh(DISPLAY_HOME,0);staged_fov=75;WORD_AT(rate_row+0xc8)=2;read_choices();
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(post_aa_row+0xc8)=2;smaa_save_ok=0;
    call(SELECT_CB,3,0);
    CHECK(pc_display_fov()==70 && requested_rate==120 && pc_frame_rate_read()==120 && !pc_smaa_enabled() && graphics_options.msaa==1 && staged_msaa==4 && page!=DISPLAY_RESTART);
    content("TSFP_DISPLAY_V2 1280 720 70\n");
    CHECK(graphics_options.anisotropy==before_failed_batch.anisotropy &&
          graphics_options.ambient_occlusion==before_failed_batch.ambient_occlusion &&
          graphics_options.shader_cache==before_failed_batch.shader_cache);
    smaa_save_ok=1;staged_fov=70;staged_rate=applied_rate;staged_aa=applied_aa;staged_msaa=graphics_options.msaa;
    staged_anisotropy=graphics_options.anisotropy;staged_ao=graphics_options.ambient_occlusion;staged_cache=graphics_options.shader_cache;
    refresh(DISPLAY_GRAPHICS,0);call(TICK_CB,11,0);CHECK(!display_dirty());
    WORD_AT(post_aa_row+0xc8)=2;read_choices();refresh(DISPLAY_VIEW,0);
    CHECK(staged_aa==2 && !pc_smaa_enabled());
    call(SELECT_CB,3,0);CHECK(pc_smaa_enabled() && !display_dirty());
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(post_aa_row+0xc8)=0;call(SELECT_CB,3,0);
    CHECK(!pc_smaa_enabled() && !display_dirty());
    refresh(DISPLAY_VIEW,0);
    call(TICK_CB,2,0x80000);CHECK(!(WORD_AT(apply_row+0x28)&1));
    call(TICK_CB,2,0x40000);CHECK(WORD_AT(apply_row+0x28)&1);
    /* Failed display save must not publish FPS or texture changes. */
    staged_fov=75;WORD_AT(subtitle_row+0xc8)=0;read_choices();
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(texture_row+0xc8)=0;read_choices();
    refresh(DISPLAY_HOME,0);WORD_AT(rate_row+0xc8)=2;
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
    WORD_AT(rate_row+0xc8)=0;texture_choice=1;call(SELECT_CB,3,0);
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
    refresh(DISPLAY_GRAPHICS,0);
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
        CHECK(observed_callback_count==2 && observed_callbacks[0]==PC_DISPLAY_MENU_CALLBACK);
        CHECK(observed_callbacks[1]==callback && WORD_AT(0x700014)==callback);
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
    /* The common pose renderer preserves local asset/bone handles and clears
       root-motion flags already represented by the host body transform. */
    const uint32_t entity=0x520000,model=0x540000;
    WORD_AT(entity+4)=model;WORD_AT(model+0xd4)=42;WORD_AT(model+0x12c)=43;
    WORD_AT(model+0xd8)=0xcafe;WORD_AT(model+0x18c)=0xbabe;
    WORD_AT(model+0x128)=0xc03;WORD_AT(model+0x180)=0xc05;
    *(int16_t *)((uintptr_t)(0x7e03c0+43*2)+g_xbox_mem_offset)=17;
    PcCharacterPose pose;pc_character_pose_read(&pose,(void *)((uintptr_t)model+g_xbox_mem_offset),NULL);
    CHECK(pc_character_pose_valid(&pose));pose.clip=43;pose.clip_channels[0]=.5f;
    pose.blend_channels[2]=.75f;g_esp=0x700000;
    pc_character_pose_present(entity,&pose);
    float pose_time;memcpy(&pose_time,&WORD_AT(model+0xdc),4);
    CHECK(pose_clip_bindings==1 && pose_time==.5f && WORD_AT(model+0x130)==17 &&
          WORD_AT(model+0x128)==3 && WORD_AT(model+0x180)==5 &&
          WORD_AT(model+0xd8)==0xcafe && WORD_AT(model+0x18c)==0xbabe && g_esp==0x700000);
    pose.clip_channels[0]=NAN;pc_character_pose_present(entity,&pose);
    CHECK(pose_clip_bindings==1);

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
    /* Split Controls retains all native profile records for readers/actions,
     * while only the selected section occupies the layout and input list. */
    controls_test_enabled=1;controls_test_settings.sensitivity=.1;
    controls_test_settings.enabled=1;tab_pointer_enabled=1;
    WORD_AT(0x79094c)=101;call(0x2cbe50,0,0);
    CHECK(native_control_opens[0]==1 && controls_tab==0);
    WORD_AT(0x300004)=CONTROLS_DESCRIPTOR;call(0x2ea8e0,0,0);
    CHECK(controls_font_menu(0x300000) && !controls_font_menu(0x320000));
    unsigned general=(1u<<1)|(1u<<2)|(1u<<4)|(1u<<5)|(1u<<6)|(1u<<7)|(1u<<10)|(1u<<11);
    unsigned controller=(1u<<0)|(1u<<3)|(1u<<8)|(1u<<9)|(1u<<10)|(1u<<11);
    CHECK(fixture_native_rows()==general && separators==2);
    CHECK(WORD_AT(0x200000+0xb0)==0 && WORD_AT(0x200200+0xb0)==1);
    call(0x2eab30,1,0);CHECK(native_control_ticks==1 && fixture_native_rows()==general);
    CHECK(!controls_dirty() && (WORD_AT(0x201400+0x28)&1));
    call(0x2cbe00,10,0);CHECK(native_control_actions==0);
    WORD_AT(controls_draft())^=1u;call(0x2eab30,CONTROLS_TABS_ID,0);
    CHECK(controls_dirty() && !(WORD_AT(0x201400+0x28)&1));
    WORD_AT(controls_draft())^=1u;call(0x2eab30,CONTROLS_TABS_ID,0);
    CHECK(!controls_dirty() && (WORD_AT(0x201400+0x28)&1));
    WORD_AT(controls_draft())^=1u;
    call(0x2cbe00,10,0);call(0x2d0970,11,0);
    CHECK(native_control_actions==2 && fixture_native_rows()==general);
    call(0x2eab30,CONTROLS_TABS_ID,0x80000);
    CHECK(controls_tab==1 && fixture_native_rows()==controller && separators==2);
    CHECK(controls_dirty() && !(WORD_AT(0x201400+0x28)&1));
    call(0x2ea210,0,0);CHECK(native_control_actions==3 && fixture_native_rows()==controller);
    call(0x2eab30,3,0);CHECK(native_control_ticks==2 && fixture_native_rows()==controller);
    /* Header navigation and keyboard IDs do not save or inherit native values. */
    call(0x2eab30,CONTROLS_TABS_ID,0x80000);
    CHECK(controls_tab==2 && kb_page==KB_HOME && controls_test_saves==0);
    CHECK(built_ids[0]==CONTROLS_TABS_ID && kb_widget_id(0)==200);
    CHECK(kb_local_id(200)==0 && kb_local_id(205)==5);
    CHECK(built_ids[built_row_count-1]==205); /* No on-screen Back. */
    kb_group=0;kb_refresh(KB_LIST,0);
    CHECK(separators==1 && built_ids[built_row_count-1]==208);
    CHECK(separator_positions[0]==built_row_count-2); /* Next below divider. */
    kb_refresh(KB_HOME,0);
    /* Mouse settings keep their live preview/debounced save and rollback. */
    kb_refresh(KB_MOUSE,0);CHECK(WORD_AT(0x30018c)==kb_widget_id(0));clock_ms=20000;tab_pointer.left=0;
    WORD_AT(kb_enabled_row+0xc8)=0;call(0x2eab30,kb_widget_id(0),0);
    CHECK(kb_settings.enabled==0 && controls_test_previews==1 && !controls_test_saves);
    clock_ms+=249;call(0x2eab30,kb_widget_id(0),0);CHECK(!controls_test_saves);
    clock_ms++;call(0x2eab30,kb_widget_id(0),0);
    CHECK(controls_test_saves==1 && controls_test_settings.enabled==0);
    controls_test_save_ok=0;WORD_AT(kb_enabled_row+0xc8)=1;
    clock_ms++;call(0x2eab30,kb_widget_id(0),0);
    clock_ms+=250;call(0x2eab30,kb_widget_id(0),0);
    CHECK(kb_settings.enabled==0 && !kb_mouse_changed && controls_test_saves==1);
    controls_test_save_ok=1;kb_refresh(KB_HOME,0);
    CHECK(controls_tabs_visible());kb_page=KB_CAPTURE;CHECK(!controls_tabs_visible());kb_page=KB_HOME;
    uint32_t caption=controls_caption();controls_text(0,"Choose a binding");
    CHECK(!strcmp((char *)(g_xbox_mem_offset+caption),"Controls"));
    call(0x2eab30,CONTROLS_TABS_ID,0x80000);
    CHECK(controls_tab==0 && fixture_native_rows()==general && controls_test_saves==1);
    call(0x2e9fc0,0,0);CHECK(native_control_backs==1 && fixture_native_rows()==0xfff);
    WORD_AT(0x79094c)=1;call(0x2ec1a0,0,0);
    CHECK(native_control_opens[1]==1 && controls_tab==0);
    call(0x2ea8e0,0,0);CHECK(!controls_dirty());WORD_AT(controls_draft())^=1u;call(0x2e9e50,10,0);call(0x2e9ea0,11,0);
    CHECK(native_control_actions==5 && fixture_native_rows()==general);
    /* The same direct tab queue serves Controls in main and pause menus. */
    slider_capture_enabled=1;WORD_AT(0x761c48)=0x410000;WORD_AT(0x410000)=1;
    WORD_AT(0x4e8cf0)=640;WORD_AT(0x4e8cf4)=480;
    for(unsigned paused=0;paused<2;paused++) {
        WORD_AT(0x79094c)=paused?1:101;controls_tab=0;
        controls_bounds[0]=20;controls_bounds[1]=30;controls_bounds[2]=320;controls_bounds[3]=54;
        controls_bounds_valid=1;controls_left=1;controls_pending=-1;
        WORD_AT(0x300038)=0;WORD_AT(0x3001b0)=1;
        tab_pointer=(PcMenuPointer){397.5f,90,1920,1080,1,0,0,0};
        CHECK(!menu_video_tabs_poll(0x300000));tab_pointer.left=1;
        CHECK(menu_video_tabs_poll(0x300000) && controls_pending==0);
        menu_video_tabs_flush();CHECK(controls_tab==0 && controls_pending==0);
        tab_pointer.left=0;menu_video_tabs_poll(0x300000);
        tab_pointer.x=847.5f;tab_pointer.left=1;
        CHECK(menu_video_tabs_poll(0x300000) && controls_pending==2);
        WORD_AT(0x3001b0)=3;menu_video_tabs_flush();
        CHECK(controls_tab==2 && controls_pending==-1);
        CHECK(!menu_video_tabs_poll(0x300000));
        tab_pointer.left=0;menu_video_tabs_poll(0x300000);
        tab_pointer.x=622.5f;tab_pointer.left=1;CHECK(menu_video_tabs_poll(0x300000));
        menu_video_tabs_flush();CHECK(controls_tab==1);
        tab_pointer.left=0;menu_video_tabs_poll(0x300000);
        tab_pointer.x=397.5f;tab_pointer.left=1;CHECK(menu_video_tabs_poll(0x300000));
        clock_ms+=1001;menu_video_tabs_flush();CHECK(controls_tab==1 && controls_pending==-1);
    }
    { /* Consumed or unmatched mouse Confirm must never cycle tabs. */
        MenuMouseDispatch click={0};click.menu=controls_menu;click.injected=0x80u;
        click.pointer=(PcMenuPointer){397.5f,90,1920,1080,1,0,0,0};
        menu_mouse_dispatch=&click;controls_bounds_valid=0;
        call(CONTROLS_TABS_SELECT,CONTROLS_TABS_ID,0);CHECK(controls_tab==1);
        controls_bounds_valid=1;click.pointer.x=622.5f;
        call(CONTROLS_TABS_SELECT,CONTROLS_TABS_ID,0);CHECK(controls_tab==1);
        menu_mouse_dispatch=0;
    }
    /* Keep the root header fixed as content height changes; modal pages
     * retain their own native centring. */
    controls_tab=0;controls_anchor_ready=0;
    WORD_AT(controls_menu+0x68)=bits(200.f);WORD_AT(controls_menu+0x64)=bits(12.f);
    controls_anchor();CHECK(FLOAT_AT(controls_menu+0x64)==12.f);
    WORD_AT(controls_menu+0x68)=bits(120.f);WORD_AT(controls_menu+0x64)=bits(0.f);
    controls_anchor();CHECK(FLOAT_AT(controls_menu+0x64)==12.f);
    WORD_AT(controls_menu+0x68)=bits(240.f);controls_anchor();
    CHECK(FLOAT_AT(controls_menu+0x64)==12.f);
    controls_tab=2;kb_page=KB_CAPTURE;WORD_AT(controls_menu+0x64)=bits(55.f);
    controls_anchor();CHECK(FLOAT_AT(controls_menu+0x64)==55.f);
    { /* Section shortcuts work from a setting row, once per press, without
       * stealing the native A/D adjustment path or changing staged values. */
        controls_tab=0;kb_page=KB_HOME;
        WORD_AT(0x7ba178)=0x510000;WORD_AT(0x510000)=0x4321;WORD_AT(0x510008)=0x1234;
        WORD_AT(0x7ba16c)=1;unsigned sounds=video_sounds;
        settings_tabs_input_reset();call(0x2eab30,1,0);
        settings_keys[0x27]=1;call(0x2eab30,1,0);CHECK(controls_tab==1 && video_sounds==sounds+1);
        call(0x2eab30,CONTROLS_TABS_ID,0x80008);CHECK(controls_tab==1 && video_sounds==sounds+1);
        settings_keys[0x27]=0;call(0x2eab30,CONTROLS_TABS_ID,0);
        settings_keys[0x25]=1;call(0x2eab30,3,0);CHECK(controls_tab==0 && video_sounds==sounds+2);
        settings_keys[0x25]=0;settings_tabs_direction(controls_menu);
        settings_keys['A']=1;WORD_AT(controls_menu+0xc4)=0x40004;
        CHECK(!settings_tabs_direction(controls_menu) && WORD_AT(controls_menu+0xc4)==0x40004);
        settings_keys['A']=0;settings_keyboard=0;settings_pad_allowed=1;
        WORD_AT(controls_menu+0xc8)=0x200;call(0x2eab30,1,0x2000200);
        CHECK(controls_tab==2 && video_sounds==sounds+3);
        WORD_AT(controls_menu+0xc8)=0x200;call(0x2eab30,CONTROLS_TABS_ID,0x2000200);
        CHECK(controls_tab==2 && video_sounds==sounds+3);
        WORD_AT(controls_menu+0xc8)=0;call(0x2eab30,CONTROLS_TABS_ID,0);
        WORD_AT(controls_menu+0xc8)=0x400;call(0x2eab30,CONTROLS_TABS_ID,0x4000400);
        CHECK(controls_tab==0 && video_sounds==sounds+4);
        settings_keyboard=1;settings_pad_allowed=0;WORD_AT(controls_menu+0xc8)=0;
        unsigned h=highlight_sounds;settings_tabs_hover_reset();WORD_AT(controls_menu+0x18c)=1;
        settings_tabs_hover(controls_menu,-1,CONTROLS_TABS_ID);
        WORD_AT(controls_menu+0x18c)=CONTROLS_TABS_ID;
        settings_tabs_hover(controls_menu,0,CONTROLS_TABS_ID);CHECK(highlight_sounds==h);
        settings_tabs_hover(controls_menu,1,CONTROLS_TABS_ID);CHECK(highlight_sounds==h+1);
        settings_tabs_hover(controls_menu,1,CONTROLS_TABS_ID);CHECK(highlight_sounds==h+1);
        settings_keyboard=0;settings_tabs_hover(controls_menu,2,CONTROLS_TABS_ID);
        CHECK(highlight_sounds==h+1);settings_keyboard=1;
        WORD_AT(0x7ba16c)=0;settings_tabs_hover(controls_menu,2,CONTROLS_TABS_ID);
        settings_tabs_sound(8);CHECK(highlight_sounds==h+1 && video_sounds==sounds+4);
        WORD_AT(0x7ba16c)=1;WORD_AT(0x7ba178)=0;settings_tabs_sound(0);settings_tabs_sound(8);
        CHECK(highlight_sounds==h+1 && video_sounds==sounds+4);
        WORD_AT(0x7ba16c)=0;
    }
    { /* Video shortcuts preserve staged graphics choices across sections. */
        WORD_AT(current_menu+4)=storage;refresh(DISPLAY_GRAPHICS,0);
        WORD_AT(graphics_rows[2]+0xc8)=4;read_choices();
        settings_tabs_input_reset();call(TICK_CB,102,0);
        settings_keys[0x25]=1;call(TICK_CB,102,0x40004);
        CHECK(page==DISPLAY_HOME && staged_anisotropy==16);
        call(TICK_CB,VIDEO_TABS_ID,0x40004);CHECK(page==DISPLAY_HOME);
        settings_keys[0x25]=0;call(TICK_CB,VIDEO_TABS_ID,0);
        settings_keys[0x27]=1;call(TICK_CB,VIDEO_TABS_ID,0x80008);
        CHECK(page==DISPLAY_GRAPHICS && staged_anisotropy==16 && WORD_AT(graphics_rows[2]+0xc8)==4);
        settings_keys[0x27]=0;
    }
    { /* Local-player choices use native selectors and commit only on Apply. */
        controls_tab=0;controls_test_enabled=1;
        WORD_AT(controls_menu+4)=CONTROLS_DESCRIPTOR;
        controls_players_open(controls_menu);
        CHECK(kb_page==KB_PLAYERS && kb_devices[0]==-2 && kb_devices[1]==-1);
        CHECK(WORD_AT(kb_device_rows[0]+0xc8)==0 && WORD_AT(kb_device_rows[1]+0xc8)==0);
        WORD_AT(kb_device_rows[1]+0xc8)=1;
        call(KB_TICK,0,0);
        CHECK(kb_devices[1]==0 && test_player_saves==0);
        call(KB_SELECT,204,0);
        CHECK(test_player_saves==1 && test_player_devices[1]==0);
        call(KB_SELECT,205,0);CHECK(kb_page==KB_PLAYER_PICK);
        call(KB_SELECT,202,0);CHECK(kb_page==KB_PLAYER_WAIT && capture_active);
        call(KB_SELECT,206,0);CHECK(kb_page==KB_PLAYERS && !capture_active);
        call(KB_SELECT,205,0);call(KB_SELECT,202,0);
        test_pad_buttons=1;call(KB_TICK,0,0);
        CHECK(kb_page==KB_PLAYER_WAIT && kb_devices[2]==-1); /* Held button ignored. */
        test_pad_buttons=0;call(KB_TICK,0,0);
        test_pad_buttons=1;call(KB_TICK,0,0);
        CHECK(kb_page==KB_PLAYERS && kb_devices[2]==-1 && !capture_active);
        CHECK(strstr(kb_status,"already assigned to Player 2"));
        test_pad_buttons=0;call(KB_SELECT,205,0);call(KB_SELECT,202,0);call(KB_TICK,0,0);
        test_pad_buttons=2;call(KB_TICK,0,0);
        CHECK(kb_page==KB_PLAYERS && kb_devices[2]==1 && !capture_active);
        CHECK(test_player_saves==1 && test_player_devices[2]==-1); /* Still a draft. */
        call(KB_SELECT,204,0);CHECK(test_player_devices[2]==1 && test_player_saves==2);
        test_pad_buttons=0;call(KB_SELECT,205,0);call(KB_SELECT,203,0);
        clock_ms+=20001;call(KB_TICK,0,0);
        CHECK(kb_page==KB_PLAYERS && !capture_active && kb_devices[3]==-1);
        call(KB_SELECT,206,0);CHECK(controls_tab==0);
    }
    VirtualFree(mem,0,MEM_RELEASE);
    puts("PASS: unified display, disabled Apply, staged batch, FOV bounds/mouse, Back discard, failed-save rollback, resolution confirmation/timeout, texture synchronization and guest ABI");
    return 0;
}

STUB(sub_001A4350) STUB(sub_001A8CE0) STUB(sub_001A8EE0)

STUB(sub_001CC360)

void sub_000D2CE0(void){}
void sub_0015BCC0_gen(void){}
