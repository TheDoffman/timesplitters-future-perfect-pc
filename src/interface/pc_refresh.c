#define WIN32_LEAN_AND_MEAN
#include "pc_refresh.h"
#include <stdio.h>
#include <wchar.h>

static HWND game_window;
static int requested, fullscreen, busy, changed;
static WCHAR device[CCHDEVICENAME];
static DEVMODEW original, applied;
static HANDLE guard_process, guard_ready;
static WCHAR guard_device[CCHDEVICENAME];
static DEVMODEW guard_mode;
static unsigned guard_target, guard_count;
#ifndef PC_REFRESH_START_GUARD
#define PC_REFRESH_START_GUARD guard
#endif

/* The normal crash helper also supports an independent, sleeping refresh
 * watchdog. It restores the saved mode if this process is killed or crashes.
 * Start it before changing the monitor and require its ready handshake. */
static int guard(const WCHAR *name, const DEVMODEW *old, unsigned target)
{
    WCHAR helper[MAX_PATH], command[1024], event_name[96];
    STARTUPINFOW startup={sizeof startup};
    PROCESS_INFORMATION process={0};
    HANDLE ready;
    WCHAR *slash;
    if(guard_process && WaitForSingleObject(guard_process,0)==WAIT_TIMEOUT &&
        !wcscmp(name,guard_device) && guard_target==target &&
        guard_mode.dmPelsWidth==old->dmPelsWidth && guard_mode.dmPelsHeight==old->dmPelsHeight &&
        guard_mode.dmBitsPerPel==old->dmBitsPerPel && guard_mode.dmDisplayFrequency==old->dmDisplayFrequency) {
        SetEvent(guard_ready);return 1;
    }
    if(guard_count>=16) return 0;
    DWORD length=GetModuleFileNameW(NULL,helper,MAX_PATH);
    if(!length || length>=MAX_PATH || !(slash=wcsrchr(helper,L'\\'))) return 0;
    if((size_t)(slash-helper)+26>=MAX_PATH) return 0;
    wcscpy(slash+1,L"tsfp_crash_helper.exe");
    swprintf(event_name,96,L"Local\\TSFP.Refresh.%lu.%llu",GetCurrentProcessId(),GetTickCount64());
    ready=CreateEventW(NULL,TRUE,FALSE,event_name);
    if(!ready) return 0;
    swprintf(command,1024,L"\"%s\" --restore-refresh %lu \"%s\" %lu %lu %lu %lu %u \"%s\"",
        helper,GetCurrentProcessId(),name,old->dmPelsWidth,old->dmPelsHeight,
        old->dmBitsPerPel,old->dmDisplayFrequency,target,event_name);
    int ok=CreateProcessW(helper,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)!=0;
    if(ok) {
        CloseHandle(process.hThread);
        ok=WaitForSingleObject(ready,2000)==WAIT_OBJECT_0;
        if(ok) {
            if(guard_ready) {ResetEvent(guard_ready);CloseHandle(guard_ready);}
            if(guard_process) CloseHandle(guard_process);
            guard_ready=ready;guard_process=process.hProcess;
            wcscpy(guard_device,name);guard_mode=*old;guard_target=target;guard_count++;
            return 1;
        }
        CloseHandle(process.hProcess);
    }
    CloseHandle(ready);
    return ok;
}

static int same_mode(const DEVMODEW *a,const DEVMODEW *b)
{
    return a->dmPelsWidth==b->dmPelsWidth && a->dmPelsHeight==b->dmPelsHeight &&
        a->dmBitsPerPel==b->dmBitsPerPel && a->dmDisplayFrequency==b->dmDisplayFrequency;
}

