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
static char ready_path[MAX_PATH],stop_path[MAX_PATH];
extern HWND xbox_D3D8Window(void);

void pc_online_disconnect(void)
{
    if(companion) {
        FILE *file=fopen(stop_path,"w"); if(file) fclose(file);
        /* Browser supervisor observes the stop request and deletes its lobby.
           It exits by itself when the game process exits, including crashes. */
        WaitForSingleObject(companion,8000);
        CloseHandle(companion); companion=NULL;
    }
    pc_network_set_online(NULL);
}

int pc_online_connect(void)
{
    char root[MAX_PATH],python[MAX_PATH],script[MAX_PATH],folder[MAX_PATH],command[2048],line[128];
    STARTUPINFOA startup={sizeof startup}; PROCESS_INFORMATION process={0};
    DWORD pid=GetCurrentProcessId(); unsigned i; FILE *file; HWND owner=xbox_D3D8Window();
    pc_online_disconnect();
    if(!GetModuleFileNameA(NULL,root,sizeof root)) return 0;
    /* Source build: project/build/windows/Configuration/game.exe. */
    for(i=0;i<4;i++) { char *slash=strrchr(root,'\\'); if(!slash) return 0; *slash=0; }
    snprintf(python,sizeof python,"%s\\.venv\\Scripts\\pythonw.exe",root);
    snprintf(script,sizeof script,"%s\\tools\\online.py",root);
    if(GetFileAttributesA(python)==INVALID_FILE_ATTRIBUTES || GetFileAttributesA(script)==INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL,"Online requires the source workspace and its Python environment. Launch the current source build.","Online",MB_OK|MB_ICONINFORMATION);
        return 0;
    }
    snprintf(folder,sizeof folder,"%s\\private\\online",root); CreateDirectoryA(folder,NULL);
    snprintf(ready_path,sizeof ready_path,"%s\\%lu.ready",folder,pid);
    snprintf(stop_path,sizeof stop_path,"%s\\%lu.stop",folder,pid);
    DeleteFileA(ready_path); DeleteFileA(stop_path);
    snprintf(command,sizeof command,"\"%s\" \"%s\" --pid %lu --result \"%s\" --compatibility %s",
        python,script,pid,ready_path,TSFP_NETWORK_COMPATIBILITY_HEX);
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
            if(accepted) { EnableWindow(owner,TRUE); return 1; }
        }
    }
    CloseHandle(companion); companion=NULL;
    EnableWindow(owner,TRUE);
    return 0;
}
