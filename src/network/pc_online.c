/* The browser/bridge is a supervised companion process. Guest game code still
   owns its System Link lobby and gameplay; only the transport changes. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_online.h"
#include "pc_network.h"
#include "pc_network_build.h"
static HANDLE companion;
static PcOnlineMode active_mode;
static char ready_path[MAX_PATH],stop_path[MAX_PATH];
extern HWND xbox_D3D8Window(void);

void pc_online_disconnect(void)
{
    active_mode=PC_ONLINE_NONE;
    if(companion) {
        FILE *file=fopen(stop_path,"w"); if(file) fclose(file);
        /* Browser supervisor observes the stop request and deletes its lobby.
           It exits by itself when the game process exits, including crashes. */
        WaitForSingleObject(companion,8000);
        CloseHandle(companion); companion=NULL;
    }
    pc_network_set_online(NULL);
}

PcOnlineMode pc_online_mode(void) { return active_mode; }

int pc_online_connect(void) { return pc_online_connect_mode(PC_ONLINE_ARCADE); }

int pc_online_connect_mode(PcOnlineMode mode)
{
    char root[MAX_PATH],python[MAX_PATH],script[MAX_PATH],folder[MAX_PATH],command[2048],line[128];
    STARTUPINFOA startup={sizeof startup}; PROCESS_INFORMATION process={0};
    DWORD pid=GetCurrentProcessId(); unsigned i; FILE *file; HWND owner=xbox_D3D8Window();
    const char *kind;
    if(mode==PC_ONLINE_STORY) kind="story";
    else if(mode==PC_ONLINE_ARCADE) kind="arcade";
    else return 0;
    pc_online_disconnect();
    if(!GetModuleFileNameA(NULL,root,sizeof root)) return 0;
    /* Portable build: package/Bin/game.exe, with its own isolated runtime. */
    for(i=0;i<2;i++) { char *slash=strrchr(root,'\\'); if(!slash) return 0; *slash=0; }
    snprintf(python,sizeof python,"%s\\Support\\Python\\pythonw.exe",root);
    snprintf(script,sizeof script,"%s\\Support\\Online\\online.py",root);
    if(GetFileAttributesA(python)!=INVALID_FILE_ATTRIBUTES && GetFileAttributesA(script)!=INVALID_FILE_ATTRIBUTES) {
        snprintf(folder,sizeof folder,"%s\\Logs",root); CreateDirectoryA(folder,NULL);
        snprintf(folder,sizeof folder,"%s\\Logs\\Online",root);
    } else {
        /* Source build: project/build/windows/Configuration/game.exe. */
        for(i=0;i<2;i++) { char *slash=strrchr(root,'\\'); if(!slash) return 0; *slash=0; }
        snprintf(python,sizeof python,"%s\\.venv\\Scripts\\pythonw.exe",root);
        snprintf(script,sizeof script,"%s\\tools\\network\\online.py",root);
        snprintf(folder,sizeof folder,"%s\\private\\online",root);
    }
    if(GetFileAttributesA(python)==INVALID_FILE_ATTRIBUTES || GetFileAttributesA(script)==INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL,"Online support files are missing. Extract the complete game package, or run Setup in the source workspace.","Online",MB_OK|MB_ICONINFORMATION);
        return 0;
    }
    CreateDirectoryA(folder,NULL);
    snprintf(ready_path,sizeof ready_path,"%s\\%lu.ready",folder,pid);
    snprintf(stop_path,sizeof stop_path,"%s\\%lu.stop",folder,pid);
    DeleteFileA(ready_path); DeleteFileA(stop_path);
    snprintf(command,sizeof command,"\"%s\" -B \"%s\" --pid %lu --result \"%s\" --compatibility %s --mode %s",
        python,script,pid,ready_path,TSFP_NETWORK_COMPATIBILITY_HEX,kind);
    if(!CreateProcessA(python,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,root,&startup,&process)) {
        MessageBoxA(NULL,"Could not start the Online browser.","Online",MB_OK|MB_ICONERROR); return 0;
    }
    CloseHandle(process.hThread); companion=process.hProcess;
    EnableWindow(owner,FALSE);
    while(WaitForSingleObject(companion,100)==WAIT_TIMEOUT) {
        MSG message;
        while(PeekMessage(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessage(&message); }
        file=fopen(ready_path,"r");
        if(file) {
            int accepted=fgets(line,sizeof line,file)!=NULL && pc_network_set_online(line);
            fclose(file);
            if(accepted) { active_mode=mode; EnableWindow(owner,TRUE); return 1; }
        }
    }
    CloseHandle(companion); companion=NULL;
    EnableWindow(owner,TRUE);
    return 0;
}