static void restore(void)
{
    DEVMODEW current={0};current.dmSize=sizeof current;
    if(!changed) return;
    /* Preserve a display mode the user selected independently while playing. */
    /* A temporarily unavailable monitor is not evidence of an independent
       mode change. Keep ownership and the crash guard armed so we can retry. */
    if(!EnumDisplaySettingsW(device,ENUM_CURRENT_SETTINGS,&current)) return;
    if(same_mode(&current,&applied)) {
        original.dmFields=DM_DISPLAYFREQUENCY;
        LONG result=ChangeDisplaySettingsExW(device,&original,NULL,CDS_FULLSCREEN,NULL);
        fprintf(stderr,"[PC-REFRESH] restore %lu Hz: %ld\n",original.dmDisplayFrequency,result);
        if(result!=DISP_CHANGE_SUCCESSFUL) return;
    }
    changed=0;
    if(guard_ready) ResetEvent(guard_ready);
}

static void update(void)
{
    MONITORINFOEXW monitor={0};
    DEVMODEW current={0}, candidate={0};
    int found=0;
    if(busy) return;
    busy=1;
    if(!fullscreen || !requested || GetForegroundWindow()!=game_window || IsIconic(game_window)) {
        restore();goto done;
    }
    monitor.cbSize=sizeof monitor;
    if(!GetMonitorInfoW(MonitorFromWindow(game_window,MONITOR_DEFAULTTONEAREST),(MONITORINFO*)&monitor)) goto done;
    if(changed && wcscmp(monitor.szDevice,device)) restore();
    if(changed) goto done;
    current.dmSize=sizeof current;
    if(!EnumDisplaySettingsW(monitor.szDevice,ENUM_CURRENT_SETTINGS,&current)) goto done;
    if(current.dmDisplayFrequency==(DWORD)requested) goto done;
    for(DWORD index=0;;index++) {
        candidate.dmSize=sizeof candidate;candidate.dmDriverExtra=0;
        if(!EnumDisplaySettingsW(monitor.szDevice,index,&candidate)) break;
        if(candidate.dmPelsWidth==current.dmPelsWidth && candidate.dmPelsHeight==current.dmPelsHeight &&
            candidate.dmBitsPerPel==current.dmBitsPerPel && candidate.dmDisplayFrequency==(DWORD)requested &&
            !(candidate.dmDisplayFlags&DM_INTERLACED)) { found=1;break; }
    }
    if(!found) {
        fprintf(stderr,"[PC-REFRESH] %d Hz unavailable at the monitor's current desktop resolution; unchanged\n",requested);
        goto done;
    }
    candidate.dmFields=DM_DISPLAYFREQUENCY;
    LONG result=ChangeDisplaySettingsExW(monitor.szDevice,&candidate,NULL,CDS_TEST,NULL);
    if(result!=DISP_CHANGE_SUCCESSFUL || !PC_REFRESH_START_GUARD(monitor.szDevice,&current,requested)) {
        fprintf(stderr,"[PC-REFRESH] %d Hz failed validation or recovery setup; unchanged\n",requested);
        goto done;
    }
    wcscpy(device,monitor.szDevice);original=current;applied=candidate;
    /* Mark before the call because Windows sends display messages inside it. */
    changed=1;
    result=ChangeDisplaySettingsExW(device,&candidate,NULL,CDS_FULLSCREEN,NULL);
    if(result!=DISP_CHANGE_SUCCESSFUL) { changed=0;if(guard_ready)ResetEvent(guard_ready); }
    fprintf(stderr,"[PC-REFRESH] requested %d Hz: %ld (desktop resolution preserved)\n",requested,result);
done:
    busy=0;
}

void pc_refresh_fullscreen(HWND window,int on,int hz)
{
    game_window=window;fullscreen=!!on;requested=(hz==120 || hz==144)?hz:0;
    update();
}
void pc_refresh_message(HWND window,UINT message,WPARAM wp)
{
    if(window!=game_window) return;
    if(message==WM_CLOSE || message==WM_DESTROY) { fullscreen=0;update(); }
    else if(message==WM_ACTIVATEAPP) {
        if(!wp) { if(!busy) {busy=1;restore();busy=0;} }
        else update();
    }
}
void pc_refresh_shutdown(void) { fullscreen=0;update(); }
