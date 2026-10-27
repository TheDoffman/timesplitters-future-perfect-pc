/* Explicit opt-in integration test: temporarily switches the primary monitor. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/interface/pc_refresh.h"
static LRESULT CALLBACK window_proc(HWND w,UINT m,WPARAM p,LPARAM l)
{ pc_refresh_message(w,m,p);return DefWindowProcW(w,m,p,l); }
static DWORD rate(void)
{ DEVMODEW mode={0};mode.dmSize=sizeof mode;assert(EnumDisplaySettingsW(NULL,ENUM_CURRENT_SETTINGS,&mode));return mode.dmDisplayFrequency; }
int main(int argc,char **argv)
{
    DWORD original=rate();
    WNDCLASSW cls={0};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"TSFP.Refresh.Validation";
    assert(RegisterClassW(&cls));
    HWND window=CreateWindowW(cls.lpszClassName,L"TSFP temporary refresh validation",WS_POPUP|WS_VISIBLE,0,0,640,480,NULL,NULL,cls.hInstance,NULL);
    assert(window);SetForegroundWindow(window);
    if(GetForegroundWindow()!=window) { puts("SKIP: Windows did not grant foreground focus");DestroyWindow(window);return 77; }
    pc_refresh_fullscreen(window,1,120);
    printf("original=%lu requested=120 actual=%lu\n",original,rate());fflush(stdout);
    if(rate()!=120) {pc_refresh_shutdown();DestroyWindow(window);return 78;}
    if(argc>1 && !strcmp(argv[1],"--abrupt")) ExitProcess(0);
    pc_refresh_message(window,WM_ACTIVATEAPP,0);assert(rate()==original);
    pc_refresh_message(window,WM_ACTIVATEAPP,1);assert(rate()==120);
    pc_refresh_fullscreen(window,0,120);assert(rate()==original);
    pc_refresh_fullscreen(window,1,144);
    assert(rate()==144);pc_refresh_shutdown();assert(rate()==original);
    DestroyWindow(window);puts("PASS: physical 120/144 Hz and focus/normal-exit restoration");
    return 0;
}
