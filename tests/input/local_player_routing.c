static unsigned g_players_auto_ports;
/* Headless sampler integration. OS focus is an explicit test input; no desktop
 * events or personal settings are touched. Parser and sampler are production. */
#define control_magnitude legacy_control_magnitude
#define main profile_fixture_main
#include "controller_profile.c"
#undef main
#undef control_magnitude
#include "pc_input.h"
#include "pc_settings.h"
#include "input/pc_control_presets.h"
static int g_pc_keyboard_preset,g_pc_controller_presets[4],g_pc_custom_semantics;
static int test_gameplay;
int pc_controls_in_menu(void) {return !test_gameplay;}
int pc_settings_gameplay_masks(unsigned port,unsigned masks[24],int *mounted) {(void)port;(void)mounted;if(test_gameplay)masks[9]=0x10;return test_gameplay;}
void pc_settings_modern(PcSettings *s) {memset(s,0,sizeof *s);}
static int test_focus=1, test_capture, test_blocked, mouse_samples;
static long g_pc_auto_pad;
#define PC_WEAPON_SLOT_COUNT 10
#define AcquireSRWLockShared(p) ((void)0)
#define ReleaseSRWLockShared(p) ((void)0)
#define AcquireSRWLockExclusive(p) ((void)0)
#define ReleaseSRWLockExclusive(p) ((void)0)
#ifndef _WIN32
#define InterlockedExchange(p,v) (*(p)=(v))
#endif
#define pc_weapon_keys_sample(...) ((void)0)
int pc_settings_capture_pump(void){return test_capture;}
static int pc_controls_accept_pad(void){return test_focus&&!test_blocked;}
static int tsfp_native_input_focused(void){return test_focus;}
static int pc_prompts_keyboard_active(void){return 1;}
static void pc_prompts_controller(const RecompPadState *p,int live){(void)p;(void)live;}
int pc_controls_weapon_slots_active(void){return 0;}
static int pc_controls_terminal_click(void){return 0;}
static int pc_controls_key_down(int key){(void)key;return 0;}
static int tsfp_native_key_down(int key){(void)key;return 0;}
static void tsfp_native_mouse_sample(XBOX_GAMEPAD *out){(void)out;mouse_samples++;}
void pc_controls_reload_sample(const unsigned char roles[256],int active,int *reload,int *fire,int *secondary)
{(void)roles;(void)active;(void)reload;(void)fire;(void)secondary;}
/* PRODUCTION_SAMPLER */
int main(void)
{
    XBOX_GAMEPAD one,two,neutral={0};
    const char *config="{\"controllers\":[{\"port\":1,\"device\":\"keyboard\",\"bindings\":{\"lstick_up\":\"key:W\",\"rtrigger\":\"key:LBUTTON\"}},{\"port\":2,\"device\":\"gamepad:0\"},{\"port\":3,\"device\":\"none\"},{\"port\":4,\"device\":\"none\"}]}";
    for(int i=0;i<4;i++)defaults_for(&g_ctl[i],i);
    assert(parse_config(config));g_ready=1;connected=1;selected_slot=0;
    state.lx=32767;state.right_trigger=255;
    /* Low-range aim tuning leaves movement and other local players intact.
     * Exercise actual sampler routing on both Windows and native SDL builds. */
    state.lx=state.rx=3277;state.right_trigger=0;
    test_sample(1,&two);assert(two.sThumbLX==0 && two.sThumbRX==0);
    g_ctl[1].aim_deadzone=1638;
    test_sample(1,&two);assert(two.sThumbLX==0 && two.sThumbRX>4500 && two.sThumbRX<5200);
    g_ctl[1].move_deadzone=0;test_sample(1,&two);assert(two.sThumbLX>3000);
    state.rx=-3277;test_sample(1,&two);assert(two.sThumbRX<-4500);
    state.rx=1638;test_sample(1,&two);assert(two.sThumbRX==0);
    g_ctl[1].aim_deadzone=32767;state.rx=32767;test_sample(1,&two);assert(two.sThumbRX==0);
    assert(g_ctl[0].aim_deadzone==-1 && g_ctl[0].move_deadzone==-1);
    g_ctl[1].aim_deadzone=g_ctl[1].move_deadzone=-1;
    memset(&state,0,sizeof state);state.lx=32767;state.right_trigger=255;
    /* Detection exposes spare ports without stealing P1's automatic pad. */
    g_players_auto_ports=2u;
    assert(test_sample(0,&one));assert(one.sThumbLX==32767);
    /* Joining claims that pad exclusively for its new player. */
    g_players_auto_ports=0;
    assert(test_sample(0,&one));assert(!memcmp(&one,&neutral,sizeof one));
    assert(test_sample(1,&two));assert(two.sThumbLX==32767 && two.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255);
#ifdef _WIN32
    held_key='W';memset(g_key_state,0,sizeof g_key_state);
    test_sample(0,&one);test_sample(1,&two);
    assert(one.sThumbLY==32767 && !one.sThumbLX && !one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    assert(two.sThumbLX==32767 && !two.sThumbLY);
    held_key=VK_LBUTTON;memset(g_key_state,0,sizeof g_key_state);
    state.right_trigger=0;test_sample(0,&one);test_sample(1,&two);
    assert(one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255 && !two.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    held_key=0;memset(g_key_state,0,sizeof g_key_state);state.right_trigger=255;
#endif
    /* Disconnecting P2 must not move its held state onto P1. */
    connected=0;test_sample(0,&one);test_sample(1,&two);
    assert(!memcmp(&one,&neutral,sizeof one) && !memcmp(&two,&neutral,sizeof two));
    connected=1;test_sample(1,&two);assert(two.sThumbLX==32767);
    /* An unassigned controller can still share P1, but never the assigned pad. */
    multi=1;second_state.ly=-32767;second_state.buttons=RECOMP_PAD_A;
    test_sample(0,&one);test_sample(1,&two);
    assert(!one.sThumbLX && one.sThumbLY==-32767 && one.bAnalogButtons[XBOX_BUTTON_A]==255);
    assert(two.sThumbLX==32767 && !two.sThumbLY && !two.bAnalogButtons[XBOX_BUTTON_A]);
    /* Freecam/overlay block both automatic P1 and explicitly assigned pads,
     * without losing the held device state when ordinary control resumes. */
    test_blocked=1;test_sample(0,&one);test_sample(1,&two);
    assert(!memcmp(&one,&neutral,sizeof one) && !memcmp(&two,&neutral,sizeof two));
    test_blocked=0;test_sample(0,&one);test_sample(1,&two);
    assert(one.sThumbLY==-32767 && two.sThumbLX==32767);
    test_focus=0;test_sample(0,&one);test_sample(1,&two);
    assert(!memcmp(&one,&neutral,sizeof one) && !memcmp(&two,&neutral,sizeof two));
    test_focus=1;test_capture=1;test_sample(0,&one);
    assert(!memcmp(&one,&neutral,sizeof one));test_capture=0;
    assert(pc_preset_controller_token('1',pc_preset_masks)=='M' && pc_preset_controller_token('G',pc_preset_masks)=='6');
    assert(pc_preset_controller_token('L',pc_preset_masks)=='1' && pc_preset_controller_token('M',pc_preset_masks)=='G');
    assert(pc_preset_controller_token('N',pc_preset_masks)=='5' && pc_preset_controller_token('2',pc_preset_masks)=='2');
    {unsigned masks[24];memcpy(masks,pc_preset_masks,sizeof masks);masks[8]=0x2000;masks[23]=0x400;
     assert(pc_preset_controller_token('G',masks)=='G' && pc_preset_controller_token('M',masks)=='6');}
    /* Modern inputs route semantic roles through each local native layout.
     * This native profile fires with X, while its saved pad bindings unbind RT. */
    test_capture=0;test_gameplay=1;multi=0;connected=1;selected_slot=0;
    memset(&state,0,sizeof state);memset(&g_ctl[1].bind[9],0,sizeof(Binding));
    Controller custom=g_ctl[1];g_pc_controller_presets[1]=1;
    state.right_trigger=255;test_sample(1,&two);
    assert(two.bAnalogButtons[XBOX_BUTTON_X]==255 && !two.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    state.right_trigger=0;state.left_trigger=255;test_sample(1,&two);
    assert((two.wButtons&XBOX_GAMEPAD_RIGHT_THUMB) && !two.bAnalogButtons[XBOX_BUTTON_LTRIGGER]);
    state.left_trigger=0;state.buttons=RECOMP_PAD_RIGHT_SHOULDER;test_sample(1,&two);
    assert(two.bAnalogButtons[XBOX_BUTTON_LTRIGGER]==255 && !two.bAnalogButtons[XBOX_BUTTON_WHITE]);
    state.buttons=RECOMP_PAD_RIGHT_THUMB;test_sample(1,&two);assert(two.bAnalogButtons[XBOX_BUTTON_B]==255);
    state.buttons=RECOMP_PAD_B;test_sample(1,&two);assert(two.wButtons&XBOX_GAMEPAD_LEFT_THUMB);
    state.buttons=RECOMP_PAD_Y;test_sample(1,&two);assert(two.wButtons&XBOX_GAMEPAD_DPAD_RIGHT);
    state.buttons=RECOMP_PAD_LEFT_SHOULDER;test_sample(1,&two);assert(two.wButtons&XBOX_GAMEPAD_DPAD_UP);
    /* Menu LB and B keep their original meanings with the preset selected. */
    test_gameplay=0;test_sample(1,&two);assert(two.bAnalogButtons[XBOX_BUTTON_BLACK]==255);
    state.buttons=RECOMP_PAD_B;test_sample(1,&two);assert(two.bAnalogButtons[XBOX_BUTTON_B]==255);
    assert(!memcmp(&g_ctl[1],&custom,sizeof custom));g_pc_controller_presets[1]=0;
    state.buttons=0;state.right_trigger=255;test_sample(1,&two);assert(!two.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    /* Modern PC must retain custom pad sources in a mixed keyboard/pad profile. */
    Controller keyboard_custom=g_ctl[0];g_pc_keyboard_preset=1;test_gameplay=1;multi=1;
    second_state.buttons=RECOMP_PAD_B;second_state.ly=0;
    g_ctl[0].bind[2].count=1;assert(parse_source("pad:b",&g_ctl[0].bind[2].src[0]));
    test_sample(0,&one);assert(one.bAnalogButtons[XBOX_BUTTON_X]==255 && one.bAnalogButtons[XBOX_BUTTON_B]==255);
    g_ctl[0]=keyboard_custom;g_pc_keyboard_preset=0;
    /* Editable semantic Custom uses assigned keys, with Modern disabled. */
    multi=0;connected=0;test_gameplay=1;g_pc_custom_semantics=1;
    memset(&g_ctl[0].bind[9],0,sizeof(Binding));g_ctl[0].bind[9].count=1;
    assert(parse_source("key:H",&g_ctl[0].bind[9].src[0]));
#ifdef _WIN32
    held_key='H';test_sample(0,&one);
    assert(one.bAnalogButtons[XBOX_BUTTON_X]==255 && !one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    g_pc_custom_semantics=0;test_sample(0,&one);
    assert(one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255 && !one.bAnalogButtons[XBOX_BUTTON_X]);
    held_key=0;
#endif
    puts("PASS: assigned P2 pad cannot steer P1; free-pad fallback, disconnect and focus gating");
    return 0;
}
