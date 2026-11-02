#ifdef _WIN32
/* Real process handoff test; no game data, settings or saves. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static HWND window;
HWND xbox_D3D8Window(void){return window;}
#include "interface/pc_game_restart.inc"
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l)
{if(m==WM_CLOSE){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int main(int argc,char **argv)
{
    if(!pc_game_restart_bootstrap(argc,argv))return 2;
    if(argc!=2)return 3;
    char *phase=getenv("TSFP_RESTART_TEST_CHILD");
    if(phase) {
        FILE *f=fopen(argv[1],"rb");if(!f)return 4;
        int done=fgetc(f);fclose(f);if(done!='P')return 5;
        if(getenv("TSFP_RESTART_WAIT"))return 6;
        if(strcmp(getenv("RECOMP_SAVE_DIR"),"isolated path with spaces"))return 7;
        f=fopen(argv[1],"wb");if(!f)return 8;fputs("CHILD",f);fclose(f);return 0;
    }
    if(pc_game_restart())return 9; /* No window: fail without exiting. */
    WNDCLASSW klass={0};klass.lpfnWndProc=proc;klass.lpszClassName=L"RestartTest";
    klass.hInstance=GetModuleHandle(NULL);if(!RegisterClassW(&klass))return 10;
    window=CreateWindowW(klass.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,NULL,klass.hInstance,NULL);
    _putenv_s("TSFP_RESTART_TEST_CHILD","1");_putenv_s("RECOMP_SAVE_DIR","isolated path with spaces");
    if(!pc_game_restart() || !pc_game_restart())return 11;
    MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    Sleep(300); /* Child must still be blocked even after quit request. */
    FILE *f=fopen(argv[1],"rb");if(f){fclose(f);return 12;}
    f=fopen(argv[1],"wb");if(!f)return 13;fputs("PARENT FINISHED",f);fclose(f);return 0;
}

#else
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int closing;
#include <SDL3/SDL.h>
bool SDL_PushEvent(SDL_Event *event){closing=event->type==SDL_EVENT_QUIT;return closing;}
#include "interface/pc_game_restart.inc"
int main(int argc,char **argv)
{
    if(!pc_game_restart_bootstrap(argc,argv))return 2;
    if(argc!=2)return 3;
    if(getenv("TSFP_RESTART_TEST_CHILD")) {
        FILE *f=fopen(argv[1],"rb");if(!f)return 4;
        int done=fgetc(f);fclose(f);if(done!='P')return 5;
        if(getenv("TSFP_RESTART_WAIT"))return 6;
        if(strcmp(getenv("RECOMP_SAVE_DIR"),"isolated path with spaces"))return 7;
        f=fopen(argv[1],"wb");if(!f)return 8;fputs("CHILD",f);fclose(f);return 0;
    }
    setenv("TSFP_RESTART_TEST_CHILD","1",1);setenv("RECOMP_SAVE_DIR","isolated path with spaces",1);
    if(!pc_game_restart() || !pc_game_restart() || !closing)return 11;
    usleep(300000);
    FILE *f=fopen(argv[1],"rb");if(f){fclose(f);return 12;}
    f=fopen(argv[1],"wb");if(!f)return 13;fputs("PARENT FINISHED",f);fclose(f);return 0;
}
#endif
