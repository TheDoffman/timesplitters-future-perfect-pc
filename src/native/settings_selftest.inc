#include "pc_settings.h"
#include <sys/stat.h>
/* Explicit test entry point; the build runner supplies a disposable config. */
int tsfp_native_settings_test(void) {
    char error[512];PcSettings before,next,read;
    const char *path=getenv("RECOMP_INPUT_CONFIG");
    if(!path||!pc_settings_read(&before,error,sizeof error))return 1;
    next=before;next.keys[0][0]='Z';next.keys[19][0]='I';next.sensitivity=.25;next.invert_y=1;
    int ok=pc_settings_save(&next,error,sizeof error);
    ok &= pc_settings_read(&read,error,sizeof error) && read.keys[19][0]=='I' && fabs(read.sensitivity-.25)<.0001 && read.invert_y;
    char hint[80];pc_prompts_reset();
    ok &= pc_prompts_rewrite("\v2 Select",hint,sizeof hint) && !strcmp(hint,"[Z] Select");
    SDL_Event e={0};XBOX_GAMEPAD pad;
    e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;tsfp_native_input_event(&e,NULL);
    e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=SDL_SCANCODE_I;tsfp_native_input_event(&e,NULL);
    recomp_bindings_sample(0,&pad);ok &= pad.sThumbRY>30000;
    next.sensitivity=NAN;ok &= !pc_settings_save(&next,error,sizeof error);
    next.sensitivity=.5;next.keys[19][0]=0x71;ok &= !pc_settings_save(&next,error,sizeof error);
    next.keys[19][0]='O';
    struct stat st;
    if(stat(path,&st)||chmod(path,0400))ok=0;
    else {ok &= !pc_settings_save(&next,error,sizeof error);if(chmod(path,st.st_mode&0777))ok=0;}
    ok &= pc_settings_read(&read,error,sizeof error) && read.keys[19][0]=='I' && fabs(read.sensitivity-.25)<.0001;
    release_all();
    fprintf(stderr,"[native-settings-test] shared save/read/live input, invalid settings rejection and read-only rollback: %s\n",ok?"PASS":"FAIL");
    return ok?0:1;
}
