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
#include "input/pc_control_presets.h"
static int g_pc_keyboard_preset,g_pc_controller_presets[4],g_pc_custom_semantics;
static int test_gameplay;
static unsigned test_axes[4]={2,1,4,3};
static unsigned test_active_ports;
unsigned pc_settings_active_ports(void) {return test_active_ports?test_active_ports:15u&~g_players_auto_ports;}
static unsigned test_action_masks[24];
static int test_remapped_actions;
int pc_controls_in_menu(void) {return !test_gameplay;}
int pc_settings_gameplay_masks(unsigned port,unsigned masks[24],int *mounted) {(void)port;(void)mounted;if(test_gameplay){if(test_remapped_actions)memcpy(masks,test_action_masks,sizeof test_action_masks);else masks[9]=0x10;pc_preset_axis_masks(masks,test_axes);}return test_gameplay;}
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
static unsigned native_button_mask(const XBOX_GAMEPAD *pad)
{
    unsigned mask=0;
    for(unsigned i=0;i<24;i++) {
        if(CONTROLS[i].kind==K_ANALOG && pad->bAnalogButtons[CONTROLS[i].arg]>=64)mask|=pc_preset_masks[i];
        if(CONTROLS[i].kind==K_DIGITAL && (pad->wButtons&CONTROLS[i].arg))mask|=pc_preset_masks[i];
    }
    return mask;
}
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
    held_key='H';test_sample(0,&one);
    assert(one.bAnalogButtons[XBOX_BUTTON_X]==255 && !one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    g_pc_custom_semantics=0;test_sample(0,&one);
    assert(one.bAnalogButtons[XBOX_BUTTON_X]==255 && !one.bAnalogButtons[XBOX_BUTTON_RTRIGGER]);
    /* The controller swaps Move/Look; semantic keyboard actions must be
     * sent to the physical axes now selected for movement, not fixed LY/LX.
     * Feeding the output through those native selectors still means Move. */
    for(int semantic=0;semantic<=1;semantic++) {
    g_pc_custom_semantics=semantic;
    memset(&g_ctl[0].bind[14],0,sizeof(Binding));g_ctl[0].bind[14].count=1;
    assert(parse_source("key:W",&g_ctl[0].bind[14].src[0]));
    memset(&g_ctl[0].bind[15],0,sizeof(Binding));g_ctl[0].bind[15].count=1;
    assert(parse_source("key:S",&g_ctl[0].bind[15].src[0]));
    test_axes[0]=4;test_axes[1]=3;test_axes[2]=2;test_axes[3]=1;
    held_key='W';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(one.sThumbRY>0 && !one.sThumbLY && !one.sThumbRX && !one.sThumbLX);
    held_key='S';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(one.sThumbRY<0 && !one.sThumbLY && !one.sThumbRX && !one.sThumbLX);
    /* Bind the optional look keys explicitly and exercise the reverse route. */
    g_ctl[0].bind[19].count=1;assert(parse_source("key:I",&g_ctl[0].bind[19].src[0]));
    g_ctl[0].bind[22].count=1;assert(parse_source("key:L",&g_ctl[0].bind[22].src[0]));
    held_key='I';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(one.sThumbLY>0 && !one.sThumbRY && !one.sThumbLX && !one.sThumbRX);
    held_key='L';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(one.sThumbLX>0 && !one.sThumbLY && !one.sThumbRY && !one.sThumbRX);
    /* An edited Forward binding uses the same independent semantic route. */
    memset(&g_ctl[0].bind[14],0,sizeof(Binding));g_ctl[0].bind[14].count=1;
    assert(parse_source("key:H",&g_ctl[0].bind[14].src[0]));
    held_key='H';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(one.sThumbRY>0 && !one.sThumbLY);
    held_key='W';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(!one.sThumbLX && !one.sThumbLY && !one.sThumbRX && !one.sThumbRY);
    /* Menus retain physical directions rather than gameplay axis remapping. */
    test_gameplay=0;held_key='H';TEST_CLEAR_KEYS();test_sample(0,&one);
    assert(one.sThumbLY>0 && !one.sThumbRY);test_gameplay=1;
    }
    /* Controller-only Custom keeps its physical sticks unchanged. */
    held_key=0;connected=1;state.ly=32767;state.right_trigger=0;
    g_ctl[1].bind[14].count=1;assert(parse_source("pad:ly+",&g_ctl[1].bind[14].src[0]));
    test_sample(1,&two);assert(two.sThumbLY>0 && !two.sThumbRY);
    connected=0;held_key=0;
    /* Every named keyboard button action survives every controller-button
     * target, without preset metadata or any change to saved bindings. */
    static const unsigned roles[]={0,1,2,8,9,10,11,12,13,18,23};
    static const unsigned keyboard_sources[]={0,1,2,3,4,5,8,9,10,11,12,13,18,23};
    test_remapped_actions=1;g_pc_custom_semantics=0;
    for(unsigned shift=0;shift<sizeof roles/sizeof roles[0];shift++) {
        memcpy(test_action_masks,pc_preset_masks,sizeof test_action_masks);
        for(unsigned role=0;role<sizeof roles/sizeof roles[0];role++)
            test_action_masks[roles[role]]=pc_preset_masks[roles[(role+shift)%(sizeof roles/sizeof roles[0])]];
        for(unsigned key=0;key<sizeof keyboard_sources/sizeof keyboard_sources[0];key++) {
            unsigned source=keyboard_sources[key],action=pc_preset_keyboard_action(source);
            memset(g_ctl[0].bind,0,sizeof g_ctl[0].bind);
            Binding *binding=&g_ctl[0].bind[source];binding->count=1;
            assert(parse_source("key:H",&binding->src[0]));
            Controller retained=g_ctl[0];
            held_key='H';TEST_CLEAR_KEYS();test_sample(0,&one);
            assert(native_button_mask(&one)==test_action_masks[action]);
            assert(!memcmp(&retained,&g_ctl[0],sizeof retained));
            /* Editing just the key keeps the action and controller unchanged. */
            assert(parse_source("key:J",&binding->src[0]));
            held_key='J';TEST_CLEAR_KEYS();test_sample(0,&one);
            assert(native_button_mask(&one)==test_action_masks[action]);
            held_key='H';TEST_CLEAR_KEYS();test_sample(0,&one);assert(!native_button_mask(&one));
        }
    }
    /* Same-player Custom pad input remains physical after keyboard edits. */
    g_ctl[1].device=DEV_NONE;connected=1;memset(&state,0,sizeof state);state.right_trigger=255;
    held_key=0;TEST_CLEAR_KEYS();test_sample(0,&one);XBOX_GAMEPAD retained_pad=one;
    g_ctl[0].bind[9].count=1;assert(parse_source("key:K",&g_ctl[0].bind[9].src[0]));
    test_sample(0,&two);assert(!memcmp(&retained_pad,&two,sizeof two));
    assert(two.bAnalogButtons[XBOX_BUTTON_RTRIGGER]==255);
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
