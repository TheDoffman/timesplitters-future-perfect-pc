#include "pc_settings.h"
#include <sys/stat.h>
/* Explicit test entry point; the build runner supplies a disposable config. */
int tsfp_native_settings_test(void) {
    char error[512];PcSettings before,next,read;
    const char *path=getenv("RECOMP_INPUT_CONFIG");
    if(!path||!pc_settings_read(&before,error,sizeof error))return 1;
    int capture_ok=tsfp_native_capture_test();
    PcSettings edit=before,snapshot=before;
    int move_key=before.keys[0][0];
    capture_ok &= pc_settings_assign(&edit,1,0,move_key,0)==-1 && !memcmp(&edit,&snapshot,sizeof edit);
    capture_ok &= pc_settings_assign(&edit,1,0,move_key,1)==1 && edit.keys[1][0]==move_key;
    for(int a=0;a<PC_ACTION_COUNT;a++)for(int slot=0;slot<PC_BINDING_SLOTS;slot++)
        if(a!=1 || slot!=0)capture_ok &= edit.keys[a][slot]!=move_key;
    snapshot=edit;
    capture_ok &= !pc_settings_assign(&edit,1,0,0x71,1) && !memcmp(&edit,&snapshot,sizeof edit);
    capture_ok &= !pc_settings_assign(&edit,-1,0,'Z',1) && !memcmp(&edit,&snapshot,sizeof edit);
    capture_ok &= pc_settings_assign(&edit,1,0,0,1)==1 && edit.keys[1][0]==0;
    /* Cancelling the draft leaves the active controls and disk unchanged. */
    capture_ok &= pc_settings_read(&read,error,sizeof error) && !memcmp(&read,&before,sizeof read);
    next=before;
    capture_ok &= pc_settings_assign(&next,20,0,0xba,1)==1;
    capture_ok &= pc_settings_assign(&next,21,0,0x67,1)==1;
    next.keys[0][0]='Z';next.keys[19][0]='I';next.sensitivity=.25;next.invert_y=1;
    int ok=capture_ok && pc_settings_save(&next,error,sizeof error);
    ok &= pc_settings_read(&read,error,sizeof error) && read.keys[19][0]=='I' && fabs(read.sensitivity-.25)<.0001 && read.invert_y;
    char hint[80];pc_prompts_reset();
    ok &= pc_prompts_rewrite("\v2 Select",hint,sizeof hint) && !strcmp(hint,"[Z] Select");
    SDL_Event e={0};XBOX_GAMEPAD pad;
    e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,NULL);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_I;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.sThumbRY>30000;
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_SEMICOLON;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.sThumbRY < -30000;
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_KP_7;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.sThumbRX < -30000;
    e.type=SDL_EVENT_KEY_UP;tsfp_native_input_event(&e,NULL);
    ok &= read.keys[20][0]==0xba && read.keys[21][0]==0x67;
    next.sensitivity=NAN;ok &= !pc_settings_save(&next,error,sizeof error);
    next.sensitivity=.5;next.keys[19][0]=0x71;ok &= !pc_settings_save(&next,error,sizeof error);
    next.keys[19][0]='O';
    struct stat st;
    if(stat(path,&st)||chmod(path,0400))ok=0;
    else {ok &= !pc_settings_save(&next,error,sizeof error);if(chmod(path,st.st_mode&0777))ok=0;}
    ok &= pc_settings_read(&read,error,sizeof error) && read.keys[19][0]=='I' && fabs(read.sensitivity-.25)<.0001;
    ok &= pc_settings_renderer_save(1,error,sizeof error);
    ok &= pc_settings_renderer_read(0)==1;
    ok &= !pc_settings_renderer_save(2,error,sizeof error);
    ok &= pc_settings_renderer_save(0,error,sizeof error);
    ok &= pc_settings_renderer_read(1)==0;
    release_all();
    fprintf(stderr,"[native-settings-test] shared conflict move/cancel, Cocoa capture, saved punctuation/keypad input, preservation and read-only rollback: %s\n",ok?"PASS":"FAIL");
    return ok?0:1;
}
