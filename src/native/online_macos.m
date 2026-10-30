/* Native process/window adapter for the shared Online browser and bridge. */
#import <Cocoa/Cocoa.h>
#include <SDL3/SDL.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_online.h"
#include "pc_network.h"
#include "pc_network_build.h"
extern char **environ;
static pid_t companion;
static char ready_path[PATH_MAX],stop_path[PATH_MAX];
static int running(void) {
    if(!companion)return 0;
    int status;pid_t result=waitpid(companion,&status,WNOHANG);
    if(result==companion || (result<0 && errno==ECHILD)){
        if(result==companion && (!WIFEXITED(status) || WEXITSTATUS(status)))
            fprintf(stderr,"[ONLINE] Browser exited: status=%d signal=%d\n",WIFEXITED(status)?WEXITSTATUS(status):-1,WIFSIGNALED(status)?WTERMSIG(status):0);
        companion=0;return 0;
    }
    return 1;
}
void pc_online_disconnect(void) {
    if(running()) {
        FILE *file=fopen(stop_path,"w");if(file)fclose(file);
        for(unsigned i=0;i<160 && running();i++)usleep(50000);
        if(running()) {kill(companion,SIGTERM);while(waitpid(companion,NULL,0)<0 && errno==EINTR){}companion=0;}
    }
    if(*ready_path)unlink(ready_path);
    if(*stop_path)unlink(stop_path);
    pc_network_set_online(NULL);
}
static int fail(const char *message) {
    fprintf(stderr,"[ONLINE] %s\n",message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Online",message,SDL_GetKeyboardFocus());
    return 0;
}
int pc_online_connect(void) {
    @autoreleasepool {
        pc_online_disconnect();
        char executable[PATH_MAX],resolved[PATH_MAX],pid[32],python[PATH_MAX],script[PATH_MAX];
        uint32_t size=sizeof executable;
        if(_NSGetExecutablePath(executable,&size) || !realpath(executable,resolved))return fail("Cannot locate the game package.");
        NSString *root=[[NSString stringWithUTF8String:resolved] stringByDeletingLastPathComponent].stringByDeletingLastPathComponent;
        NSString *support=[root stringByAppendingPathComponent:@"Support/Online"];
        BOOL portable=[[NSFileManager defaultManager] fileExistsAtPath:[support stringByAppendingPathComponent:@"online.py"]];
        if(!portable) {
            root=@TSFP_ONLINE_SOURCE_ROOT;
            support=[root stringByAppendingPathComponent:@"tools/network"];
        }
        NSString *pythonPath=portable?[root stringByAppendingPathComponent:@"Support/Python/bin/python3"]:[root stringByAppendingPathComponent:@".venv/bin/python"];
        NSString *folder=[root stringByAppendingPathComponent:portable?@"Logs/Online":@"private/online"];
        NSString *manifest=portable?[support stringByAppendingPathComponent:@"network-identity.json"]:
            [NSString stringWithUTF8String:TSFP_ONLINE_MANIFEST];
        NSError *error=nil;
        if(![[NSFileManager defaultManager] createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:&error])
            return fail("Cannot create the Online session folder.");
        snprintf(python,sizeof python,"%s",pythonPath.fileSystemRepresentation);
        snprintf(script,sizeof script,"%s",[support stringByAppendingPathComponent:@"online.py"].fileSystemRepresentation);
        if(access(python,X_OK) || access(script,R_OK))return fail("Online support is missing. Extract the complete Mac game package or run the source setup.");
        snprintf(pid,sizeof pid,"%d",getpid());
        snprintf(ready_path,sizeof ready_path,"%s/%s.ready",folder.fileSystemRepresentation,pid);
        snprintf(stop_path,sizeof stop_path,"%s/%s.stop",folder.fileSystemRepresentation,pid);
        unlink(ready_path);unlink(stop_path);
        setenv("TSFP_ONLINE_HANDSHAKES",folder.fileSystemRepresentation,1);
        setenv("TSFP_ONLINE_MANIFEST",manifest.fileSystemRepresentation,1);
        char *arguments[]={python,"-B",script,"--pid",pid,"--result",ready_path,
                           "--compatibility",TSFP_NETWORK_COMPATIBILITY_HEX,NULL};
        int result=posix_spawn(&companion,python,NULL,NULL,arguments,environ);
        if(result){companion=0;return fail("Could not start the Online browser.");}
        SDL_Window *window=SDL_GetKeyboardFocus();
        bool relative=window?SDL_GetWindowRelativeMouseMode(window):false;
        bool grabbed=window?SDL_GetWindowMouseGrab(window):false;
        if(window){SDL_SetWindowRelativeMouseMode(window,false);SDL_SetWindowMouseGrab(window,false);}
        int accepted=0;
        while(running()) {
            FILE *file=fopen(ready_path,"r");
            if(file) {char line[128];accepted=fgets(line,sizeof line,file) && pc_network_set_online(line);fclose(file);if(accepted)break;}
            /* Keep Cocoa responsive while the guest menu awaits selection. */
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];
            SDL_PumpEvents();
        }
        if(window){SDL_RaiseWindow(window);SDL_SetWindowMouseGrab(window,grabbed);SDL_SetWindowRelativeMouseMode(window,relative);}
        return accepted;
    }
}

/* Asset-free supervisor acceptance, using the same connect/disconnect path. */
int pc_online_self_test(void) {
    if(!getenv("TSFP_ONLINE_TEST_COMMAND"))return 2;
    if(!SDL_Init(SDL_INIT_VIDEO))return 3;
    SDL_Window *window=SDL_CreateWindow("Multiplayer adapter test",400,200,0);
    if(!window){SDL_Quit();return 4;}
    int connected=pc_online_connect();
    int expected=getenv("TSFP_ONLINE_TEST_CANCEL")?0:1;
    int valid=connected==expected && (!connected || pc_network_enabled());
    fprintf(stderr,"[ONLINE-TEST] connected=%d expected=%d enabled=%d\n",connected,expected,pc_network_enabled());
    pc_online_disconnect();
    valid=valid && !running() && access(ready_path,F_OK)!=0 && access(stop_path,F_OK)!=0;
    SDL_DestroyWindow(window);SDL_Quit();
    if(valid)puts("PASS: native Online browser, handshake, return and companion cleanup");
    return valid?0:1;
}
