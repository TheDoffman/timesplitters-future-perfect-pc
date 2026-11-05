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
static int focus=1,menu_captured,fullscreen,texture,saves;
static PcMenuPointer pointer;
static PcDebugOptions debug;
const char *const pc_action_labels[PC_ACTION_COUNT]={
    "Fire","Action","3","4","5","6","7","8","9","10","11","12",
    "13","14","15","16","17","18","19","20","21","22","23","24",
    "25","26","27","28","29","30","31","32","33","34"};
int pc_controls_developer_keys(unsigned char out[256]) {memcpy(out,test_keys,256);return focus;}
void pc_controls_developer_release(void) {}
void pc_settings_capture_end(void) {}
void pc_controls_text_mode(uint32_t owner) {(void)owner;}
void pc_controls_menu_mode(int active) {menu_captured=active;}
PcMenuPointer pc_controls_developer_pointer(void) {return pointer;}
void pc_controls_developer_fullscreen(void) {fullscreen++;}
void pc_controls_developer_debug_action(int open_folder) {(void)open_folder;}
void pc_controls_set_mouse(double s,int e,int i) {(void)s;(void)e;(void)i;}
void pc_settings_defaults(PcSettings *s) {memset(s,0,sizeof *s);s->enabled=1;s->sensitivity=.1;s->keys[0][0]=0x20;s->keys[14][0]='I';s->keys[15][0]='K';s->keys[16][0]='J';s->keys[17][0]='L';s->keys[18][0]='C';}
int pc_settings_read(PcSettings *s,char *error,size_t n) {pc_settings_defaults(s);(void)error;(void)n;return 1;}
int pc_settings_save(const PcSettings *s,char *error,size_t n) {(void)s;(void)error;(void)n;saves++;return 1;}
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
static void frame(void) {test_time+=16;pc_developer_frame();}
static void key(int k) {test_keys[k]=1;frame();test_keys[k]=0;frame();}
int main(void) {
    PcDeveloperState s;PcDeveloperLine lines[16];
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
    assert(pc_developer_lines(1920,1080,lines,16)>8);
    assert(strstr(lines[0].text,"Developer Tools"));
    /* UI keyboard events exercise real navigation and value state. */
    key(0x28);key(13);assert(!pc_developer_hud_visible());
    key(0x28);key(13);pc_developer_get(&s);assert(!s.noclip); /* frontend guard */
    pc_developer_publish(1,1,0);key(13);pc_developer_get(&s);assert(s.noclip&&!pc_developer_blocks_player());
    key(0x28);key(13);pc_developer_get(&s);assert(s.freecam&&!s.noclip&&s.hide_hud&&pc_developer_blocks_player());
    key(13);pc_developer_get(&s);assert(!s.freecam&&!s.hide_hud);
    key(0x28);key(13);assert(!pc_developer_watermark_visible());
    key(0x28);key(13);pc_developer_get(&s);assert(s.checkpoint==1);
    key(0x28);key(13);key('2');key(13);pc_developer_get(&s);assert(s.speed==2.);
    pc_developer_close();test_keys[0xdb]=1;frame();pc_developer_get(&s);assert(s.speed==2.); /* outside-menu disabled */
    pc_developer_toggle();selected=9;key(13);pc_developer_close();frame();pc_developer_get(&s);assert(s.speed==.5&&s.manual_speed==2.);
    test_keys[0xdd]=1;frame();pc_developer_get(&s);assert(s.speed==2.); /* ] wins ties */
    test_keys[0xdd]=test_keys[0xdb]=0;frame();pc_developer_get(&s);assert(s.speed==2.);
    pc_developer_toggle();selected=10;key(13);key('0');key(0xbe);key('2');key('5');key(13);pc_developer_get(&s);assert(s.slow_speed==.25);
    selected=11;key(13);key('3');key(13);pc_developer_get(&s);assert(s.fast_speed==3.);
    selected=12;key(13);selected=13;key(13);pc_developer_get(&s);assert(s.god&&s.infinite_ammo);
    pc_developer_close();test_keys[0xdd]=1;frame();pc_developer_get(&s);assert(s.speed==3.);
    focus=0;frame();pc_developer_get(&s);assert(s.speed==2.);focus=1;test_keys[0xdd]=0;
    pc_developer_publish(1,1,1);pc_developer_get(&s);assert(s.speed==1.&&!s.freecam&&!s.noclip&&!s.god&&!s.infinite_ammo);
    pc_developer_toggle();key(27); /* Home Escape closes */
    assert(!pc_developer_open());
    pc_developer_toggle();selected=0;key(13);assert(page==SETTINGS);
    key(13);assert(page==KEYBOARD);
    key(13);key('X');assert(conflict_key=='X');key(13);assert(draft.keys[0][0]=='X');
    key(0x27);key(13);key('Y');assert(draft.keys[0][1]=='Y');
    key(13);key(0x71);assert(capture_action==0);key(27);assert(capture_action<0);
    for(int i=0;i<PC_ACTION_COUNT+4;i++)key(0x28);
    key(13);assert(saves==1);
    assert(pc_developer_lines(640,480,lines,16)>0);assert(lines[0].x>=0&&lines[0].y>=0);
    key(27);assert(page==SETTINGS);key(0x28);key(13);assert(page==DEBUG);
    key(13);assert(pc_debug_option_value(&debug,0));
    key(27);key(0x28);key(0x28);key(13);assert(texture);
    key(0x28);key(13);assert(fullscreen==1);
    puts("Developer Tools navigation, capture, focus, guarded requests and settings passed");return 0;
}
