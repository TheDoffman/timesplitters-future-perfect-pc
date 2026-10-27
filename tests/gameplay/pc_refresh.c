/* Mock physical display mutations. No monitor settings change in this test. */
#include <windows.h>
#include <assert.h>
#include <wchar.h>
static HWND window=(HWND)1,foreground=(HWND)1;
static int hz=144,changes,tests,guards,guard_ok=1,has120=1,query_ok=1,restore_ok=1;
static HWND WINAPI foreground_window(void) { return foreground; }
static BOOL WINAPI iconic(HWND unused) { return FALSE; }
static HMONITOR WINAPI monitor_from(HWND w,DWORD flags) { return (HMONITOR)1; }
static BOOL WINAPI monitor_info(HMONITOR m,LPMONITORINFO info) {
    wcscpy(((MONITORINFOEXW*)info)->szDevice,L"\\\\.\\DISPLAY1");return TRUE;
}
static BOOL WINAPI display_settings(LPCWSTR name,DWORD index,DEVMODEW *mode) {
    if(index==ENUM_CURRENT_SETTINGS && !query_ok)return FALSE;
    if(index!=ENUM_CURRENT_SETTINGS && index>1) return FALSE;
    mode->dmPelsWidth=3440;mode->dmPelsHeight=1440;mode->dmBitsPerPel=32;mode->dmDisplayFlags=0;
    mode->dmDisplayFrequency=index==ENUM_CURRENT_SETTINGS?hz:index==0 && has120?120:144;
    return TRUE;
}
static LONG WINAPI change_settings(LPCWSTR name,DEVMODEW *mode,HWND owner,DWORD flags,LPVOID unused) {
    assert(mode->dmFields==DM_DISPLAYFREQUENCY);
    if(flags==CDS_TEST) tests++;
    else {assert(flags==CDS_FULLSCREEN);if(!restore_ok && mode->dmDisplayFrequency==144)return DISP_CHANGE_FAILED;changes++;hz=mode->dmDisplayFrequency;}
    return DISP_CHANGE_SUCCESSFUL;
}
static int fake_guard(const WCHAR *name,const DEVMODEW *mode,unsigned target) {guards++;return guard_ok;}
#define PC_REFRESH_START_GUARD fake_guard
#define GetForegroundWindow foreground_window
#define IsIconic iconic
#define MonitorFromWindow monitor_from
#define GetMonitorInfoW monitor_info
#define EnumDisplaySettingsW display_settings
#define ChangeDisplaySettingsExW change_settings
#include "../src/interface/pc_refresh.c"

int main(void)
{
    pc_refresh_fullscreen(window,1,0);assert(hz==144 && !changes);
    pc_refresh_fullscreen(window,0,120);assert(!changes);
    pc_refresh_fullscreen(window,1,120);assert(hz==120 && changes==1 && tests==1 && guards==1);
    pc_refresh_message(window,WM_ACTIVATEAPP,0);assert(hz==144 && changes==2);
    pc_refresh_message(window,WM_ACTIVATEAPP,1);assert(hz==120 && changes==3);
    pc_refresh_message(window,WM_CLOSE,0);assert(hz==144 && changes==4);
    has120=0;pc_refresh_fullscreen(window,1,120);assert(hz==144 && changes==4);
    has120=1;guard_ok=0;pc_refresh_fullscreen(window,1,120);assert(hz==144 && changes==4);
    guard_ok=1;foreground=NULL;pc_refresh_fullscreen(window,1,120);assert(hz==144 && changes==4);
    foreground=window;pc_refresh_message(window,WM_ACTIVATEAPP,1);assert(hz==120);
    hz=100;pc_refresh_shutdown();assert(hz==100); /* Preserve independent user changes. */
    hz=144;pc_refresh_fullscreen(window,1,120);assert(hz==120);
    pc_refresh_message(window,WM_DESTROY,0);assert(hz==144);
    /* A transient enumeration failure must not disarm process-death recovery. */
    guard_ready=CreateEventW(NULL,TRUE,TRUE,NULL);assert(guard_ready);
    pc_refresh_fullscreen(window,1,120);assert(changed && hz==120);
    query_ok=0;pc_refresh_message(window,WM_ACTIVATEAPP,0);
    assert(changed && hz==120 && WaitForSingleObject(guard_ready,0)==WAIT_OBJECT_0);
    query_ok=1;restore_ok=0;pc_refresh_shutdown();
    assert(changed && hz==120 && WaitForSingleObject(guard_ready,0)==WAIT_OBJECT_0);
    restore_ok=1;pc_refresh_shutdown();
    assert(!changed && hz==144 && WaitForSingleObject(guard_ready,0)==WAIT_TIMEOUT);
    CloseHandle(guard_ready);guard_ready=NULL;
    puts("PASS: supported refresh, unchanged resolution, focus/exit restoration and failure/user-change preservation");
    return 0;
}
