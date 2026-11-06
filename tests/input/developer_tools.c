#include "pc_controls.h"
#include "pc_settings.h"
#include "texture_mode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ULONGLONG test_time;
#define GetTickCount64() test_time
#include "pc_developer.c"
static unsigned char test_keys[256];
static int dev_enabled,dev_key=0x70;
static int focus=1,menu_captured,fullscreen,texture,saves;
static PcMenuPointer pointer;
static PcDebugOptions debug;
static float test_ui_scale=1.f;
static PcSettings persisted;static int persisted_valid;
const char *const pc_action_labels[PC_ACTION_COUNT]={
    "Fire","Action","3","4","5","6","7","8","9","10","11","12",
    "13","14","15","16","17","18","19","20","21","22","23","24",
    "25","26","27","28","29","30","31","32","33","34","Developer Tools","Toggle no-clip","Toggle free-cam"};
int pc_controls_developer_keys(unsigned char out[256]) {memcpy(out,test_keys,256);return focus;}
void pc_controls_developer_release(void) {}
void pc_settings_capture_end(void) {}
int pc_settings_capture_blocked(void) {return 0;}
int pc_settings_developer_enabled(void) {return dev_enabled;}
int pc_settings_developer_save(int enabled,char *e,size_t n) {(void)e;(void)n;dev_enabled=enabled;return 1;}
void pc_settings_developer_keys(int keys[PC_BINDING_SLOTS]) {keys[0]=dev_key;keys[1]=0;}
void pc_settings_action_keys(int action,int keys[PC_BINDING_SLOTS]) {keys[0]=action==PC_NOCLIP_ACTION?'B':action==PC_FREECAM_ACTION?'N':dev_key;keys[1]=0;}

