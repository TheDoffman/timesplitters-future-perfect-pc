#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <wchar.h>
static int current_hz=120,active_on_death=1,active,changes;
static HANDLE WINAPI open_process(DWORD access,BOOL inherit,DWORD pid) {return (HANDLE)1;}
static HANDLE WINAPI open_event(DWORD access,BOOL inherit,LPCWSTR name) {return (HANDLE)2;}
static BOOL WINAPI close_handle(HANDLE h) {return TRUE;}
static BOOL WINAPI set_event(HANDLE h) {active=1;return TRUE;}
static DWORD WINAPI wait_handle(HANDLE h,DWORD ms) {
    if(h==(HANDLE)1) {active=active_on_death;return WAIT_OBJECT_0;}
    return active?WAIT_OBJECT_0:WAIT_TIMEOUT;
}
static BOOL WINAPI settings(LPCWSTR device,DWORD index,DEVMODEW *mode) {
    if(index!=ENUM_CURRENT_SETTINGS && index!=0)return FALSE;
    mode->dmPelsWidth=3440;mode->dmPelsHeight=1440;mode->dmBitsPerPel=32;
    mode->dmDisplayFrequency=index==ENUM_CURRENT_SETTINGS?current_hz:144;return TRUE;
}
static LONG WINAPI change(LPCWSTR name,DEVMODEW *mode,HWND owner,DWORD flags,LPVOID unused) {
    assert(flags==CDS_FULLSCREEN && mode->dmFields==DM_DISPLAYFREQUENCY);
    current_hz=mode->dmDisplayFrequency;changes++;return 0;
}
#define OpenProcess open_process
#define OpenEventW open_event
#define CloseHandle close_handle
#define SetEvent set_event
#define WaitForSingleObject wait_handle
#define EnumDisplaySettingsW settings
#define ChangeDisplaySettingsExW change
#include "../src/interface/pc_refresh_guard.h"

int main(void)
{
    WCHAR *args[]={L"helper",L"--restore-refresh",L"42",L"\\\\.\\DISPLAY1",L"3440",L"1440",L"32",L"144",L"120",L"ready"};
    assert(pc_refresh_guard(10,args)==0 && current_hz==144 && changes==1);
    current_hz=120;active_on_death=0;
    assert(pc_refresh_guard(10,args)==0 && current_hz==120 && changes==1);
    current_hz=100;active_on_death=1;
    assert(pc_refresh_guard(10,args)==0 && current_hz==100 && changes==1);
    args[8]=L"60";assert(pc_refresh_guard(10,args)==1 && changes==1);
    args[8]=L"144junk";assert(pc_refresh_guard(10,args)==1 && changes==1);
    args[8]=L"120";args[3]=L"wrong";assert(pc_refresh_guard(10,args)==1 && changes==1);
    puts("PASS: process-death restoration, inactive/user-changed preservation and strict arguments");
    return 0;
}
