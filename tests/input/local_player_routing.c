static unsigned g_players_auto_ports;
/* Headless sampler integration. OS focus is an explicit test input; no desktop
 * events or personal settings are touched. Parser and sampler are production. */
#define control_magnitude legacy_control_magnitude
#define source_magnitude legacy_source_magnitude
#define main profile_fixture_main
#include "controller_profile.c"
#undef main
#undef control_magnitude
#undef source_magnitude
#ifndef _WIN32
static int held_key;
#endif
/* Synthetic OS keys; keep production source evaluation for physical pads. */
static int source_magnitude(const Source *source,KeyCache *cache,
                            const RecompPadState *pad,int have_pad,int deadzone)
{
    if(source->kind==SRC_KEY)return source->arg==held_key?255:0;
    return legacy_source_magnitude(source,cache,pad,have_pad,deadzone);
}
#define TEST_CLEAR_KEYS() ((void)0)
#include "pc_input.h"
#include "pc_settings.h"
#include "pc_prompts.h"
static int test_gameplay;
static unsigned test_active_ports;
unsigned pc_settings_active_ports(void) {return test_active_ports?test_active_ports:15u&~g_players_auto_ports;}
int pc_controls_in_menu(void) {return !test_gameplay;}
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
int pc_controls_weapon_slots_active(void){return 0;}
static int pc_controls_terminal_click(void){return 0;}
static int pc_controls_key_down(int key){(void)key;return 0;}
static int tsfp_native_key_down(int key){(void)key;return 0;}
static void tsfp_native_mouse_sample(XBOX_GAMEPAD *out){(void)out;mouse_samples++;}
void pc_controls_reload_sample(const unsigned char roles[256],int active,int *reload,int *fire,int *secondary)
{(void)roles;(void)active;(void)reload;(void)fire;(void)secondary;}
static unsigned controller_layout_depth;
/* PRODUCTION_PROMPTS */
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
    held_key='W';TEST_CLEAR_KEYS();
    test_sample(0,&one);test_sample(1,&two);
    assert(one.sThumbLY==32767 && !one.sThumbLX && !one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    assert(two.sThumbLX==32767 && !two.sThumbLY);
    held_key=VK_LBUTTON;TEST_CLEAR_KEYS();
    state.right_trigger=0;test_sample(0,&one);test_sample(1,&two);
    assert(one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255 && !two.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    held_key=0;TEST_CLEAR_KEYS();state.right_trigger=255;
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
    {
        /* A configured P2 controller is P1's shared pad until P2 actually joins.
         * Start with it powered off, then connect/reconnect without a restart. */
        Controller retained[4];memcpy(retained,g_ctl,sizeof retained);
        test_active_ports=1;test_gameplay=0;connected=0;multi=0;
        held_key=0;TEST_CLEAR_KEYS();
        memset(&state,0,sizeof state);pc_prompts_reset();
        test_sample(0,&one);assert(!memcmp(&one,&neutral,sizeof one) && pc_prompts_keyboard_active());
        connected=1;state.buttons=RECOMP_PAD_A;test_sample(0,&one);
        assert(one.bAnalogButtons[XBOX_BUTTON_A]==255 && !pc_prompts_keyboard_active());
        /* An unchanged held pad must not steal prompts back from a new key. */
        pc_prompts_keyboard();test_sample(0,&one);assert(pc_prompts_keyboard_active());
        state.buttons=RECOMP_PAD_B;test_sample(0,&one);assert(!pc_prompts_keyboard_active());
        test_gameplay=1;state.buttons=0;state.lx=32767;test_sample(0,&one);
        assert(one.sThumbLX==32767 && !pc_prompts_keyboard_active());
        state.lx=0;
        held_key='W';TEST_CLEAR_KEYS();pc_prompts_keyboard();test_sample(0,&one);
        assert(one.sThumbLY==32767 && pc_prompts_keyboard_active());
        held_key=0;TEST_CLEAR_KEYS();
        for(int cycle=0;cycle<4;cycle++) {
            connected=0;test_sample(0,&one);assert(!memcmp(&one,&neutral,sizeof one) && pc_prompts_keyboard_active());
            connected=1;state.buttons=RECOMP_PAD_A;test_sample(0,&one);
            assert(one.bAnalogButtons[XBOX_BUTTON_A]==255 && !pc_prompts_keyboard_active());
        }
        g_players_auto_ports=2; /* A joined port wins over stale detection metadata. */
        test_active_ports=3;test_sample(0,&one);assert(!memcmp(&one,&neutral,sizeof one));
        test_sample(1,&two);assert(two.bAnalogButtons[XBOX_BUTTON_A]==255);
        /* A joined controller can navigate a menu without P1's empty pad poll
         * forcing keyboard glyphs back on the very next sample. */
        test_gameplay=0;pc_prompts_keyboard();state.buttons=0;test_sample(1,&two);
        state.buttons=RECOMP_PAD_A;test_sample(1,&two);assert(!pc_prompts_keyboard_active());
        test_sample(0,&one);assert(!pc_prompts_keyboard_active());
        pc_prompts_keyboard();test_sample(1,&two);assert(pc_prompts_keyboard_active());
        state.buttons=RECOMP_PAD_B;test_sample(1,&two);assert(!pc_prompts_keyboard_active());
        connected=0;test_sample(1,&two);assert(pc_prompts_keyboard_active());
        connected=1;state.buttons=RECOMP_PAD_A;
        g_players_auto_ports=0;test_active_ports=1;test_sample(0,&one);assert(one.bAnalogButtons[XBOX_BUTTON_A]==255);
        assert(!memcmp(retained,g_ctl,sizeof retained));
        test_active_ports=0;test_gameplay=0;connected=0;memset(&state,0,sizeof state);
    }
    /* Explicit P1 controller prompts work on both host paths. */
    test_gameplay=0;test_focus=1;test_capture=test_blocked=0;
    connected=1;multi=0;selected_slot=2;memset(&state,0,sizeof state);state.buttons=RECOMP_PAD_A;
    defaults_for(&g_ctl[0],0);
    g_ctl[0].device=DEV_PAD;g_ctl[0].pad=2;pc_prompts_reset();
    test_sample(0,&one);
    assert(one.bAnalogButtons[XBOX_BUTTON_A]==255 && !pc_prompts_keyboard_active());
    test_sample(1,&two);assert(!pc_prompts_keyboard_active());
    puts("PASS: assigned P2 pad cannot steer P1; free-pad fallback, disconnect and focus gating");
    return 0;
}
