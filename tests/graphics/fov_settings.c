/* Production display model and guest callbacks with isolated settings and RAM.
 * Original menu drawing/input functions are stubs; no game data is needed. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
static ULONGLONG clock_ms;
static ULONGLONG ticks(void) { return clock_ms; }
#define GetTickCount64 ticks
#include "../src/interface/pc_display.c"
#include "../src/interface/pc_display_menu.c"
/* Developer Tools is a separate guest-thread consumer. These menu fixtures
 * retain normal HUD visibility and reject accidental flight/Story actions. */
static PcVideoSnapshot sidebar_video_observed;
static PcVideoRequest sidebar_video_queued;
static int sidebar_open=1,sidebar_fullscreen;
int pc_developer_open(void){return sidebar_open;}
void pc_developer_video_register(void (*tick)(void)){(void)tick;}
void pc_developer_video_guest_tick(void){}
void pc_developer_video_publish(const PcVideoSnapshot *s){sidebar_video_observed=*s;}
int pc_developer_video_take(PcVideoRequest *r){*r=sidebar_video_queued;sidebar_video_queued.serial=0;return r->serial!=0;}
int pc_controls_developer_is_fullscreen(void){return sidebar_fullscreen;}
void pc_controls_developer_fullscreen(void){sidebar_fullscreen=!sidebar_fullscreen;}
void pc_developer_get(PcDeveloperState *out) {*out=(PcDeveloperState){0,0,0,0,1.,0};}
static int developer_hud_visible=1;
int pc_developer_hud_visible(void) {return developer_hud_visible;}
void pc_developer_close(void) {assert(!"unexpected Developer Tools close");}
void pc_developer_report(const char *message) {(void)message;assert(!"unexpected developer request");}
int pc_developer_flight_step(int mode,const float origin[3],const float direction[3],double seconds,float position[3],float forward[3])
{(void)mode;(void)origin;(void)direction;(void)seconds;(void)position;(void)forward;assert(!"unexpected flight");return 0;}
void sub_0025EF80(void) {assert(!"unexpected checkpoint capture");}
void sub_001519E0(void) {assert(!"unexpected camera view");}
void sub_00153270(void) {assert(!"unexpected camera composition");}
void sub_00152260(void) {assert(!"unexpected camera inverse");}
void sub_0006BB90(void) {assert(!"unexpected checkpoint lookup");}
void sub_00067450(void) {assert(!"unexpected mission ending");}
void pc_controls_apply_mouse(uint32_t player,uint32_t camera,ptrdiff_t offset)
{(void)player;(void)camera;(void)offset;assert(!"unexpected developer mouse update");}

int tsfp_input_icon_begin(const void *spans,unsigned count,unsigned size,unsigned pixels) {(void)spans;(void)count;(void)size;(void)pixels;return 0;}
void tsfp_input_icon_end(void) {}
void sub_0001B7B0(void) {g_esp+=4;}
const char *pc_build_version(void) { return "development"; }
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
static int available=1,returned;
static int enhanced_audio,audio_save_ok=1;
static PcAudioOptions audio_test_options={0,1,1,1,1};
int pc_audio_enhanced_enabled(void) {return enhanced_audio;}
int pc_audio_set_enhanced(int enabled) {if(!audio_save_ok)return 0;enhanced_audio=!!enabled;return 1;}
PcAudioOptions pc_audio_get_options(void) {audio_test_options.enhanced=enhanced_audio;return audio_test_options;}
int pc_audio_set_options(const PcAudioOptions *o) {if(!audio_save_ok)return 0;audio_test_options=*o;enhanced_audio=o->enhanced;return 1;}
static PcAudioMixOptions audio_mix_test_options=PC_AUDIO_MIX_DEFAULTS;
PcAudioMixOptions pc_audio_mix_get(void){return audio_mix_test_options;}
int pc_audio_mix_set(const PcAudioMixOptions *o){if(!audio_save_ok)return 0;audio_mix_test_options=*o;return 1;}
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
static HWND fixture_window;
static int fullscreen_request_ok=1;
int recomp_config_bool(const char *env,const char *key,int fallback)
{ (void)env;(void)key;return fallback; }
static LRESULT CALLBACK fullscreen_test_proc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    if(message==PC_FULLSCREEN_SET_MESSAGE) {
        if(!fullscreen_request_ok)return 0;
        SetWindowLongPtrA(window,GWL_STYLE,wp?WS_POPUP:WS_OVERLAPPEDWINDOW);return 1;
    }
    return DefWindowProcA(window,message,wp,lp);
}
HWND xbox_D3D8Window(void) { return fixture_window; }
int xbox_D3D8ChangeResolution(unsigned w,unsigned h) { (void)w;(void)h;return available; }
static unsigned online_connect_calls;
int pc_online_connect(void) { ++online_connect_calls;return 0; }
int pc_online_connect_mode(PcOnlineMode mode) { (void)mode;return 0; }
int pc_online_menu_start(void){return 0;}
int pc_online_menu_send(PcOnlineMenuRequest *request){(void)request;CHECK(0);return 0;}
const PcOnlineMenuState *pc_online_menu_poll(void){CHECK(0);return NULL;}
int pc_online_menu_accept(void){CHECK(0);return 0;}
int pc_online_menu_active(void){return 0;}
void pc_online_menu_leave(void){CHECK(0);}
int pc_online_menu_copy(const char *text){(void)text;CHECK(0);return 0;}
int pc_online_menu_paste(char *text,size_t bytes){(void)text;(void)bytes;CHECK(0);return 0;}
int pc_online_menu_publish(PcOnlineMatchInfo *info){(void)info;CHECK(0);return 0;}
PcOnlineMode pc_online_mode(void) { return PC_ONLINE_NONE; }
static unsigned online_disconnects,lan_browser_entries,lan_enables;
void pc_online_disconnect(void) { online_disconnects++; }
uint32_t xbox_HeapAlloc(uint32_t size,uint32_t alignment) {static uint32_t next=0x100000;next=(next+alignment-1)&~(alignment-1);uint32_t result=next;next+=size;CHECK(next<0x1f0000);return result;}
#define STUB(name) void name(void) { g_esp+=4; }
/* The menu translation unit also contains tutorial hooks. Their drawing and
 * input behavior is covered by tutorial_layout.c; these paths must not run
 * during the FOV/settings test. */
static int settings_keyboard=1,settings_pad_allowed;
static unsigned char settings_keys[256];
static unsigned physical_diagram_depth,physical_diagram_draws;
void pc_prompts_controller_layout_begin(void) {++physical_diagram_depth;}
void pc_prompts_controller_layout_end(void) {--physical_diagram_depth;}
void sub_002E97C0(void) {CHECK(physical_diagram_depth==1);++physical_diagram_draws;g_esp+=4;}
int pc_prompts_keyboard_active(void) { return settings_keyboard && !physical_diagram_depth; }
void sub_000402D0(void) {CHECK(0);}
void sub_0001A2E0(void) {CHECK(0);}
int pc_prompts_keycap_label(const char *label,size_t length) {(void)label;(void)length;CHECK(0);return 0;}
int pc_prompts_keycap_key(const char *label,size_t length) {(void)label;(void)length;CHECK(0);return -1;}
size_t pc_prompts_read_region(uintptr_t address) {(void)address;CHECK(0);return 0;}
int pc_controls_accept_pad(void) {return settings_pad_allowed;}
int pc_controls_binding_keys(unsigned char keys[256]) {memcpy(keys,settings_keys,256);return 1;}
int pc_controls_key_down(int key) {return key>0 && key<256?settings_keys[key]:0;}
void pc_prompts_layout_begin(void) {}
void pc_prompts_layout_end(void) {}
#define TUTORIAL_STUB(name) void name(void) { CHECK(0); }
TUTORIAL_STUB(sub_00147080) TUTORIAL_STUB(sub_00042D10)
static int row_alignment_test,update_notice_test,update_notice_draws;
void sub_0003FA10(void) {
    CHECK(row_alignment_test);g_eax=WORD_AT(0x6b93b4);WORD_AT(0x6b93b4)=WORD_AT(g_esp+4);g_esp+=4;
}
void sub_0003FCF0(void) {
    CHECK(row_alignment_test || update_notice_test);FLOAT_AT(WORD_AT(g_esp+4))=80.f;
    FLOAT_AT(WORD_AT(g_esp+8))=WORD_AT(0x6b93b4)==2?14.f:10.f;g_esp+=4;
}
void sub_000718E0(void) {CHECK(row_alignment_test);g_eax=0x600400;g_esp+=4;}
void sub_0003FB90(void) {CHECK(row_alignment_test);g_eax=WORD_AT(g_esp+4);g_esp+=4;}
void sub_0003F8D0(void) {CHECK(update_notice_test);g_esp+=4;}
/* Kill-feed drawing is linked through the same menu unit, but must not
 * execute in this settings-only fixture. */