void pc_controls_text_mode(uint32_t owner) {(void)owner;}
void pc_controls_menu_mode(int active) {menu_captured=active;}
PcMenuPointer pc_controls_developer_pointer(void) {return pointer;}
void pc_controls_developer_fullscreen(void) {fullscreen++;}
float pc_controls_developer_ui_scale(void) {return test_ui_scale;}
int pc_controls_developer_is_fullscreen(void) {return fullscreen&1;}
void pc_controls_developer_debug_action(int open_folder) {(void)open_folder;}
void pc_controls_set_mouse(double s,int e,int i) {(void)s;(void)e;(void)i;}
void pc_settings_defaults(PcSettings *s) {memset(s,0,sizeof *s);s->enabled=1;s->sensitivity=.1;s->keys[0][0]=0x20;s->keys[14][0]='I';s->keys[15][0]='K';s->keys[16][0]='J';s->keys[17][0]='L';s->keys[18][0]='C';}
int pc_settings_read(PcSettings *s,char *error,size_t n) {if(persisted_valid)*s=persisted;else pc_settings_defaults(s);(void)error;(void)n;return 1;}
int pc_settings_save(const PcSettings *s,char *error,size_t n) {persisted=*s;persisted_valid=1;(void)error;(void)n;saves++;return 1;}
void pc_settings_key_name(int k,char *out,size_t n) {snprintf(out,n,"Key %d",k);}
int pc_settings_assign(PcSettings *s,int action,int binding,int key,int move) {
    if(key==0x70||key==0x71)return 0;
    if(key=='X'&&!move)return -1;
    s->keys[action][binding]=key;return 1;
}
int tsfp_texture_is_4k(void) {return texture;}
int tsfp_texture_toggle(void) {texture=!texture;return texture;}
void pc_debug_get(PcDebugOptions *out) {*out=debug;}
void pc_debug_set(const PcDebugOptions *v) {debug=*v;}
int pc_debug_config_save(const PcDebugOptions *v,char *error,size_t n) {(void)v;(void)error;(void)n;return 1;}
void pc_debug_clear(void) {}
void pc_debug_snapshot(PcDebugSnapshot *snapshot) {memset(snapshot,0,sizeof *snapshot);}
static float observed_audio_speed;
static void audio_speed(float value) {observed_audio_speed=value;}
static void frame(void) {test_time+=16;pc_developer_frame();}
static void key(int k) {test_keys[k]=1;frame();test_keys[k]=0;frame();}
static PcDeveloperLine painted[PC_DEVELOPER_MAX_DRAW];
static unsigned paint(void) {return pc_developer_lines(1280,830,painted,PC_DEVELOPER_MAX_DRAW);}
static int drawn(const char *text) {unsigned n=paint();for(unsigned i=0;i<n;i++)if(strstr(painted[i].text,text))return 1;return 0;}
static void click(int kind,int a,int b) {
    paint();int found=-1;
    for(unsigned i=0;i<sidebar_count;i++)if(sidebar_widgets[i].kind==kind&&sidebar_widgets[i].a==a&&sidebar_widgets[i].b==b){found=(int)i;break;}
    assert(found>=0);DeveloperWidget v=sidebar_widgets[found];
    if(v.content&&(v.y<sidebar_top||v.y+v.h>sidebar_bottom)) {
        sidebar_scroll[page]+=v.y-sidebar_top;paint();v=sidebar_widgets[found];
    }
    pointer.valid=1;pointer.width=1280;pointer.height=830;
    pointer.x=sidebar_left+(v.x+v.w*.5f)*sidebar_scale;pointer.y=(v.y+v.h*.5f)*sidebar_scale;
    pointer.left=1;frame();pointer.left=0;frame();paint();
}
static void type(const char *s) {for(;*s;s++)key(*s=='.'?0xbe:*s>='a'&&*s<='z'?*s-32:*s);}
int main(void) {
    PcDeveloperState s;PcDeveloperLine lines[PC_DEVELOPER_MAX_DRAW];
    pc_developer_audio_register(audio_speed);frame();assert(observed_audio_speed==1.f);
    pc_developer_toggle();assert(!pc_developer_open());frame();
    dev_key=0x72;assert(pc_developer_set_enabled(1));
    assert(!pc_developer_open() && pc_developer_lines(840,620,lines,PC_DEVELOPER_MAX_DRAW)==0);
    char name[104];pc_developer_shortcut_name(name,sizeof name);assert(strstr(name,"Key 114"));
    key(0x70);assert(!pc_developer_open());key(0x72);assert(pc_developer_open());
    key(0x72);assert(!pc_developer_open());dev_key=0x70;
    pc_developer_lobby_notice("Alice, Bob");assert(pc_developer_lines(840,620,lines,PC_DEVELOPER_MAX_DRAW)==0);
    test_time+=1001;assert(pc_developer_lines(840,620,lines,PC_DEVELOPER_MAX_DRAW)==0);

    /* Configured host-only flight toggles obey focus, enablement and offline gates. */
    pc_developer_publish(1,1,0);key('B');pc_developer_get(&s);assert(s.noclip);
    key('N');pc_developer_get(&s);assert(s.freecam&&!s.noclip&&s.hide_hud);
    key('N');pc_developer_get(&s);assert(!s.freecam&&!s.hide_hud);
    focus=0;key('B');focus=1;pc_developer_get(&s);assert(!s.noclip);
    pc_developer_publish(1,1,1);key('B');pc_developer_get(&s);assert(!s.noclip);
    pc_developer_publish(0,0,0);
    float origin[3]={1,2,3},direction[3]={0,0,-1},position[3],forward[3];
    assert(pc_developer_flight_step(2,origin,direction,0,position,forward));
    assert(position[0]==1&&position[1]==2&&position[2]==3&&forward[2]<-.99f);
    test_keys['W']=1;
    pc_developer_flight_step(2,origin,direction,.05,position,forward);assert(position[2]==3); /* no hard-coded WASD */
    test_keys['W']=0;test_keys['I']=1;
    pc_developer_flight_step(2,origin,direction,.05,position,forward);assert(fabs(position[2]-2.6)<.0001);
    test_keys[0x10]=1;
    pc_developer_flight_step(2,origin,direction,.05,position,forward);assert(fabs(position[2]-1.)<.0001);
    /* Use the same yaw/pitch convention and sensitivity as the player. */
    state.freecam=1;PcInput look={0};pc_input_focus(&look,1);pc_input_capture(&look,1);
    float initial_yaw=flight.yaw;pc_input_motion(&look,30,20,test_time);
    assert(pc_developer_mouse(&look,test_time,.2f,0,1));
    assert(fabsf(flight.yaw-(initial_yaw-6))<.001f&&fabsf(flight.pitch+4)<.001f);
    pc_input_motion(&look,0,20,test_time);pc_developer_mouse(&look,test_time,.2f,1,1);
    assert(fabsf(flight.pitch)<.001f);state.freecam=0;
    memset(test_keys,0,sizeof test_keys);
    pc_developer_flight_step(0,origin,direction,0,position,forward);
    frame();assert(!pc_developer_open());pc_developer_toggle();frame();assert(menu_captured);
    assert(paint()>40 && drawn("Developer Tools") && drawn("Use with caution."));
    assert(painted[0].kind==PC_DEVELOPER_BOX && painted[0].width==452);
    pc_developer_get(&s);assert(!s.freecam); /* unavailable in frontend */
    pc_developer_publish(1,1,0);click(UI_ACTION,HOME,2);pc_developer_get(&s);assert(s.noclip);
    click(UI_ACTION,HOME,3);pc_developer_get(&s);assert(s.freecam&&!s.noclip&&s.hide_hud);
    click(UI_ACTION,HOME,3);assert(pc_developer_hud_visible());
    click(UI_GROUP,2,0);click(UI_ACTION,HOME,11);click(UI_ACTION,HOME,12);click(UI_ACTION,HOME,5);
    pc_developer_get(&s);assert(s.god&&s.infinite_ammo&&s.checkpoint==1);
    assert(!drawn("HUD & interface")&&!drawn("HUD & UI")&&!drawn("Version watermark"));
    click(UI_NUMBER,1,0);type("2");key(13);pc_developer_get(&s);assert(s.manual_speed==2&&!edit_number);
    click(UI_STEP,1,1);pc_developer_get(&s);assert(fabs(s.manual_speed-2.1)<.001);
    click(UI_PRESET,1,0); /* first preset is Reset */
    pc_developer_get(&s);assert(s.manual_speed==1);
    paint();
    for(unsigned i=0;i<sidebar_count;i++)if(sidebar_widgets[i].kind==UI_SLIDER) {
        DeveloperWidget v=sidebar_widgets[i];sidebar_scroll[page]+=v.y-sidebar_top;paint();v=sidebar_widgets[i];
        pointer.x=v.x+v.w*.5f;pointer.y=v.y+v.h*.5f;pointer.left=1;frame();
        pointer.x=v.x+v.w;frame();pointer.left=0;frame();
        pc_developer_get(&s);assert(s.manual_speed==4);break;
    }
    click(UI_NUMBER,1,0);type("9");key(13);assert(edit_number==1&&strstr(status,"0.05"));key(27);assert(!edit_number&&pc_developer_open());
    click(UI_NUMBER,1,0);type("2");key(13);
    click(UI_GROUP,10,0);click(UI_NUMBER,3,0);type("0.25");key(13);
    click(UI_NUMBER,4,0);type("3");key(13);click(UI_ACTION,HOME,8);
    pc_developer_close();test_keys[0xdb]=1;frame();pc_developer_get(&s);assert(s.speed==.25);
    test_keys[0xdd]=1;frame();assert(observed_audio_speed==3.f);
    pc_developer_toggle();click(UI_ACTION,HOME,13);frame();assert(observed_audio_speed==1.f);
    click(UI_ACTION,HOME,13);assert(observed_audio_speed==3.f);
    pc_developer_close();focus=0;frame();assert(pc_developer_speed()==2.);focus=1;
    test_keys[0xdd]=test_keys[0xdb]=0;
    pc_developer_publish(1,1,1);frame();pc_developer_get(&s);assert(s.speed==1&&!s.god&&!s.infinite_ammo&&!s.freecam&&!s.noclip);
    pc_developer_toggle();assert(pc_developer_open());paint();
    assert(!pc_developer_tools_allowed()&&drawn("Tools are disabled by the host"));
    click(UI_TAB,SETTINGS,0);assert(page==SETTINGS);click(UI_TAB,DEBUG,0);assert(page==DEBUG);
    pc_developer_match_policy(1,1);click(UI_TAB,HOME,0);paint();
    assert(drawn("Time & audio")&&drawn("Unavailable in multiplayer")&&!drawn("Game speed"));
    pc_developer_publish(1,1,1);click(UI_ACTION,HOME,3);pc_developer_get(&s);assert(s.freecam);
    pc_developer_publish(1,1,1);pc_developer_get(&s);assert(s.freecam&&s.speed==1&&!s.god&&!s.infinite_ammo);
    pc_developer_match_policy(1,0);pc_developer_get(&s);assert(!s.freecam&&!s.hide_hud);
    pc_developer_close();pc_developer_publish(1,1,0);pc_developer_toggle();
    pc_developer_lines(1920,1080,painted,PC_DEVELOPER_MAX_DRAW);assert(painted[0].width==452);
    pc_developer_lines(3840,2160,painted,PC_DEVELOPER_MAX_DRAW);assert(painted[0].width==452);
    test_ui_scale=1.5f;pc_developer_lines(3840,2160,painted,PC_DEVELOPER_MAX_DRAW);assert(painted[0].width==678);test_ui_scale=1;paint();
    click(UI_DOCK,0,0);assert(sidebar_right&&painted[0].x==828);click(UI_DOCK,0,0);
    click(UI_SEARCH,0,0);type("mouse");assert(drawn("Mouse sensitivity")&&!drawn("Game speed"));key(27);assert(!sidebar_search[0]);
    test_keys[0x11]=1;key('F');test_keys[0x11]=0;type("nomatchxyz");assert(drawn("No matching tools"));key(27);
    click(UI_TAB,SETTINGS,0);assert(page==SETTINGS);click(UI_GROUP,11,0);
    click(UI_NUMBER,2,0);type("0.35");key(13);assert(fabs(persisted.sensitivity-.35)<.0001);
    click(UI_SETTING,0,0);assert(persisted.invert_y);
    PcVideoSnapshot video={0};video.available=video.effects=1;video.current.width=1920;video.current.height=1080;
    video.current.value[VIDEO_FOV]=55;video.current.value[VIDEO_MSAA]=video.current.value[VIDEO_AF]=1;
    video.modes[0][0]=1920;video.modes[0][1]=1080;video.modes[1][0]=2560;video.modes[1][1]=1440;video.mode_count=2;
    pc_developer_video_publish(&video);paint();
    assert(drawn("Input Settings")&&drawn("Video Settings")&&!drawn("Debug settings & diagnostics"));
    /* Input and Video can collapse independently. Edits stay drafts until Apply. */
    click(UI_GROUP,11,0);assert(!drawn("Mouse sensitivity"));click(UI_GROUP,11,0);
    click(UI_VIDEO,VIDEO_FULLSCREEN,0);assert(sidebar_video_dirty()&&!fullscreen);
    click(UI_VIDEO,VIDEO_REVERT_BUTTON,0);assert(!sidebar_video_dirty());
    click(UI_SEARCH,0,0);type("resolution");
    click(UI_VIDEO,VIDEO_RESOLUTION,0);assert(sidebar_video_dropdown==VIDEO_RESOLUTION);
    assert(drawn("2560"));
    click(UI_VIDEO_OPTION,VIDEO_RESOLUTION,1);assert(sidebar_video_draft.width==2560);
    click(UI_VIDEO,VIDEO_APPLY_BUTTON,0);PcVideoRequest request;assert(pc_developer_video_take(&request));
    assert(request.action==VIDEO_APPLY&&request.desired.width==2560&&sidebar_video_wait);
    video.completed=request.serial;video.preview=1;video.seconds=15;pc_developer_video_publish(&video);paint();
    click(UI_VIDEO,VIDEO_REVERT_BUTTON,0);assert(pc_developer_video_take(&request)&&request.action==VIDEO_REVERT);
    video.completed=request.serial;video.preview=0;pc_developer_video_publish(&video);paint();assert(!sidebar_video_dirty());
    key(27);click(UI_GROUP,15,0);
    pc_developer_match_policy(1,0);
    click(UI_ACTION,SETTINGS,4);click(UI_ACTION,SETTINGS,5);
    pc_developer_match_policy(1,0);pc_developer_publish(1,0,1);
    assert(!pc_developer_hud_visible()&&!pc_developer_watermark_visible()&&page==SETTINGS);
    assert(!sidebar_video_dirty()); /* Session visibility applies immediately. */
    click(UI_ACTION,SETTINGS,4);click(UI_ACTION,SETTINGS,5);
    assert(pc_developer_hud_visible()&&pc_developer_watermark_visible());
    pc_developer_match_policy(0,0);
    click(UI_STEP,5,1);assert(sidebar_video_draft.value[VIDEO_FOV]==56);
    click(UI_NUMBER,5,0);type("85");key(13);assert(sidebar_video_draft.value[VIDEO_FOV]==85);
    click(UI_VIDEO,VIDEO_REVERT_BUTTON,0);assert(sidebar_video_draft.value[VIDEO_FOV]==55);
    click(UI_GROUP,12,0);assert(!drawn("Resolution"));click(UI_GROUP,12,0);
    click(UI_ACTION,SETTINGS,0);assert(page==KEYBOARD);
    unsigned tall=pc_developer_lines(2160,3840,painted,PC_DEVELOPER_MAX_DRAW);
    assert(tall<PC_DEVELOPER_MAX_DRAW&&strstr(painted[tall-1].text,"Changes apply live"));paint();
    click(UI_BIND,0,0);key('X');assert(conflict_key=='X');key(13);assert(draft.keys[0][0]=='X'&&sidebar_dirty);
    click(UI_BIND,0,1);key('Y');assert(draft.keys[0][1]=='Y');
    click(UI_BIND,0,1);key(0x71);assert(capture_action==0);key(27);assert(capture_action<0);
    int before=saves;click(UI_ACTION,KEYBOARD,PC_ACTION_COUNT+4);assert(saves==before+1&&!sidebar_dirty&&persisted.keys[0][0]=='X');
    click(UI_BIND,0,1);key('Z');key(27);assert(page==SETTINGS);click(UI_ACTION,SETTINGS,0);assert(draft.keys[0][1]=='Y');key(27);
    click(UI_TAB,DEBUG,0);click(UI_ACTION,DEBUG,0);assert(pc_debug_option_value(&debug,0));
    click(UI_ACTION,DEBUG,PC_DEBUG_OPTION_COUNT+1);assert(page==LOGS);key(27);assert(page==DEBUG);
    /* Narrow and ultrawide layouts keep panel/footer in bounds and repaint on dock. */
    unsigned n=pc_developer_lines(640,480,painted,PC_DEVELOPER_MAX_DRAW);assert(n>20&&n<PC_DEVELOPER_MAX_DRAW);
    for(unsigned i=0;i<n;i++)assert(painted[i].x>=0&&painted[i].y>=0&&painted[i].x+painted[i].width<=640.1f);
    key(27);assert(!pc_developer_open());
    puts("Developer sidebar pointer, search, numeric editing, bindings, focus and offline guards passed");return 0;
}
