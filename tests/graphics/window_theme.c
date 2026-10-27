#include "pc_window_theme.h"
#include <stdio.h>

int main(void)
{
    typedef HRESULT (WINAPI *GetAttribute)(HWND, DWORD, PVOID, DWORD);
    HMODULE library = LoadLibraryExW(L"dwmapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    GetAttribute get = library ? (GetAttribute)GetProcAddress(library, "DwmGetWindowAttribute") : NULL;
    HWND window = CreateWindowExW(0, L"STATIC", L"Title bar regression", WS_OVERLAPPEDWINDOW,
        0, 0, 320, 160, NULL, NULL, GetModuleHandleW(NULL), NULL);
    int value, dark;
    LONG_PTR style, borderless;
    if (!window) return 1;
    if (!get || !pc_window_theme_set(window, 0)) {
        puts("SKIP: optional DWM theme attribute unavailable");
        DestroyWindow(window); if (library) FreeLibrary(library); return 0;
    }
    for (dark = 0; dark <= 1; ++dark) {
        if (!pc_window_theme_set(window, dark)) return 2;
        value = -1;
        if (FAILED(get(window, 20, &value, sizeof(value))) || value != dark) return 3;
    }
    pc_window_theme_apply(window);
    if (FAILED(get(window, 20, &value, sizeof(value))) || value != pc_window_theme_dark()) return 4;
    style = GetWindowLongPtrW(window, GWL_STYLE);
    SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP);
    borderless = GetWindowLongPtrW(window, GWL_STYLE);
    pc_window_theme_apply(window);
    if (GetWindowLongPtrW(window, GWL_STYLE) != borderless) return 5;
    SetWindowLongPtrW(window, GWL_STYLE, style);
    pc_window_theme_apply(window);
    if (GetWindowLongPtrW(window, GWL_STYLE) != style) return 6;
    DestroyWindow(window); FreeLibrary(library);
    puts("PASS: light, dark, system preference and borderless style preservation");
    return 0;
}
