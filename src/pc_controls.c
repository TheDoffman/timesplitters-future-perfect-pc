#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_controls.h"
#include "pc_input.h"
#include "pc_settings.h"
#include "pc_prompts.h"
#include "texture_mode.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SRWLOCK lock = SRWLOCK_INIT;
static PcInput input;
static HWND window_handle;
static int enabled = 1, want_capture = 1, replaying;
static float sensitivity = 0.10f;
static int invert_y;
static int wheel_remainder;
static unsigned long applied;
static volatile LONG editing;

/* Deterministic integration tests use the same key/motion model as the
   window messages. No desktop keyboard or mouse events are synthesized. */
static struct Replay { unsigned long at; char type[12]; int a, b; } replay[256];
static int replay_count, replay_next;
static ULONGLONG replay_started;

static void replay_tick(void)
{
    ULONGLONG now = GetTickCount64();
    if (!replaying) return;
    if (!replay_started) replay_started = now;
    while (replay_next < replay_count && now - replay_started >= replay[replay_next].at) {
        struct Replay *r = &replay[replay_next++];
        if (!strcmp(r->type, "key")) pc_input_key(&input, (unsigned)r->a, r->b);
        else if (!strcmp(r->type, "mouse")) pc_input_motion(&input, r->a, r->b, now);
        else if (!strcmp(r->type, "wheel")) pc_input_wheel(&input, r->a);
        fprintf(stderr, "[PC-INPUT-TEST] %lu %s %d %d\n", r->at, r->type, r->a, r->b);
    }
}

static void clip_to_window(HWND window)
{
    RECT rect;
    POINT origin = {0, 0};
    if (!GetClientRect(window, &rect) || !ClientToScreen(window, &origin) ||
        rect.right <= 0 || rect.bottom <= 0) return;
    OffsetRect(&rect, origin.x, origin.y);
    ClipCursor(&rect);
}

/* Window thread only: it owns cursor confinement and visibility. */
static void capture(HWND window, int on)
{
    AcquireSRWLockExclusive(&lock);
    pc_input_capture(&input, enabled && on && !replaying);
    wheel_remainder = 0;
    on = input.captured;
    ReleaseSRWLockExclusive(&lock);
    if (on) { clip_to_window(window); SetCursor(NULL); }
    else { ClipCursor(NULL); SetCursor(LoadCursor(NULL, IDC_ARROW)); }
}

void pc_controls_attach(HWND window)
{
    const char *v;
    FILE *f;
    RAWINPUTDEVICE mouse = { 0x01, 0x02, 0, NULL };
    window_handle = window;
    v = getenv("RECOMP_PC_MOUSE");
    enabled = !v || strcmp(v, "0");
    v = getenv("RECOMP_MOUSE_SENSITIVITY");
    if (v) {
        char *end;
        double x = strtod(v, &end);
        if (end != v && !*end && isfinite(x) && x >= 0.001 && x <= 10) sensitivity = (float)x;
    }
    v = getenv("RECOMP_MOUSE_INVERT_Y");
    invert_y = v && !strcmp(v, "1");
    mouse.hwndTarget = window;
    if (window && !RegisterRawInputDevices(&mouse, 1, sizeof mouse)) {
        fprintf(stderr, "[PC-INPUT] raw mouse registration failed: %lu\n", GetLastError());
        enabled = 0;
    }
    v = getenv("TSFP_PC_INPUT_REPLAY");
    if (v && (f = fopen(v, "r")) != NULL) {
        char line[160];
        while (replay_count < 256 && fgets(line, sizeof line, f)) {
            struct Replay *r = &replay[replay_count];
            if (sscanf(line, "%lu %11s %d %d", &r->at, r->type, &r->a, &r->b) == 4) replay_count++;
        }
        fclose(f);
        replaying = 1;
        pc_input_focus(&input, 1);
        pc_input_capture(&input, 1);
    }
    fprintf(stderr, "[PC-INPUT] WASD/mouse controls ready; %.3f degrees/count, invert Y %s. F1 shows/hides the menu bar; F2 opens settings; F10 switches Classic/4K textures; F12 opens Debug; Alt+Enter toggles fullscreen; Escape releases mouse.\n",
            sensitivity, invert_y ? "on" : "off");
    if (window) pc_settings_attach(window);
    pc_debug_init(window);
}

void pc_controls_get_mouse(double *value, int *on, int *inverted)
{
    AcquireSRWLockShared(&lock);
    *value = sensitivity; *on = enabled; *inverted = invert_y;
    ReleaseSRWLockShared(&lock);
}

void pc_controls_set_mouse(double value, int on, int inverted)
{
    AcquireSRWLockExclusive(&lock);
    sensitivity = (float)value; enabled = !!on; invert_y = !!inverted;
    pc_input_focus(&input, input.focused);
    wheel_remainder = 0;
    ReleaseSRWLockExclusive(&lock);
}

