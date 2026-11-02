/* Linux desktop boundary. Configuration, bindings and display policy stay in
 * the shared models. Native editing dialogs are an explicit bring-up gap. */
#include "settings_macos.h"
#include "input_sdl.h"
#include "pc_settings.h"
#include "pc_display.h"
#include "pc_debug.h"
#include "pc_storage.h"
#include "crash_posix.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static SDL_Window *game_window;
static atomic_int picker_state;
static char picker_path[4096];
static void selected(void *context,const char *const *files,int filter){
    (void)context;(void)filter;
    int accepted=files&&files[0]&&strlen(files[0])<sizeof picker_path;
    if(accepted)snprintf(picker_path,sizeof picker_path,"%s",files[0]);
    atomic_store(&picker_state,accepted?2:3);
}
int pc_storage_picker_start(void){
    int expected=0;if(!atomic_compare_exchange_strong(&picker_state,&expected,1))return 0;
    SDL_ShowOpenFolderDialog(selected,NULL,game_window,NULL,false);return 1;
}
int pc_storage_picker_poll(char *out,size_t capacity){
    int state=atomic_load(&picker_state);if(state<2)return 0;
    if(state==2)snprintf(out,capacity,"%s",picker_path);
    atomic_store(&picker_state,0);return state==2?1:-1;
}
void tsfp_native_settings_attach(SDL_Window *window){game_window=window;pc_debug_init((HWND)window);}
void tsfp_native_settings_show(SDL_Window *window){
    tsfp_native_settings_release_input(window);tsfp_native_crash_inhibit(1);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"TimeSplitters Settings",
        "Display settings are available in the game's Video Options.\nKeyboard and mouse bindings use the controls JSON profile.\nThe Linux desktop settings editor is not yet available.",window);
    tsfp_native_crash_inhibit(0);
}
void pc_debug_show(HWND owner){
    PcDebugSnapshot snapshot;pc_debug_snapshot(&snapshot);
    char text[1024];snprintf(text,sizeof text,"Game: %.1f FPS\nF9 toggles the shared FPS overlay.\nThe Linux debug settings editor is not yet available.",snapshot.fps);
    tsfp_native_settings_release_input((SDL_Window *)owner);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Debug & Performance",text,(SDL_Window *)owner);
}
int pc_debug_open_logs(HWND owner){(void)owner;return 0;}
int tsfp_native_settings_test(void){fputs("SKIP: Linux native editing dialog acceptance is pending\n",stderr);return 77;}
int tsfp_native_debug_test(void){fputs("SKIP: Linux native debug dialog acceptance is pending\n",stderr);return 77;}
int tsfp_native_capture_test(void){return 77;}
int tsfp_native_graphics_settings_test(SDL_Window *window){
    (void)window;PcDisplayMode old=pc_display_current();
    int ok=pc_display_apply((PcDisplayMode){640,480});
    PcDisplayMode actual=pc_display_current();ok &= actual.width==640&&actual.height==480;
    ok &= pc_display_apply(old);
    fprintf(stderr,"[linux-settings-test] shared resolution apply/restore: %s (desktop dialog untested)\n",ok?"PASS":"FAIL");
    return ok;
}
