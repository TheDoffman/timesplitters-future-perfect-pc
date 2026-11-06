#ifndef TSFP_PC_DEVELOPER_H
#define TSFP_PC_DEVELOPER_H
#include <stddef.h>
#include "pc_input.h"
#include "pc_menu_mouse.h"
/* Host UI requests contain values only. Guest memory is touched exclusively by
 * the game-thread adapter; window/presentation threads never retain guest pointers. */
typedef struct PcDeveloperState {
    int hide_hud, hide_watermark, noclip, freecam;
    double speed;
    unsigned checkpoint;
    int speed_keys, god, infinite_ammo;
    double manual_speed, slow_speed, fast_speed;
    int audio_speed;
} PcDeveloperState;
typedef struct PcDeveloperLine {
    char text[160];
    float x,y,width,height;
} PcDeveloperLine;
int pc_developer_open(void);
int pc_developer_enabled(void);
int pc_developer_set_enabled(int enabled);
void pc_developer_shortcut_name(char *out,size_t bytes);
void pc_developer_audio_register(void (*set_speed)(float));
int pc_developer_shortcut_key(int key);
void pc_developer_lobby_notice(const char *names);
void pc_developer_lobby_guest(void);
double pc_developer_speed(void);
double pc_developer_audio_speed(void);
void pc_developer_movie(int active);
void pc_developer_toggle(void);
void pc_developer_close(void);
void pc_developer_frame(void);
unsigned pc_developer_lines(unsigned width,unsigned height,PcDeveloperLine *out,unsigned capacity);
void pc_developer_get(PcDeveloperState *out);
void pc_developer_publish(int gameplay,int story,int network);
void pc_developer_report(const char *message);
void pc_developer_guest_frame(void);
int pc_developer_hud_visible(void);
int pc_developer_watermark_visible(void);
int pc_developer_flying(void);
int pc_developer_blocks_player(void);
int pc_developer_mouse(PcInput *input,uint64_t now,float sensitivity,int invert,float fov_scale);
int pc_developer_flight_step(int mode,const float origin[3],const float direction[3],double seconds,
                             float position[3],float forward[3]);
/* Thin host adapters, called outside UI locks. */
void pc_controls_developer_fullscreen(void);
void pc_controls_developer_debug_action(int open_folder);
int pc_controls_developer_keys(unsigned char keys[256]);
PcMenuPointer pc_controls_developer_pointer(void);
void pc_controls_developer_release(void);
#endif
