/* Headless sampler integration. OS focus is an explicit test input; no desktop
 * events or personal settings are touched. Parser and sampler are production. */
#define main profile_fixture_main
#include "controller_profile.c"
#undef main
#include "pc_input.h"
static int test_focus=1, test_capture, mouse_samples;
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
static int pc_settings_capture_pump(void){return test_capture;}
static int pc_controls_accept_pad(void){return test_focus;}
static int tsfp_native_input_focused(void){return test_focus;}
static int pc_prompts_keyboard_active(void){return 1;}
static void pc_prompts_controller(const RecompPadState *p,int live){(void)p;(void)live;}
static int pc_controls_weapon_slots_active(void){return 0;}
static int pc_controls_terminal_click(void){return 0;}
static int pc_controls_key_down(int key){(void)key;return 0;}
static int tsfp_native_key_down(int key){(void)key;return 0;}
static void tsfp_native_mouse_sample(XBOX_GAMEPAD *out){(void)out;mouse_samples++;}
static void pc_controls_reload_sample(const unsigned char *roles,int active,int *reload,int *fire,int *secondary)
{(void)roles;(void)active;(void)reload;(void)fire;(void)secondary;}
/* PRODUCTION_SAMPLER */
int main(void)
{
    XBOX_GAMEPAD one,two,neutral={0};
    const char *config="{\"controllers\":[{\"port\":1,\"device\":\"keyboard\",\"bindings\":{\"lstick_up\":\"key:W\",\"rtrigger\":\"key:LBUTTON\"}},{\"port\":2,\"device\":\"gamepad:0\"},{\"port\":3,\"device\":\"none\"},{\"port\":4,\"device\":\"none\"}]}";
    for(int i=0;i<4;i++)defaults_for(&g_ctl[i],i);
    assert(parse_config(config));g_ready=1;connected=1;selected_slot=0;
    state.lx=32767;state.right_trigger=255;
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
    test_focus=0;test_sample(0,&one);test_sample(1,&two);
    assert(!memcmp(&one,&neutral,sizeof one) && !memcmp(&two,&neutral,sizeof two));
    test_focus=1;test_capture=1;test_sample(0,&one);
    assert(!memcmp(&one,&neutral,sizeof one));test_capture=0;
    puts("PASS: assigned P2 pad cannot steer P1; free-pad fallback, disconnect and focus gating");
    return 0;
}