static void show_settings(HWND window, int debug)
{
    if (InterlockedCompareExchange(&editing, 1, 0)) return;
    capture(window, 0);
    if (debug) pc_debug_show(window); else pc_settings_show(window);
    AcquireSRWLockExclusive(&lock);
    pc_input_focus(&input, replaying || GetForegroundWindow() == window);
    if(replaying) pc_input_capture(&input,1);
    ReleaseSRWLockExclusive(&lock);
    InterlockedExchange(&editing, 0);
    if(!replaying) capture(window, want_capture);
}

static void toggle_fullscreen(HWND window)
{
    /* Use the runtime's existing Alt+Enter path on its window thread. */
    PostMessage(window, WM_SYSKEYDOWN, VK_RETURN, (LPARAM)1 << 29);
}

static void update_fullscreen_menu(HWND window)
{
    HMENU menu = GetSubMenu(GetMenu(window), 0);
    if (menu) CheckMenuItem(menu, PC_FULLSCREEN_COMMAND, MF_BYCOMMAND |
        ((GetWindowLongPtr(window, GWL_STYLE) & WS_CAPTION) ? MF_UNCHECKED : MF_CHECKED));
    if (menu) CheckMenuItem(menu, PC_TEXTURE_MODE_COMMAND, MF_BYCOMMAND |
        (tsfp_texture_is_4k() ? MF_CHECKED : MF_UNCHECKED));
}

int pc_controls_message(HWND window, UINT message, WPARAM wp, LPARAM lp, LRESULT *result)
{
    int captured;
    *result = 0;
    if (window != window_handle) return 0;
    if (message == WM_TIMER && wp == PC_DEBUG_TIMER) { pc_debug_poll(); return 1; }
    if (message == WM_DESTROY) { pc_settings_detach(window); pc_debug_shutdown(); }
    if(replaying) return 0;
    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wp) == PC_TEXTURE_MODE_COMMAND) {
            tsfp_texture_toggle(); update_fullscreen_menu(window); return 1;
        }
        if (LOWORD(wp) == PC_MENU_BAR_COMMAND) {
            pc_settings_toggle_menu(window); update_fullscreen_menu(window); return 1;
        }
        if (LOWORD(wp) == PC_FULLSCREEN_COMMAND) { toggle_fullscreen(window); return 1; }
        if (LOWORD(wp) == PC_SETTINGS_COMMAND || LOWORD(wp) == PC_DEBUG_COMMAND) {
            show_settings(window, LOWORD(wp) == PC_DEBUG_COMMAND); return 1;
        }
        break;
    case WM_INITMENUPOPUP:
        update_fullscreen_menu(window);
        break;
    case WM_STYLECHANGED:
        if ((int)wp == GWL_STYLE) update_fullscreen_menu(window);
        break;
    case WM_ENTERMENULOOP:
        capture(window, 0);
        break;
    case WM_EXITMENULOOP:
        capture(window, want_capture);
        break;
    case WM_MOUSEACTIVATE:
        AcquireSRWLockShared(&lock); captured = input.captured; ReleaseSRWLockShared(&lock);
        if (!captured && enabled) {
            want_capture = 1;
            *result = MA_ACTIVATEANDEAT; /* focus the game without an accidental shot */
            return 1;
        }
        break;
    case WM_SETFOCUS:
        AcquireSRWLockExclusive(&lock);
        pc_input_focus(&input, 1);
        ReleaseSRWLockExclusive(&lock);
        capture(window, want_capture);
        break;
    case WM_KILLFOCUS:
        capture(window, 0);
        AcquireSRWLockExclusive(&lock);
        pc_input_focus(&input, 0);
        ReleaseSRWLockExclusive(&lock);
        break;
    case WM_CLOSE: case WM_DESTROY:
        capture(window, 0);
        break;
    case WM_MOVE: case WM_SIZE:
        AcquireSRWLockShared(&lock); captured = input.captured; ReleaseSRWLockShared(&lock);
        if (captured) clip_to_window(window);
        break;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        pc_prompts_keyboard();
        if (wp == VK_F10) {
            if (!(lp & (1LL << 30))) {
                tsfp_texture_toggle(); update_fullscreen_menu(window);
            }
            return 1;
        }
        if (wp == VK_F2 || wp == VK_F12) {
            if (!(lp & (1LL << 30))) show_settings(window, wp == VK_F12);
            return 1;
        }
        if (wp == VK_F11) {
            /* Ctrl+F11 belongs to the renderer's diagnostic frame capture. */
            return 1;
        }
        if (message == WM_SYSKEYDOWN && wp == VK_RETURN && (lp & (1LL << 29))) break;
        if (wp == VK_F1) {
            if (!(lp & (1LL << 30))) {
                pc_settings_toggle_menu(window);
                update_fullscreen_menu(window);
            }
            return 1;
        }
        if (wp == VK_ESCAPE && !(lp & (1LL << 30))) {
            want_capture = 0;
            capture(window, 0);
        }
        AcquireSRWLockExclusive(&lock);
        pc_input_key(&input, (unsigned)wp, 1);
        ReleaseSRWLockExclusive(&lock);
        break;
    case WM_KEYUP: case WM_SYSKEYUP:
        if (wp == VK_F1 || wp == VK_F2 || wp == VK_F12 || wp == VK_F11 || wp == VK_F10) return 1;
        AcquireSRWLockExclusive(&lock);
        pc_input_key(&input, (unsigned)wp, 0);
        ReleaseSRWLockExclusive(&lock);
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {
        unsigned key = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ? VK_LBUTTON :
                       message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ? VK_RBUTTON : VK_MBUTTON;
        int down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
        if (down) pc_prompts_keyboard();
        AcquireSRWLockShared(&lock); captured = input.captured; ReleaseSRWLockShared(&lock);
        if (!captured && message == WM_LBUTTONDOWN && enabled) {
            want_capture = 1;
            capture(window, 1); /* recapture click must not fire a weapon */
            return 1;
        }
        AcquireSRWLockExclusive(&lock);
        pc_input_key(&input, key, down && captured);
        ReleaseSRWLockExclusive(&lock);
        break;
    }
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        if (message == WM_XBUTTONDOWN) pc_prompts_keyboard();
        AcquireSRWLockExclusive(&lock);
        pc_input_key(&input, HIWORD(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2,
                     message == WM_XBUTTONDOWN && input.captured);
        ReleaseSRWLockExclusive(&lock);
        *result = TRUE;
        return 1;
    case WM_INPUT: {
        RAWINPUT raw;
        UINT bytes = sizeof raw;
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &raw, &bytes, sizeof raw.header) != (UINT)-1 &&
            raw.header.dwType == RIM_TYPEMOUSE) {
            if (GetForegroundWindow()==window &&
                (raw.data.mouse.usButtonFlags ||
                 (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) &&
                  (abs(raw.data.mouse.lLastX)+abs(raw.data.mouse.lLastY)>=2))))
                pc_prompts_keyboard();
            AcquireSRWLockExclusive(&lock);
            if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
                pc_input_motion(&input, raw.data.mouse.lLastX, raw.data.mouse.lLastY, GetTickCount64());
            if (raw.data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                int delta = (short)raw.data.mouse.usButtonData + wheel_remainder;
                pc_input_wheel(&input, delta / WHEEL_DELTA);
                wheel_remainder = delta % WHEEL_DELTA;
            }
            ReleaseSRWLockExclusive(&lock);
        }
        break; /* DefWindowProc must perform WM_INPUT cleanup */
    }
    case WM_SETCURSOR:
        AcquireSRWLockShared(&lock); captured = input.captured; ReleaseSRWLockShared(&lock);
        if (captured && LOWORD(lp) == HTCLIENT) { SetCursor(NULL); *result = TRUE; return 1; }
        break;
    }
    return 0;
}