TUTORIAL_STUB(sub_00041E30) TUTORIAL_STUB(sub_00021450)
TUTORIAL_STUB(sub_0001ED50) TUTORIAL_STUB(sub_0001F090)
#undef TUTORIAL_STUB
/* Mouse-menu behavior is exercised by menu_mouse.c. Keep its dependencies
 * linked here, but fail if this settings-only fixture enters those paths. */
void pc_controls_text_mode(uint32_t owner) {(void)owner;CHECK(0);}
unsigned pc_controls_text_event(uint32_t owner) {(void)owner;CHECK(0);return 0;}
void pc_settings_gameplay_reader(PcGameplayMasks reader) {(void)reader;}
static int controls_test_menu_active=1;
int pc_controls_in_menu(void) {return controls_test_menu_active;}
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
MOUSE_STUB(sub_001A2720) MOUSE_STUB(sub_001CBA50) MOUSE_STUB(sub_001CBD40)
MOUSE_STUB(sub_00352980) MOUSE_STUB(sub_00352990) MOUSE_STUB(sub_0007D090)
MOUSE_STUB(sub_001A4510) MOUSE_STUB(sub_000318A0) MOUSE_STUB(sub_003800D1)
MOUSE_STUB(sub_001CBDA0) MOUSE_STUB(sub_00064E20)
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
void pc_settings_modern(PcSettings *s) {pc_settings_defaults(s);s->preset=1;}
int pc_settings_custom_read(PcSettings *s,char *e,size_t n) {return pc_settings_read(s,e,n);}
static int test_custom_semantics;
int pc_settings_custom_semantics(void) {return test_custom_semantics;}
static int test_controller_presets[4];
static int test_deadzones[4][2]={{7849,7849},{7849,7849},{7849,7849},{7849,7849}};
int pc_settings_deadzone(unsigned p,int a) {CHECK(p<4);return test_deadzones[p][!!a];}
int pc_settings_deadzone_save(unsigned p,int a,int v,char *e,size_t n) {(void)e;(void)n;CHECK(p<4);test_deadzones[p][!!a]=v;return 1;}
int pc_settings_controller_preset(unsigned port) {CHECK(port<4);return test_controller_presets[port];}
int pc_settings_controller_preset_save(unsigned port,int value,char *e,size_t n) {(void)e;(void)n;CHECK(port<4);test_controller_presets[port]=value;return 1;}
static int test_player_devices[4]={-2,-1,-1,-1},test_player_saves;
void pc_settings_players_read(int d[4]) {memcpy(d,test_player_devices,sizeof test_player_devices);}
int pc_settings_players_save(const int d[4],char *e,size_t n) {(void)e;(void)n;memcpy(test_player_devices,d,sizeof test_player_devices);test_player_saves++;return 1;}
static unsigned test_pad_buttons,test_pad_connected=15;
static int join_port=-1;
static unsigned join_used;
int pc_settings_players_join(unsigned used){if(used>15)return -1;join_used=used;int p=join_port;join_port=-1;return p;}
void sub_00042D00(void){WORD_AT(0x6b93c0+WORD_AT(g_esp+4)*4)=WORD_AT(g_esp+8);g_esp+=4;}
void sub_00042870(void){g_esp+=4;}

