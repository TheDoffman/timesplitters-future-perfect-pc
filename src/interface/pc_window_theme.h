#ifndef TSFP_PC_WINDOW_THEME_H
#define TSFP_PC_WINDOW_THEME_H

/* Windows adapter only. Resolve optional APIs without new linker dependencies. */
#include <windows.h>

static int pc_window_theme_dark(void)
{
    typedef LSTATUS (WINAPI *ReadValue)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
    HIGHCONTRASTW contrast = { sizeof(contrast) };
    HMODULE library;
    ReadValue read;
    DWORD light = 1, size = sizeof(light);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON)) return 0;
    library = LoadLibraryExW(L"advapi32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return 0;
    read = (ReadValue)GetProcAddress(library, "RegGetValueW");
    if (!read || read(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &size) != ERROR_SUCCESS) light = 1;
    FreeLibrary(library);
    return light == 0;
}

static int pc_window_theme_set(HWND window, int dark)
{
    typedef HRESULT (WINAPI *SetAttribute)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE library;
    SetAttribute set;
    BOOL value = dark != 0;
    HRESULT result = E_NOTIMPL;
    if (!window) return 0;
    library = LoadLibraryExW(L"dwmapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return 0;
    set = (SetAttribute)GetProcAddress(library, "DwmSetWindowAttribute");
    if (set) result = set(window, 20, &value, sizeof(value));
    FreeLibrary(library);
    return SUCCEEDED(result);
}

static void pc_window_theme_apply(HWND window)
{
    pc_window_theme_set(window, pc_window_theme_dark());
}
#endif