int pc_controls_accept_pad(void)
{
    return !replaying && !InterlockedCompareExchange(&editing,0,0) &&
           window_handle && GetForegroundWindow()==window_handle;
}

int pc_controls_key_down(int key)
{
    int down;
    if (InterlockedCompareExchange(&editing, 0, 0)) return 0;
    AcquireSRWLockExclusive(&lock);
    replay_tick();
    down = (replaying || GetForegroundWindow() == window_handle) &&
           pc_input_down(&input, (unsigned)key, GetTickCount64());
    ReleaseSRWLockExclusive(&lock);
    return down;
}

static int readable(uintptr_t address, size_t bytes)
{
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery((const void *)address, &m, sizeof m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    return address + bytes >= address && address + bytes <= (uintptr_t)m.BaseAddress + m.RegionSize;
}

void pc_controls_apply_mouse(uint32_t player, uint32_t fov_object, ptrdiff_t memory_offset)
{
    float *yaw, *pitch, fov, old_yaw, old_pitch;
    uintptr_t p = (uintptr_t)player + memory_offset;
    uintptr_t f = (uintptr_t)fov_object + memory_offset;
    int changed;
    if (InterlockedCompareExchange(&editing, 0, 0)) return;
    if (!player || !fov_object || !readable(p, 0x108) || !readable(f + 0x384, 4)) return;
    /* Player slot/port zero only. These fields and globals are verified
       against this workspace's US XBE; no GameCube addresses are used. */
    if (*(const uint32_t *)p != 0) return;
    yaw = (float *)(p + 0x100); pitch = (float *)(p + 0x104);
    fov = *(const float *)(f + 0x384);
    if (!isfinite(fov) || fov < 1 || fov > 120) return;
    old_yaw = *yaw; old_pitch = *pitch;
    AcquireSRWLockExclusive(&lock);
    replay_tick();
    changed = (replaying || GetForegroundWindow() == window_handle) &&
        pc_input_look(&input, GetTickCount64(), sensitivity, invert_y,
                      fov < 55 ? fov / 55 : 1, yaw, pitch);
    ReleaseSRWLockExclusive(&lock);
    if (changed && applied++ < 12)
        fprintf(stderr, "[PC-MOUSE] yaw %.3f -> %.3f, pitch %.3f -> %.3f, fov %.3f\n",
                old_yaw, *yaw, old_pitch, *pitch, fov);
}