int pc_settings_pad_status(int slot,char *name,size_t n,int *pressed) {snprintf(name,n,"Test controller %d",slot+1);*pressed=(test_pad_buttons>>slot)&1;return (test_pad_connected>>slot)&1;}
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
    if(controls_font_menu(menu) || audio_font_menu(menu)) {
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
    if(controls_font_menu(WORD_AT(g_esp+4)))controls_build();
    else if(audio_font_menu(WORD_AT(g_esp+4)))audio_options_build();else build_menu();
    /* Native refresh restores choices by row ID, even across different pages. */
    WORD_AT(0x200000+0xc8)=first;WORD_AT(0x200200+0xc8)=second;
    WORD_AT(0x200e00+0xc8)=textures;
}
STUB(sub_0007D480) STUB(sub_000717D0)
void sub_00075430(void) { WORD_AT(WORD_AT(g_esp+4)+0xc8)=WORD_AT(g_esp+8);g_esp+=4; }
STUB(sub_0003D7B0) STUB(sub_001CD120) STUB(sub_00069720) STUB(sub_00071CF0)
static unsigned audio_native_ticks,audio_native_actions,audio_native_opens;
static unsigned fixture_audio_rows(void) {
    unsigned mask=0;
    for(uint32_t row=WORD_AT(0x300178);row;row=WORD_AT(row+4)) {
        unsigned id=WORD_AT(row+0x14);if(id<8)mask|=1u<<id;
    }
    return mask;
}
void sub_002C3720(void) {CHECK((fixture_audio_rows()&0xefu)==0xefu);audio_native_ticks++;g_esp+=4;}
static void fixture_audio_action(void){CHECK((fixture_audio_rows()&0xefu)==0xefu);audio_native_actions++;g_esp+=4;}
void sub_002C3D30(void){fixture_audio_action();}
void sub_002C3AC0(void){fixture_audio_action();}
void sub_002C3BF0(void){fixture_audio_action();}
static void fixture_audio_open(void) {
    audio_native_opens++;WORD_AT(0x300004)=0x52377cu;
    guest_call(audio_options_build,1,0x300000u);g_esp+=4;
}
void sub_002CBE10(void){fixture_audio_open();}
void sub_002EC1F0(void){fixture_audio_open();}
static void fixture_audio_button(void){CHECK(audio_native_button(sub_00079D60_gen));}
void sub_002C3E20(void) {
    guest_call(sub_00075B70,5,0xa000u,audio_options_caption(),bits(54.f),0u,0x002C3D30u);
    WORD_AT(0x30017c)=WORD_AT(0x300180)=0;
    for(unsigned id=0;id<8;id++) {
        if(id==4)continue;
        if(id==6)guest_call(sub_00079710,1,0u);
        guest_call(fixture_audio_button,6,audio_options_caption(),id,0u,bits(6.f),0u,0u);
    }
    WORD_AT(0x300064)=bits(100.f-audio_page*10.f);g_esp+=4;
}
 STUB(sub_002D9390) STUB(sub_00072510) STUB(sub_00022FF0) STUB(sub_0001C5D0) STUB(sub_00019650) STUB(sub_00019D70)
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
STUB(sub_00197890) STUB(sub_002CA9F0) STUB(sub_003535C0) STUB(sub_003544B0) STUB(sub_00359EB0) STUB(sub_00198E20) STUB(sub_00040F00)
STUB(sub_00080F20)
STUB(sub_002CAD80)
static float drawn;
void sub_000814B0(void) { uint32_t b=WORD_AT(g_esp+4);memcpy(&drawn,&b,4);g_esp+=4; }
void sub_00154FE0(void) { g_eax=WORD_AT(g_esp+4);g_esp+=4; }
void sub_00079B70(void) { returned++;g_esp+=4; }
void sub_00079D60_gen(void) {
    unsigned id=WORD_AT(g_esp+8);CHECK(id<256);
    if(page==DISPLAY_HOME && id>=20 && id<=21)CHECK(!strncmp((char *)(g_xbox_mem_offset+WORD_AT(g_esp+4)),"\vs75",4));
    if(audio_building && (id==1 || id==3))CHECK(WORD_AT(g_esp+24)==AUDIO_VOLUME_DRAW);
    if(audio_building && id==AUDIO_TABS_ID)CHECK(!strcmp((char *)(g_xbox_mem_offset+WORD_AT(g_esp+4))," "));
    if(id<32)built_rows|=1u<<id;CHECK(built_row_count<32);built_ids[built_row_count++]=id;
    g_eax=0x200000+id*0x200;WORD_AT(g_eax)=4;WORD_AT(g_eax+0xb0)=1;WORD_AT(g_eax+0x14)=id;fixture_link(g_eax);WORD_AT(g_eax+0x28)=WORD_AT(g_esp+12)|0x2000u;WORD_AT(g_eax+0x88)=current_menu;g_esp+=4;
}
void sub_00041DC0(void) {
    CHECK(update_notice_test);
    CHECK(!strcmp((char *)(g_xbox_mem_offset+WORD_AT(g_esp+12)),"UPDATE AVAILABLE - OPEN THE LAUNCHER"));
    CHECK(FLOAT_AT(g_esp+4)==280.f && FLOAT_AT(g_esp+8)==398.f);
    update_notice_draws++;g_esp+=4;
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
    uint32_t audio_id=WORD_AT(g_esp+8);
    g_eax=audio_id>=15 && audio_id<=19?0x510000+(audio_id-15)*0x200:0x510000;g_esp+=4;
}
static void reload(const char *text)
{
    FILE *f=fopen(path,"wb");CHECK(f);CHECK(fputs(text,f)>=0);CHECK(!fclose(f));
    initialized=0;current=(PcDisplayMode){0,0};vertical_fov=PC_FOV_DEFAULT;weapon_fov=PC_WEAPON_FOV_DEFAULT;
    pc_display_current();
}
static void content(const char *expected)
{
    FILE *f=fopen(path,"rb");char text[128]={0};CHECK(f);fread(text,1,sizeof text-1,f);fclose(f);
    CHECK(!strcmp(text,expected));
}
static void nested_match_options(void) {
    CHECK(WORD_AT(0x78ad2c)==9);
    g_esp-=4;WORD_AT(g_esp+4)=0x300000;
    pc_network_bots_menu(sub_002C78F0_gen);
    CHECK(WORD_AT(0x78ad2c)==9);
    g_esp+=4;
}
static void fixture_profile_initialized(void) {g_esp+=4;}
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
    { /* Main-only visibility; late update checks, expiry and font/register restoration. */
        char path[1024];snprintf(path,sizeof path,"%s.update",argv[1]);
        CHECK(!_putenv_s("TSFP_GAME_UPDATE_NOTICE",path));CHECK(!_putenv_s("TSFP_GAME_INSTALLED_VERSION","1.2.3"));
        FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"TSFP_GAME_UPDATE_V1 %lld v1.2.3 v1.2.4\n",(long long)time(NULL));fclose(f);
        uint32_t menu=0x43000;WORD_AT(menu+4)=0x1234;WORD_AT(menu+0x1b0)=3;
        settings_footer_menu=menu;settings_footer_x=bits(1.f);settings_footer_y=bits(1.f);
        WORD_AT(0x4e8cf0)=640;WORD_AT(0x4e8cf4)=480;
        WORD_AT(0x7e7270)=bits(2.f);WORD_AT(0x7e7280)=bits(3.f);WORD_AT(0x7e7260)=4;
        g_esp=0x700000;update_notice_test=1;main_update_notice_open(menu);
        main_update_notice_draw(menu);CHECK(update_notice_draws==1);
        CHECK(g_esp==0x700000 && FLOAT_AT(0x7e7270)==2.f && FLOAT_AT(0x7e7280)==3.f && WORD_AT(0x7e7260)==4);
        WORD_AT(menu+0x170)=0x44000;main_update_notice_draw(menu);CHECK(update_notice_draws==1);
        WORD_AT(menu+0x170)=0;WORD_AT(menu+4)=0x5678;main_update_notice_draw(menu);CHECK(update_notice_draws==1);
        WORD_AT(menu+4)=0x1234;WORD_AT(menu+0x38)=1;main_update_notice_draw(menu);CHECK(update_notice_draws==1);
        WORD_AT(menu+0x38)=0;WORD_AT(menu+0x1b0)=2;main_update_notice_draw(menu);CHECK(update_notice_draws==1);
        WORD_AT(menu+0x1b0)=3;main_update_notice_draw(menu+0x1000);CHECK(update_notice_draws==1);
        f=fopen(path,"wb");CHECK(f);fclose(f);update_notice_polled=0;
        main_update_notice_draw(menu);CHECK(update_notice_draws==1 && !update_notice_available);
        f=fopen(path,"wb");CHECK(f);fprintf(f,"TSFP_GAME_UPDATE_V1 %lld v1.2.3 v1.2.4\n",(long long)time(NULL));fclose(f);
        update_notice_polled=0;main_update_notice_draw(menu);CHECK(update_notice_draws==2);
        update_notice_test=0;settings_footer_menu=0;update_notice_menu=0;
        CHECK(!_putenv_s("TSFP_GAME_UPDATE_NOTICE",""));CHECK(!_putenv_s("TSFP_GAME_INSTALLED_VERSION",""));remove(path);
    }
    g_esp=0x700000;pc_display_menu_lookup(0x002E97C0u)();
    CHECK(physical_diagram_draws==1 && !physical_diagram_depth && g_esp==0x700004);
    { /* New native profiles seed editable Modern; existing profiles stay byte-identical. */
        uint32_t profile=0x44000,identity=0x48000;unsigned char retained[0x7c];
        WORD_AT(profile)=identity;WORD_AT(identity+0x24)=0;WORD_AT(profile+4)=1;
        const unsigned stock[14]={0x800,0x2000,0x80,0x40,0x400,0x10,4,8,1,2,0,4,8,0x1000};
        for(unsigned i=0;i<22;i++)WORD_AT(profile+8+i*4)=i<14?stock[i]:0xdead0000u+i;
        memcpy(retained,(void *)(g_xbox_mem_offset+profile+4),sizeof retained);
        test_custom_semantics=1;g_eax=profile;pc_unlock_profile_init(fixture_profile_initialized);
        CHECK(!memcmp(retained,(void *)(g_xbox_mem_offset+profile+4),sizeof retained));
        WORD_AT(identity+0x24)=UINT32_MAX;test_custom_semantics=0;g_eax=profile;pc_unlock_profile_init(fixture_profile_initialized);
        CHECK(!memcmp(retained,(void *)(g_xbox_mem_offset+profile+4),sizeof retained));
        test_custom_semantics=1;g_eax=profile;pc_unlock_profile_init(fixture_profile_initialized);
        CHECK(!(WORD_AT(profile+4)&0xffffu));
        const unsigned modern_masks[14]={0x800,0x8000,0x80,0x10,0x400,0x20,4,8,1,2,0,4,8,0x2000};
        for(unsigned i=0;i<14;i++)CHECK(WORD_AT(profile+8+i*4)==modern_masks[i]);
        CHECK(WORD_AT(profile+8+10*4)==0); /* Unused native field stays unused. */
        CHECK(WORD_AT(profile+8+4*4)==0x400u); /* aim: LT */
        CHECK(WORD_AT(profile+8+3*4)==0x10u); /* reload: X */
        CHECK(WORD_AT(profile+8+13*4)==0x2000u); /* melee: RS */
        CHECK(WORD_AT(profile+8+1*4)==0x8000u); /* grenade: RB */
        CHECK(WORD_AT(profile+8+5*4)==0x20u); /* Native editor: one B token; modern LS routes to it. */
        const unsigned axes[8]={2,1,4,3,2,1,4,3};
        for(unsigned i=0;i<8;i++)CHECK(WORD_AT(profile+8+(14+i)*4)==axes[i]);
        /* An existing broken fresh seed repairs; configured/custom axes survive. */
        WORD_AT(identity+0x24)=0;
        WORD_AT(profile+8+5*4)=0x1020u;
        WORD_AT(profile+8+6*4)=WORD_AT(profile+8+11*4)=0x4004u;
        WORD_AT(profile+8+9*4)=0x42u;
        for(unsigned i=0;i<8;i++)WORD_AT(profile+8+(14+i)*4)=0;
        g_eax=profile;pc_unlock_profile_init(fixture_profile_initialized);
        for(unsigned i=0;i<8;i++)CHECK(WORD_AT(profile+8+(14+i)*4)==axes[i]);
        WORD_AT(profile+8+14*4)=3;g_eax=profile;pc_unlock_profile_init(fixture_profile_initialized);
        CHECK(WORD_AT(profile+8+14*4)==3);
        test_custom_semantics=0;
    }
    { /* Only the local port owning this native player receives its action masks. */
        unsigned masks[24]={0};int mounted=0;uint32_t player=0x500000;
        WORD_AT(0x790950)=1;WORD_AT(0x7b0c48)=player;WORD_AT(player)=2;WORD_AT(player+4)=2;WORD_AT(0x6b93c8)=3;
        WORD_AT(player+0x18)=0x10;WORD_AT(player+0x1c)=0x400;WORD_AT(player+0x20)=0x80;
        WORD_AT(player+0x24)=0x4000;WORD_AT(player+0x28)=0x8000;WORD_AT(player+0x2c)=0x40;WORD_AT(player+0x4c)=0x20;
        CHECK(!controls_gameplay_masks(3,masks,&mounted));controls_test_menu_active=0;
        CHECK(!controls_gameplay_masks(0,masks,&mounted));
        CHECK(controls_gameplay_masks(3,masks,&mounted) && masks[9]==0x10 && masks[23]==0x8000 && masks[18]==0x40);
        WORD_AT(player+0x50)=4;WORD_AT(player+0x54)=3;
        WORD_AT(player+0x58)=2;WORD_AT(player+0x5c)=1;
        CHECK(controls_gameplay_masks(3,masks,&mounted));
        CHECK(masks[14]==(1u<<19) && masks[17]==(1u<<22));
        CHECK(masks[19]==(1u<<14) && masks[22]==(1u<<17));
        CHECK(masks[2]==0x4000 && masks[1]==0x20 && masks[8]==0x400 && !mounted && masks[0]==0x80);
        WORD_AT(player+0x114)=0x80;CHECK(controls_gameplay_masks(3,masks,&mounted) && mounted && masks[0]==0x4000);
        WORD_AT(0x79094c)=101;CHECK(!controls_gameplay_masks(3,masks,&mounted));WORD_AT(0x79094c)=0;
        WORD_AT(0x6b98a4)=2;CHECK(!controls_gameplay_masks(3,masks,&mounted));WORD_AT(0x6b98a4)=0;
        CHECK(WORD_AT(player+0x20)==0x80); /* Reading a preset never rewrites the native profile. */
        WORD_AT(0x790950)=WORD_AT(0x7b0c48)=WORD_AT(0x6b93c8)=0;memset((char *)mem+player,0,0x1584);
        controls_test_menu_active=1;
    }

    { /* Guest global slot1 is local input0; the remote host has no local owner. */
        uint32_t host=0x500000,guest=host+0x1584;unsigned masks[24]={0};int mounted;
        WORD_AT(0x790950)=2;WORD_AT(0x7b0c48)=host;controls_test_menu_active=0;
        WORD_AT(host)=0;WORD_AT(host+4)=UINT32_MAX;WORD_AT(host+0x18)=0x20;WORD_AT(host+0x114)=0x80;
        WORD_AT(guest)=1;WORD_AT(guest+4)=0;WORD_AT(guest+0x18)=0x10;
        WORD_AT(guest+0x24)=0x4000;WORD_AT(guest+0x28)=0x8000;
        WORD_AT(0x6b93c0)=0;WORD_AT(0x6b93c4)=1;
        WORD_AT(0x7b0c7c)=guest;
        CHECK(controls_context_port(WORD_AT(0x7b0c7c))==0 && controls_context_port(host)==4);
        CHECK(controls_context_port(0)==4);

        CHECK(controls_gameplay_masks(0,masks,&mounted) && masks[9]==0x10 && masks[2]==0x4000 && masks[23]==0x8000 && !mounted);
        CHECK(!controls_gameplay_masks(1,masks,&mounted));
        /* Reassigned input ports and split-screen owners also use +4, not +0. */
        WORD_AT(0x6b93c0)=3;WORD_AT(0x6b93c4)=2;WORD_AT(host+4)=1;
        CHECK(controls_context_port(WORD_AT(0x7b0c7c))==3 && controls_context_port(host)==2);
        CHECK(controls_gameplay_masks(3,masks,&mounted) && masks[9]==0x10 && !mounted);
        CHECK(controls_gameplay_masks(2,masks,&mounted) && masks[9]==0x20 && mounted);
        WORD_AT(host+4)=UINT32_MAX;WORD_AT(guest+4)=4;
        CHECK(!controls_gameplay_masks(2,masks,&mounted) && !controls_gameplay_masks(3,masks,&mounted));
        memset((char *)mem+host,0,2*0x1584);
        WORD_AT(0x790950)=WORD_AT(0x7b0c48)=WORD_AT(0x7b0c7c)=WORD_AT(0x6b93c0)=WORD_AT(0x6b93c4)=0;
        controls_test_menu_active=1;
    }

    { /* No startup network overrides: the real row callback must enable LAN. */
        CHECK(!_putenv_s("TSFP_NETWORK",""));
        CHECK(!_putenv_s("TSFP_ONLINE_CONFIG","keep-online"));
        call(0x001A2700u,0,0);call(0x001A2700u,7,0);
        CHECK(!online_disconnects && !lan_browser_entries && !lan_enables);
        CHECK(!strcmp(getenv("TSFP_ONLINE_CONFIG"),"keep-online"));
        call(0x001A2700u,8,0);
        CHECK(online_disconnects==1 && lan_browser_entries==1 && lan_enables==1);
        call(0x001A2700u,8,0);
        CHECK(online_disconnects==2 && lan_browser_entries==2 && lan_enables==2);
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
    CHECK(pc_display_weapon_fov()==100 && pc_display_weapon_fov_scale()==1.f);
    CHECK(!pc_display_set_weapon_fov(75) && !pc_display_set_weapon_fov(155) && !pc_display_set_weapon_fov(103));
    CHECK(pc_display_set_weapon_fov(130) && pc_display_save());
    content("TSFP_DISPLAY_V3 1280 720 55 130\n");
    initialized=0;weapon_fov=100;CHECK(pc_display_weapon_fov()==130);
    CHECK(pc_display_set_weapon_fov(100) && pc_display_save());

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
    CHECK(page==DISPLAY_HOME && built_row_count==8 && separators==2 && separator_positions[0]==1 && separator_positions[1]==6 && window_mode_row && aspect_row && rate_row && !subtitle_row && !texture_row && !counter_row);
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
    /* Weapon aperture is staged across tabs, applies live, and persists separately from world FOV. */
    refresh(DISPLAY_VIEW,0);
    for(int i=0;i<50;i++)call(TICK_CB,12,0x80000);CHECK(staged_weapon_fov==150);
    for(int i=0;i<50;i++)call(TICK_CB,12,0x40000);CHECK(staged_weapon_fov==80);
    slider_capture_enabled=1;
    memset(menu_slider_tracks,0,sizeof menu_slider_tracks);
    WORD_AT(0x700008)=weapon_fov_row;
    call(FOV_DRAW_CB,12,0);CHECK(drawn==0.f);
    CHECK(menu_slider_tracks[MENU_WEAPON_FOV].item==weapon_fov_row);
    WORD_AT(0x300198)=weapon_fov_row;
    pointer.left=1;
    CHECK(menu_slider_pointer(0x300000,pointer,2,348,8,1920,1080));
    CHECK(staged_weapon_fov==115 && staged_fov==70 && pc_display_weapon_fov()==100);
    pointer.left=0;CHECK(!menu_slider_pointer(0x300000,pointer,0,348,8,1920,1080));
    slider_capture_enabled=0;
    menu_slider_weapon_fov_pointer(398,298,100);CHECK(staged_weapon_fov==150);
    for(int i=0;i<4;i++)call(TICK_CB,12,0x40000);
    CHECK(staged_weapon_fov==130 && pc_display_weapon_fov()==100 && display_dirty());
    refresh(DISPLAY_HOME,0);refresh(DISPLAY_VIEW,0);
    CHECK(staged_weapon_fov==130);
    call(SELECT_CB,3,0);CHECK(pc_display_weapon_fov()==130 && pc_display_fov()==70 && !display_dirty());
    content("TSFP_DISPLAY_V3 1280 720 70 130\n");
    for(int i=0;i<6;i++)call(TICK_CB,12,0x40000);call(SELECT_CB,3,0);CHECK(pc_display_weapon_fov()==100);
    /* A failed SMAA save also rolls back every Apply-only graphics field. */
    TsfpGraphicsOptions before_failed_batch=pc_graphics_get();
    staged_anisotropy=4;staged_ao=!before_failed_batch.ambient_occlusion;staged_cache=!before_failed_batch.shader_cache;
    staged_msaa=4;staged_weapon_fov=150;
    refresh(DISPLAY_HOME,0);staged_fov=75;WORD_AT(rate_row+0xc8)=2;read_choices();
    refresh(DISPLAY_GRAPHICS,0);WORD_AT(post_aa_row+0xc8)=2;smaa_save_ok=0;
    call(SELECT_CB,3,0);
    CHECK(pc_display_weapon_fov()==100);
    CHECK(pc_display_fov()==70 && requested_rate==120 && pc_frame_rate_read()==120 && !pc_smaa_enabled() && graphics_options.msaa==1 && staged_msaa==4 && page!=DISPLAY_RESTART);
    content("TSFP_DISPLAY_V2 1280 720 70\n");
    CHECK(graphics_options.anisotropy==before_failed_batch.anisotropy &&
          graphics_options.ambient_occlusion==before_failed_batch.ambient_occlusion &&
          graphics_options.shader_cache==before_failed_batch.shader_cache);
    smaa_save_ok=1;staged_weapon_fov=100;staged_fov=70;staged_rate=applied_rate;staged_aa=applied_aa;staged_msaa=graphics_options.msaa;
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
    CHECK(SetFileAttributesA(rate_path,FILE_ATTRIBUTE_NORMAL));
    /* A final texture-save failure rolls back the earlier display/rate/AA saves. */
    char texture_path[32768];strcpy(texture_path,rate_path);
    char *texture_name=strrchr(texture_path,'\\');
    if(!texture_name)texture_name=strrchr(texture_path,'/');
    CHECK(texture_name);strcpy(texture_name+1,"pc-textures.ini");
    CHECK(SetFileAttributesA(texture_path,FILE_ATTRIBUTE_READONLY));
    staged_aa=1;call(SELECT_CB,3,0);
    CHECK(pc_display_fov()==70 && requested_rate==120 && tsfp_texture_is_4k() && WORD_AT(0x400130)==1);
    CHECK(display_dirty() && *status && staged_aa==1 && applied_aa==0 && pc_post_aa_get()==0);
    CHECK(SetFileAttributesA(texture_path,FILE_ATTRIBUTE_NORMAL));staged_aa=0;call(SELECT_CB,3,0);
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
    /* Window mode stages across tabs, previews on Apply, and persists only
     * after Keep. Timeout/Back/save failure restore the previous host mode. */
    {
        WNDCLASSA cls={0};cls.lpfnWndProc=fullscreen_test_proc;cls.hInstance=GetModuleHandleA(NULL);cls.lpszClassName="TsfpFullscreenTest";
        CHECK(RegisterClassA(&cls));
        fixture_window=CreateWindowExA(0,cls.lpszClassName,"",WS_OVERLAPPEDWINDOW,0,0,640,480,NULL,NULL,cls.hInstance,NULL);CHECK(fixture_window);
        call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);
        CHECK(window_mode_row && !pc_display_fullscreen() && !display_dirty());
        WORD_AT(window_mode_row+0xc8)=1;read_choices();CHECK(display_dirty() && !pc_display_fullscreen());
        refresh(DISPLAY_VIEW,0);CHECK(staged_fullscreen==1 && !pc_display_fullscreen());
        call(BACK_CB,0,0);call(PC_DISPLAY_MENU_CALLBACK,0,0);call(BUILD_CB,0,0);CHECK(!staged_fullscreen);
        WORD_AT(window_mode_row+0xc8)=1;call(SELECT_CB,3,0);CHECK(page==DISPLAY_CONFIRM && pc_display_fullscreen());
        call(BACK_CB,0,0);CHECK(page==DISPLAY_HOME && !pc_display_fullscreen());
        WORD_AT(window_mode_row+0xc8)=1;call(SELECT_CB,3,0);clock_ms+=16000;call(TICK_CB,0,0);CHECK(!pc_display_fullscreen() && page==DISPLAY_HOME);
        WORD_AT(window_mode_row+0xc8)=1;call(SELECT_CB,3,0);call(SELECT_CB,0,0);
        CHECK(pc_display_fullscreen() && previous_fullscreen && !display_dirty() && page==DISPLAY_HOME);
        char text[512]={0};FILE *file=fopen(runtime_path,"rb");CHECK(file);fread(text,1,sizeof text-1,file);fclose(file);
        CHECK(strstr(text,"fullscreen = 1") && strstr(text,"future_key = keep") && strstr(text,"frame_cap = 30"));
        WORD_AT(window_mode_row+0xc8)=0;call(SELECT_CB,3,0);CHECK(!pc_display_fullscreen());
        CHECK(SetFileAttributesA(runtime_path,FILE_ATTRIBUTE_READONLY));call(SELECT_CB,0,0);
        CHECK(pc_display_fullscreen() && display_dirty() && *status && page==DISPLAY_HOME);
        CHECK(SetFileAttributesA(runtime_path,FILE_ATTRIBUTE_NORMAL));fullscreen_request_ok=0;
        call(SELECT_CB,3,0);CHECK(page==DISPLAY_HOME && pc_display_fullscreen() && *status);
        fullscreen_request_ok=1;call(SELECT_CB,3,0);call(SELECT_CB,0,0);CHECK(!pc_display_fullscreen() && !display_dirty());
        CHECK(DestroyWindow(fixture_window));fixture_window=NULL;CHECK(UnregisterClassA(cls.lpszClassName,cls.hInstance));
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

    built_row_count=0;last_built_item=0;CHECK(audio_menu_prepare());audio_feature_add();
    uint32_t audio_beta_row=audio_feature_rows[0];CHECK(audio_beta_row && built_row_count==5);
    WORD_AT(audio_beta_row+0xc8)=1;call(AUDIO_BETA_CHANGE,0,0);CHECK(enhanced_audio==1);
    audio_save_ok=0;WORD_AT(audio_beta_row+0xc8)=0;call(AUDIO_BETA_CHANGE,0,0);
    CHECK(enhanced_audio==1 && WORD_AT(audio_beta_row+0xc8)==1);
    audio_save_ok=1;WORD_AT(audio_beta_row+0xc8)=0;call(AUDIO_BETA_CHANGE,0,0);CHECK(enhanced_audio==0);
    WORD_AT(audio_feature_rows[2]+0xc8)=0;call(AUDIO_BETA_CHANGE,0,0);
    CHECK(!audio_test_options.weapon_range && audio_test_options.spatial && audio_test_options.environment && audio_test_options.ambience);
    WORD_AT(audio_beta_row+0xc8)=1;call(AUDIO_BETA_CHANGE,0,0);
    CHECK(enhanced_audio && !audio_test_options.weapon_range);
    audio_save_ok=0;WORD_AT(audio_feature_rows[3]+0xc8)=0;call(AUDIO_BETA_CHANGE,0,0);
    CHECK(audio_test_options.environment && WORD_AT(audio_feature_rows[3]+0xc8)==1);audio_save_ok=1;
    /* The native profile descriptor survives every tab: hidden save/update
     * records stay accessible, visible links exclude them, and Y stays fixed. */
    /* Caption and value are measured independently, including cell-origin
     * differences. Restore bit-exact cached offsets after the paint scope. */
    row_alignment_test=1;
    uint32_t align_row=0x600500,align_rect=0x600700,align_menu=0x600900;
    WORD_AT(align_row)=4;WORD_AT(align_row+0x88)=align_menu;WORD_AT(align_row+0x14)=30;
    WORD_AT(align_row+0x20)=WORD_AT(0x600404)=0x600800;
    WORD_AT(align_row+0x1c)=1;WORD_AT(align_row+0xbc)=2;WORD_AT(align_row+0xb4)=1;
    FLOAT_AT(align_row+0x40)=-5.f;FLOAT_AT(align_row+0xe0)=99.f;
    FLOAT_AT(align_row+0x68)=96.f;FLOAT_AT(align_row+0xe8)=100.f;
    FLOAT_AT(align_rect+4)=40.f;FLOAT_AT(align_rect+12)=100.f;FLOAT_AT(0x7ba188)=3.f;
    WORD_AT(0x6b93b4)=7;
    SettingsRowPaint align_scope;settings_rows_begin(&align_scope,align_menu);
    settings_row_center(align_menu,align_row,align_rect);
    CHECK(fabsf(FLOAT_AT(align_row+0x40)*3.f+5.f-30.f)<.001f);
    CHECK(fabsf((FLOAT_AT(align_row+0xe0)+4.f)*3.f+7.f-30.f)<.001f);
    CHECK(WORD_AT(0x6b93b4)==7 && align_scope.count==1);
    settings_row_center(align_menu,align_row,align_rect);CHECK(align_scope.count==1);
    settings_rows_end(&align_scope);CHECK(FLOAT_AT(align_row+0x40)==-5.f && FLOAT_AT(align_row+0xe0)==99.f);
    CHECK(!settings_row_paint);
    WORD_AT(0x6b93b8)=0x601000;WORD_AT(0x601008)=0x601100;
    WORD_AT(0x600800)='A';WORD_AT(0x601100+33*12+4)=2u<<16;
    FLOAT_AT(0x601100+33*12+8)=3.f;FLOAT_AT(0x7e7290)=2.f;
    CHECK(settings_row_text_top(0x600800)==6.f);
    settings_rows_begin(&align_scope,align_menu);settings_row_center(align_menu,align_row,align_rect);
    CHECK(fabsf(FLOAT_AT(align_row+0x40)*3.f+(6.f+10.f)*.5f-30.f)<.001f);
    settings_rows_end(&align_scope);CHECK(FLOAT_AT(align_row+0x40)==-5.f);
    WORD_AT(0x600800)=11;CHECK(settings_row_text_top(0x600800)==0.f); /* native escapes */
    WORD_AT(align_menu+0x1c)=0x601800;WORD_AT(0x601848)=2;
    WORD_AT(0x600800)='A';
    uint32_t prior_flags=WORD_AT(align_row+0x28);
    settings_rows_begin(&align_scope,align_menu);settings_row_center(align_menu,align_row,align_rect);
    CHECK(WORD_AT(align_row+0x1c)==2 && WORD_AT(align_row+0xbc)==2);
    CHECK(WORD_AT(align_row+0x28)&0x2000u);
    CHECK(settings_tabs_font(align_menu,9)==2);
    settings_rows_end(&align_scope);
    CHECK(WORD_AT(align_row+0x1c)==1 && WORD_AT(align_row+0xbc)==2);
    CHECK(WORD_AT(align_row+0x28)==prior_flags);
    WORD_AT(0x600800)=11; /* Native scale escape: uppercase must not alter its command. */
    settings_rows_begin(&align_scope,align_menu);settings_row_center(align_menu,align_row,align_rect);
    CHECK(WORD_AT(align_row+0x28)==prior_flags);
    settings_rows_end(&align_scope);
    WORD_AT(align_menu+0x1c)=0;
    WORD_AT(0x6b93b8)=0;row_alignment_test=0;
    /* Native separator height is scoped, symmetric and never changes a skin permanently. */
    WORD_AT(0x600100+0x1c)=0x600200;FLOAT_AT(0x600230)=1.f;
    SettingsDividerStyle spacing=settings_divider_begin(0x600100);
    CHECK(fabsf(FLOAT_AT(0x600230)-2.2f)<.0001f);settings_divider_end(spacing);
    CHECK(FLOAT_AT(0x600230)==1.f && !settings_divider_begin(0).address);
    uint32_t before_audio_descriptor=WORD_AT(0x300004);
    WORD_AT(0x300004)=0x52377cu;WORD_AT(0x7bb1e0)=0x500000;
    call(0x002CBE10u,0,0);CHECK(audio_page==0 && audio_native_opens==1);
    CHECK(audio_font_menu(audio_menu) && audio_row_count==10 && fixture_audio_rows()==0xcfu);
    CHECK(WORD_AT(audio_menu+0x50)==bits(78.f) && separators==2);
    /* Compact bars occupy the value column, and mouse capture matches paint. */
    float audio_bar_bounds[]={100.f,40.f,500.f,70.f};
    memcpy((void *)(g_xbox_mem_offset+0x600000),audio_bar_bounds,sizeof audio_bar_bounds);
    slider_capture_enabled=1;
    for(unsigned id=1;id<=3;id+=2) {
        WORD_AT(id==1?0x5236bc:0x5236c0)=id==1?8u:12u;
        g_esp=0x700000;WORD_AT(g_esp+4)=audio_menu;WORD_AT(g_esp+8)=0x200000+id*0x200;
        WORD_AT(g_esp+12)=0x600000;audio_volume_draw();
        MenuSliderTrack *track=&menu_slider_tracks[id==1?MENU_MUSIC:MENU_SOUND];
        CHECK(track->x==378.f && track->y==51.f && track->w==100.f && track->h==8.f);
        CHECK(track->item==0x200000+id*0x200 && drawn==(id==1?.5f:.75f) && g_esp==0x700004);
    }
    slider_capture_enabled=0;
    for(int tab=1;tab<4;tab++) {
        audio_change_tab(tab);CHECK(audio_page==tab && audio_native_opens==1);
        CHECK(WORD_AT(audio_menu+0x64)==bits(90.f) && fixture_audio_rows()==0xc0u);
        CHECK(WORD_AT(audio_menu+0x18c)==AUDIO_TABS_ID && separators==2);
        if(tab==1) {
            slider_capture_enabled=1;
            for(unsigned i=0;i<6;i++) {
                menu_slider_audio_mix_pointer(i,378.f+i*17.f,378.f,100.f);
                CHECK(WORD_AT(audio_mix_rows[i]+0xc8)==i*17);
                g_esp=0x700000;WORD_AT(g_esp+4)=audio_menu;WORD_AT(g_esp+8)=audio_mix_rows[i];
                WORD_AT(g_esp+12)=0x600000;audio_mix_draw();
                CHECK(drawn==i*17.f/100.f && menu_slider_tracks[MENU_AUDIO_MIX+i].item==audio_mix_rows[i] &&
                      menu_slider_tracks[MENU_AUDIO_MIX+i].y==51.f);
            }
            CHECK(pc_audio_mix_get().master==0 && pc_audio_mix_get().volume[4]==85);
            menu_slider_audio_mix_pointer(5,-100.f,0.f,100.f);CHECK(pc_audio_mix_get().volume[4]==0);
            menu_slider_audio_mix_pointer(5,200.f,0.f,100.f);CHECK(pc_audio_mix_get().volume[4]==100);
            audio_save_ok=0;menu_slider_audio_mix_pointer(5,25.f,0.f,100.f);
            CHECK(WORD_AT(audio_mix_rows[5]+0xc8)==100 && strstr((char *)(g_xbox_mem_offset+audio_text(7)),"100%"));
            audio_save_ok=1;call(0x002C3720u,35,0x40000u);CHECK(pc_audio_mix_get().volume[4]==99);
            slider_capture_enabled=0;
        }
        audio_link_rows(1);CHECK(fixture_audio_rows()==0xefu);audio_link_rows(0);
        call(0x002C3720u,AUDIO_TABS_ID,0);CHECK(audio_page==tab);
    }
    unsigned sounds_before=video_sounds;audio_change_tab(3);CHECK(video_sounds==sounds_before);
    audio_change_tab(0);CHECK(fixture_audio_rows()==0xcfu && audio_native_ticks==4);
    call(0x002C3AC0u,6,0);CHECK(audio_native_actions==1 && fixture_audio_rows()==0xefu);
    g_esp=0x700000;WORD_AT(g_esp+4)=audio_menu;audio_options_build();
    call(0x002C3BF0u,7,0);
    g_esp=0x700000;WORD_AT(g_esp+4)=audio_menu;audio_options_build();
    call(0x002C3D30u,AUDIO_TABS_ID,0);CHECK(audio_native_actions==3);
    /* Fast mouse clicks retain the latest target through native cooldown;
     * repeated clicks on the active tab stay there in frontend and pause. */
    tab_pointer_enabled=slider_capture_enabled=1;
    WORD_AT(0x761c48)=0x410000;WORD_AT(0x410000)=1;
    WORD_AT(0x4e8cf0)=640;WORD_AT(0x4e8cf4)=480;
    for(unsigned paused=0;paused<2;paused++) {
        WORD_AT(0x79094c)=paused?1:101;audio_page=0;
        audio_bounds[0]=20;audio_bounds[1]=30;audio_bounds[2]=320;audio_bounds[3]=54;
        audio_bounds_valid=1;audio_left=1;audio_pending=-1;
        WORD_AT(0x300038)=0;WORD_AT(0x3001b0)=1;
        tab_pointer=(PcMenuPointer){397.5f,90,1920,1080,1,0,0,0};
        CHECK(!menu_video_tabs_poll(audio_menu));tab_pointer.left=1;
        CHECK(menu_video_tabs_poll(audio_menu) && audio_pending==0);
        menu_video_tabs_flush();CHECK(audio_page==0 && audio_pending==0);
        tab_pointer.left=0;menu_video_tabs_poll(audio_menu);
        tab_pointer.x=847.5f;tab_pointer.left=1;
        CHECK(menu_video_tabs_poll(audio_menu) && audio_pending==3);
        WORD_AT(0x3001b0)=3;menu_video_tabs_flush();CHECK(audio_page==3 && audio_pending==-1 && audio_bounds_valid);
        tab_pointer.left=0;menu_video_tabs_poll(audio_menu);tab_pointer.left=1;
        CHECK(menu_video_tabs_poll(audio_menu));menu_video_tabs_flush();CHECK(audio_page==3);
        tab_pointer.left=0;menu_video_tabs_poll(audio_menu);tab_pointer.x=397.5f;tab_pointer.left=1;
        CHECK(menu_video_tabs_poll(audio_menu));clock_ms+=1001;menu_video_tabs_flush();
        CHECK(audio_page==3 && audio_pending==-1);
    }
    /* Online selection must return before launching, allowing a notice frame.
     * The next input tick after two draws launches once; cancel restores idle. */
    _putenv_s("TSFP_ONLINE_TEST_COMMAND","legacy-fixture");
    g_esp=0x700000;online_activate();
    CHECK(online_opening_pending && online_connect_calls==0 && g_esp==0x700004);
    online_opening_tick();CHECK(online_connect_calls==0);
    online_opening_frames=2;online_opening_tick();
    CHECK(online_connect_calls==1 && !online_opening_pending && g_esp==0x700004);
    online_opening_tick();CHECK(online_connect_calls==1);
    _putenv_s("TSFP_ONLINE_TEST_COMMAND","");
    menu_mouse_dispatch=0;tab_pointer_enabled=slider_capture_enabled=0;
    WORD_AT(0x300004)=before_audio_descriptor;audio_menu=0;audio_row_count=0;
    /* The actual native waiting panel adopts an unconfigured pad's input port,
     * advances only the first waiting player, and preserves P1's controller. */
    WORD_AT(0x5236c4)=3;WORD_AT(0x74f9ec)=3;
    WORD_AT(0x74f9f0)=WORD_AT(0x74f9f4)=1;WORD_AT(0x74e914)=0;
    WORD_AT(0x320038)=2;WORD_AT(0x3201b8)=bits(1.f);WORD_AT(0x475cd4)=bits(.5f);
    join_port=3;g_esp=0x700000;WORD_AT(g_esp+4)=0x320000;
    sd_profile_tick();CHECK(join_port==3 && WORD_AT(0x74f9f4)==1);
    WORD_AT(0x320038)=1;g_esp=0x700000;sd_profile_tick();
    CHECK(join_used==1 && WORD_AT(0x74f9f0)==2 && WORD_AT(0x74e918)==3);
    CHECK(WORD_AT(0x6b93c4)==3 && WORD_AT(0x74e914)==0 && g_esp==0x700004);
    WORD_AT(0x320038)=2;join_port=1;g_esp=0x700000;sd_profile_tick();
    CHECK(join_used==9 && WORD_AT(0x74f9f4)==2 && WORD_AT(0x74e91c)==1);
    WORD_AT(0x5236c4)=1;join_port=2;g_esp=0x700000;sd_profile_tick();CHECK(join_port==2);
    join_port=-1;
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
    CHECK(controls_deadzone_rows[0] && controls_deadzone_rows[1]);
    CHECK(WORD_AT(controls_deadzone_rows[1]+0xc8)==24);
    WORD_AT(controls_deadzone_rows[1]+0xc8)=5;controls_deadzone_poll();
    CHECK(test_deadzones[kb_controller_port][1]==1638);
    CHECK(test_deadzones[kb_controller_port][0]==7849);
    CHECK(kb_controller_preset_row && WORD_AT(kb_controller_preset_row+0xc8)==0);
    WORD_AT(kb_controller_preset_row+0xc8)=1;kb_preset_poll();
    CHECK(kb_controller_choice==1 && pc_settings_controller_preset(kb_controller_port)==0);
    WORD_AT(kb_controller_preset_row+0xc8)=0;kb_preset_poll();CHECK(kb_controller_choice==0);
    /* Header navigation and keyboard IDs do not save or inherit native values. */
    call(0x2eab30,CONTROLS_TABS_ID,0x80000);
    CHECK(controls_tab==2 && kb_page==KB_HOME && controls_test_saves==0);
    CHECK(built_ids[0]==CONTROLS_TABS_ID && kb_widget_id(0)==200);
    CHECK(kb_local_id(200)==0 && kb_local_id(205)==5);
    CHECK(built_ids[built_row_count-1]==213); /* Apply below the existing footer divider; no Back row. */
    /* Inline presets use native choices, stay staged, and do not resize Apply. */
    CHECK(kb_keyboard_preset_row && kb_keyboard_apply_row);
    CHECK(!strcmp((char *)(g_xbox_mem_offset+KB_TEXT(13)),"\vs100CONTROL PRESET\vs100"));
    CHECK(WORD_AT(kb_keyboard_preset_row+0xc8)==0 && (WORD_AT(kb_keyboard_apply_row+0x28)&1u));
    WORD_AT(kb_keyboard_preset_row+0xc8)=1;call(0x2eab30,212,0);
    CHECK(kb_keyboard_choice==1 && !kb_settings.preset && !controls_test_saves);
    CHECK(!(WORD_AT(kb_keyboard_apply_row+0x28)&1u));
    WORD_AT(kb_keyboard_preset_row+0xc8)=0;call(0x2eab30,212,0);
    CHECK(kb_keyboard_choice==0 && (WORD_AT(kb_keyboard_apply_row+0x28)&1u));
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
        WORD_AT(0x5236c4)=1;WORD_AT(0x6b93c0)=0;
        WORD_AT(controls_menu+4)=CONTROLS_DESCRIPTOR;
        controls_players_open(controls_menu);
        CHECK(kb_page==KB_PLAYERS && kb_devices[0]==-2 && kb_devices[1]==-1);
        CHECK(WORD_AT(kb_device_rows[0]+0xc8)==0 && WORD_AT(kb_device_rows[1]+0xc8)==0);
        WORD_AT(kb_device_rows[1]+0xc8)=1;
        call(KB_TICK,0,0);
        CHECK(kb_devices[1]==0 && test_player_saves==0);
        test_pad_connected=0;call(KB_SELECT,204,0);
        CHECK(test_player_saves==0 && test_player_devices[1]==-1);
        CHECK(strstr(kb_status,"connected controller"));
        test_pad_connected=15;
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
        /* Switching an existing player to an absent pad keeps both live ports. */
        kb_devices[1]=2;test_pad_connected=3;call(KB_SELECT,204,0);
        CHECK(test_player_saves==2 && test_player_devices[1]==0 && test_player_devices[2]==1);
        CHECK(strstr(kb_status,"connected controller"));
        test_pad_connected=7;call(KB_SELECT,204,0);
        CHECK(test_player_saves==3 && test_player_devices[1]==2);
        test_pad_connected=15;
        test_pad_buttons=0;call(KB_SELECT,205,0);call(KB_SELECT,203,0);
        clock_ms+=20001;call(KB_TICK,0,0);
        CHECK(kb_page==KB_PLAYERS && !capture_active && kb_devices[3]==-1);
        call(KB_SELECT,206,0);CHECK(controls_tab==0);
    }
    { /* Reported case: one pad joins P2 through old P3 input port. */
        WORD_AT(0x5236c4)=2;WORD_AT(0x6b93c0)=0;WORD_AT(0x6b93c4)=2;
        test_player_devices[0]=-2;test_player_devices[1]=1;
        test_player_devices[2]=0;test_player_devices[3]=-1;test_pad_connected=1;
        controls_players_open(controls_menu);
        CHECK(kb_devices[0]==-2 && kb_devices[1]==0 && kb_devices[2]==1);
        CHECK(kb_player_ports[1]==2 && kb_player_ports[2]==1);
        int saves=test_player_saves;
        kb_devices[1]=-1;call(KB_SELECT,204,0);
        CHECK(test_player_saves==saves+1 && test_player_devices[2]==0);
        CHECK(test_player_devices[1]==1 && kb_devices[1]==0);
        /* Assigning that sole pad elsewhere must not strand joined P2. */
        kb_devices[1]=-1;kb_devices[2]=0;call(KB_SELECT,204,0);
        CHECK(test_player_saves==saves+1 && test_player_devices[2]==0);
        CHECK(strstr(kb_status,"Player 2 needs a connected controller"));
        /* With a second pad connected, simultaneous swap stays in guest ports. */
        test_pad_connected=3;kb_devices[1]=1;kb_devices[2]=0;call(KB_SELECT,204,0);
        CHECK(test_player_saves==saves+2 && test_player_devices[2]==1 && test_player_devices[1]==0);
        controls_players_open(controls_menu);CHECK(kb_devices[1]==1 && kb_devices[2]==0);
    }
    { /* P2 shoulder navigation does not depend on P1's keyboard prompt mode. */
        uint32_t descriptor=WORD_AT(controls_menu+4);
        WORD_AT(0x79094c)=101;WORD_AT(0x5236c4)=2;WORD_AT(0x78ad34)=1;
        WORD_AT(controls_menu+4)=0x523910;WORD_AT(controls_menu+0x38)=0;
        WORD_AT(controls_menu+0xc4)=WORD_AT(controls_menu+0xc8)=0;
        settings_keyboard=1;settings_pad_allowed=1;settings_tabs_input_reset();
        CHECK(!settings_tabs_direction(controls_menu));
        settings_keys[0x27]=1;CHECK(settings_tabs_direction(controls_menu)==1);
        settings_keys[0x27]=0;CHECK(!settings_tabs_direction(controls_menu));
        WORD_AT(controls_menu+0xc4)=0x400;CHECK(settings_tabs_direction(controls_menu)==1);
        settings_keys[0x27]=0;settings_pad_allowed=0;WORD_AT(controls_menu+4)=descriptor;
    }
    /* Known reticle paths must obey hidden HUD even without widescreen. */
    developer_hud_visible=0;g_esp=0x30000;WORD_AT(g_esp)=0x0014CC10u;
    pc_display_aim_primitive(original_highlight,0);CHECK(g_esp==0x30004);
    g_esp=0x30000;pc_display_aim_panel(original_highlight,0x137010u);CHECK(g_esp==0x30004);
    developer_hud_visible=1;
    /* Real native match Options insertion: host only, regular left/right
       selector, stable callback/ABI, no stale menu or guest-role writes. */
    WORD_AT(0x78ad2c)=11;WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;
    built_row_count=0;g_esp=0x700000;WORD_AT(g_esp+4)=0x300000;
    pc_network_bots_menu(nested_match_options);
    CHECK(g_esp==0x700004&&WORD_AT(0x78ad2c)==11&&built_row_count==1&&built_ids[0]==121);
    CHECK(!WORD_AT(developer_match_row+0xc8));WORD_AT(developer_match_row+0xc8)=1;
    call(DEVELOPER_MATCH_CHANGE,121,0);CHECK(developer_match_tools==1);
    WORD_AT(0x76b140)=2;WORD_AT(developer_match_row+0xc8)=0;call(DEVELOPER_MATCH_CHANGE,121,0);CHECK(developer_match_tools==1);
    WORD_AT(0x76b140)=1;WORD_AT(developer_match_owner+4)^=4;call(DEVELOPER_MATCH_CHANGE,121,0);CHECK(developer_match_tools==1);
    WORD_AT(developer_match_owner+4)^=4;call(DEVELOPER_MATCH_CHANGE,121,0);CHECK(!developer_match_tools);
    WORD_AT(0x76b140)=2;built_row_count=0;g_esp=0x700000;WORD_AT(g_esp+4)=0x300000;pc_network_bots_menu(sub_002C78F0_gen);CHECK(!built_row_count);
    WORD_AT(0x76b108)=WORD_AT(0x76b140)=0;WORD_AT(0x78ad2c)=9;
    /* Real sidebar adapter: save on guest thread, confirm/revert previews,
       preserve existing settings on a failed batch, and use native subtitles. */
    sidebar_open=1;video_sample_at=video_modes_at=0;g_esp=0x700000;
    video_sidebar_tick();CHECK(sidebar_video_observed.available&&sidebar_video_observed.mode_count>1);
    PcVideoValues saved=sidebar_video_observed.current,edit=saved;
    edit.value[VIDEO_FOV]=73;edit.value[VIDEO_SUBTITLES]=!saved.value[VIDEO_SUBTITLES];
    sidebar_video_queued=(PcVideoRequest){1,VIDEO_APPLY,edit};video_sidebar_tick();
    CHECK(sidebar_video_observed.completed==1&&pc_display_fov()==73);
    CHECK(WORD_AT(0x400130)==(unsigned)edit.value[VIDEO_SUBTITLES]&&(WORD_AT(0x78ad24)&0x10000));
    saved=sidebar_video_observed.current;edit=saved;edit.value[VIDEO_FOV]=81;
    edit.value[VIDEO_AF]=saved.value[VIDEO_AF]==16?8:16;graphics_save_ok=0;
    sidebar_video_queued=(PcVideoRequest){2,VIDEO_APPLY,edit};video_sidebar_tick();graphics_save_ok=1;
    CHECK(pc_display_fov()==73&&strstr(sidebar_video_observed.message,"Could not save"));
    saved=sidebar_video_observed.current;edit=saved;edit.width=saved.width==1280?1920:1280;edit.height=edit.width==1280?720:1080;
    sidebar_video_queued=(PcVideoRequest){3,VIDEO_APPLY,edit};video_sidebar_tick();
    CHECK(sidebar_video_observed.preview&&pc_display_current().width==edit.width);
    clock_ms+=15001;video_sidebar_tick();CHECK(!sidebar_video_observed.preview&&pc_display_current().width==saved.width);
    sidebar_video_queued=(PcVideoRequest){4,VIDEO_APPLY,edit};video_sidebar_tick();CHECK(sidebar_video_observed.preview);
    sidebar_open=0;clock_ms+=201;video_sidebar_tick();CHECK(pc_display_current().width==saved.width);
    sidebar_open=1;sidebar_video_queued=(PcVideoRequest){5,VIDEO_APPLY,edit};video_sidebar_tick();
    sidebar_video_queued=(PcVideoRequest){6,VIDEO_KEEP,edit};video_sidebar_tick();CHECK(!sidebar_video_observed.preview&&pc_display_current().width==edit.width);
    edit=sidebar_video_observed.current;edit.value[VIDEO_FOV]=999;
    sidebar_video_queued=(PcVideoRequest){7,VIDEO_APPLY,edit};video_sidebar_tick();CHECK(pc_display_fov()==73);
    edit=sidebar_video_observed.current;edit.value[VIDEO_FOV]=65;
    edit.value[VIDEO_AA]=edit.value[VIDEO_AA]==2?0:2;edit.value[VIDEO_AF]=2;
    edit.value[VIDEO_AO]=1;edit.value[VIDEO_MSAA]=4;edit.value[VIDEO_SS]=1;
    edit.value[VIDEO_CACHE]=0;edit.value[VIDEO_TEXTURE]=1;
    edit.value[VIDEO_FEED]=!edit.value[VIDEO_FEED];edit.value[VIDEO_COUNTER]=!edit.value[VIDEO_COUNTER];
    edit.value[VIDEO_RATE]=120;edit.value[VIDEO_FULLSCREEN]=1;
    sidebar_video_queued=(PcVideoRequest){8,VIDEO_APPLY,edit};video_sidebar_tick();
    CHECK(pc_display_fov()==65&&graphics_options.anisotropy==2&&graphics_options.ambient_occlusion);
    CHECK(graphics_options.msaa==4&&graphics_options.supersampling==1&&!graphics_options.shader_cache);
    CHECK(pc_post_aa_get()==(unsigned)edit.value[VIDEO_AA]&&tsfp_texture_is_4k());
    CHECK(pc_feed_enabled()==edit.value[VIDEO_FEED]&&pc_counter_enabled()==edit.value[VIDEO_COUNTER]);
    CHECK(requested_rate==120&&sidebar_fullscreen&&sidebar_video_observed.restart);
    sidebar_video_queued=(PcVideoRequest){9,VIDEO_CLEAR_CACHE,edit};int old_clears=cache_clear_calls;video_sidebar_tick();CHECK(cache_clear_calls==old_clears+1);
    VirtualFree(mem,0,MEM_RELEASE);
    puts("PASS: unified display, disabled Apply, staged batch, FOV bounds/mouse, Back discard, failed-save rollback, resolution confirmation/timeout, texture synchronization and guest ABI");
    return 0;
}

