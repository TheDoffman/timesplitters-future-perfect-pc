/* Linux process/window adapter for the existing Online companion protocol. */
#define _GNU_SOURCE 1
#include <SDL3/SDL.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_online.h"
#include "pc_network.h"
#include "pc_network_build.h"
#include "online-python.h"
extern char **environ;
static pid_t companion;
static PcOnlineMode active_mode;
static char ready_path[4096],stop_path[4096];
static int running(void){
    if(!companion)return 0;
    int status;pid_t result=waitpid(companion,&status,WNOHANG);
    if(result==companion||(result<0&&errno==ECHILD)){companion=0;return 0;}return 1;
}
void pc_online_disconnect(void){
    active_mode=PC_ONLINE_NONE;
    if(running()){
        FILE *file=fopen(stop_path,"w");if(file)fclose(file);
        for(unsigned i=0;i<160&&running();i++)usleep(50000);
        if(running()){kill(companion,SIGTERM);while(waitpid(companion,NULL,0)<0&&errno==EINTR){}companion=0;}
    }
    if(*ready_path)unlink(ready_path);if(*stop_path)unlink(stop_path);pc_network_set_online(NULL);
}
PcOnlineMode pc_online_mode(void){return active_mode;}
int pc_online_connect(void){return pc_online_connect_mode(PC_ONLINE_ARCADE);}
int pc_online_connect_mode(PcOnlineMode mode){
    if(mode!=PC_ONLINE_ARCADE)return 0; /* Same native Story transport gap as Mac. */
    pc_online_disconnect();char folder[4096],pid[32],script[4096];
    snprintf(folder,sizeof folder,"%s/private/online",TSFP_ONLINE_SOURCE_ROOT);
    snprintf(script,sizeof script,"%s/tools/network/online.py",TSFP_ONLINE_SOURCE_ROOT);
    if((mkdir(folder,0700)&&errno!=EEXIST)||access(TSFP_ONLINE_PYTHON,X_OK)||access(script,R_OK))return 0;
    snprintf(pid,sizeof pid,"%d",getpid());
    if(snprintf(ready_path,sizeof ready_path,"%s/%s.ready",folder,pid)>=(int)sizeof ready_path||
       snprintf(stop_path,sizeof stop_path,"%s/%s.stop",folder,pid)>=(int)sizeof stop_path)return 0;
    unlink(ready_path);unlink(stop_path);
    setenv("TSFP_ONLINE_HANDSHAKES",folder,1);setenv("TSFP_ONLINE_MANIFEST",TSFP_ONLINE_MANIFEST,1);
    char *args[]={TSFP_ONLINE_PYTHON,"-B",script,"--pid",pid,"--result",ready_path,"--compatibility",TSFP_NETWORK_COMPATIBILITY_HEX,NULL};
    if(posix_spawn(&companion,args[0],NULL,NULL,args,environ)){companion=0;return 0;}
    SDL_Window *window=SDL_GetKeyboardFocus();
    bool relative=window?SDL_GetWindowRelativeMouseMode(window):false;
    bool grabbed=window?SDL_GetWindowMouseGrab(window):false;
    if(window){SDL_SetWindowRelativeMouseMode(window,false);SDL_SetWindowMouseGrab(window,false);}
    int accepted=0;
    while(running()){
        FILE *file=fopen(ready_path,"r");
        if(file){char line[128];accepted=fgets(line,sizeof line,file)&&pc_network_set_online(line);fclose(file);if(accepted)break;}
        if(SDL_IsMainThread())SDL_PumpEvents();SDL_Delay(50);
    }
    if(window){SDL_RaiseWindow(window);SDL_SetWindowMouseGrab(window,grabbed);SDL_SetWindowRelativeMouseMode(window,relative);}
    if(accepted)active_mode=PC_ONLINE_ARCADE;
    return accepted;
}
int pc_online_self_test(void){
    if(!getenv("TSFP_ONLINE_TEST_COMMAND"))return 77;
    if(!SDL_Init(SDL_INIT_VIDEO))return 1;
    SDL_Window *window=SDL_CreateWindow("Online adapter test",400,200,0);if(!window)return 1;
    int connected=pc_online_connect();int ok=connected==!getenv("TSFP_ONLINE_TEST_CANCEL");
    pc_online_disconnect();ok &= !running()&&access(ready_path,F_OK)!=0&&access(stop_path,F_OK)!=0;
    SDL_DestroyWindow(window);SDL_Quit();return ok?0:1;
}