STUB(sub_001A4350) STUB(sub_001A8CE0) STUB(sub_001A8EE0)

STUB(sub_001CC360)

void sub_000D2CE0(void){}
void sub_0015BCC0_gen(void){}

void sub_000A2740(void){}

int pc_developer_enabled(void) {return 0;}
void pc_developer_shortcut_name(char *out,size_t bytes){snprintf(out,bytes,"F1");}
int pc_developer_set_enabled(int enabled) {(void)enabled;assert(0);return 0;}
void pc_developer_lobby_notice(const char *names) {(void)names;}
unsigned pc_network_developer_lobby(const PcDeveloperPeer *roster,unsigned count,int enabled,char *names,size_t bytes)
{(void)roster;(void)count;(void)enabled;(void)bytes;names[0]=0;return 0;}

void sub_00353760(void) {g_eax=0;g_esp+=4;}

void sub_002C78F0_gen(void) {developer_match_button(0x002C8010u);g_esp+=4;}
int pc_developer_tools_allowed(void) {return 1;}
void pc_developer_match_policy(int online,int allowed) {(void)online;(void)allowed;}
int pc_network_tools_policy(const PcDeveloperPeer *roster,unsigned count,int allowed) {(void)roster;(void)count;return allowed;}

int pc_network_enabled(void){return 1;}
void pc_network_enable_lan(void){lan_enables++;}
void sub_000668F0(void){}
