#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_controls.h"
#include "pc_crash.h"
#include "pc_discord.h"
#include "pc_input.h"
#include "pc_text_input.h"
#include "pc_camera.h"
#include "pc_reload_guest.h"
#include "pc_mounted.h"
#include "pc_terminal.h"
#include "pc_native_rate.h"
#include "pc_settings.h"
#include "pc_prompts.h"
#include "texture_mode.h"
#include "pc_window_theme.h"
#include "pc_menu_theme.h"
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
static int menu_mode, menu_button_block;
static PcTextInput text_input;

void pc_controls_text_mode(uint32_t owner) {
    AcquireSRWLockExclusive(&lock);
    if(text_input.owner!=owner) {
        pc_text_session(&text_input,owner);
        for(unsigned i=8;i<256;i++) if(pc_text_key(i)) input.keys[i]=0;
    }
    ReleaseSRWLockExclusive(&lock);
}
unsigned pc_controls_text_event(uint32_t owner) {
    AcquireSRWLockExclusive(&lock);
    unsigned event=text_input.owner==owner?pc_text_get(&text_input):0;
    ReleaseSRWLockExclusive(&lock);
    return event;
}
static int menu_wheel;
static ULONGLONG menu_wheel_at;
static int test_cursor_x=-1,test_cursor_y=-1;
#define PC_MENU_MOUSE_MESSAGE (WM_APP+0x186)
#define PC_MOUSE_CAPTURE_TIMER 0x7e31


/* Deterministic integration tests use the same key/motion model as the
   window messages. No desktop keyboard or mouse events are synthesized. */
static struct Replay { unsigned long at; char type[12]; int a, b; } replay[256];
static int replay_count, replay_next;
static ULONGLONG replay_started;
/* Opt-in integration harness: hold a key until an atomic state-file update.
   Unlike timed startup replays this survives loading stalls and allows two
   background game processes to be driven without desktop focus changes. */
static const char *test_state_path;
static ULONGLONG test_state_checked;
static unsigned test_state_key;
static unsigned test_state_motion;

/* Caller holds lock. Raw input is the only wheel event source, avoiding a
 * duplicate step from Windows' corresponding WM_MOUSEWHEEL message. */
static void wheel_input(int steps)
{
    if(menu_mode) {
        if(!input.focused || InterlockedCompareExchange(&editing,0,0) ||
           (!replaying && GetForegroundWindow()!=window_handle)) return;
        menu_wheel+=steps>8?8:steps<-8?-8:steps;
        if(menu_wheel>8) menu_wheel=8;
        if(menu_wheel<-8) menu_wheel=-8;
        menu_wheel_at=GetTickCount64();
        if(steps)pc_settings_capture_key(steps>0?0x86:0x87);
    } else pc_input_wheel(&input,steps);
}

static void replay_tick(void)
{
    ULONGLONG now = GetTickCount64();
    if (!replaying) return;
    if(test_state_path && now-test_state_checked>=16) {
        FILE *state; unsigned key=0,motion=0; int down=0,dx=0,dy=0,fields=0;
        test_state_checked=now;
        state=fopen(test_state_path,"r");
        if(state) {
            char line[128];
            if(fgets(line,sizeof line,state)) {
                fields=sscanf(line,"%u %d %u %d %d",&key,&down,&motion,&dx,&dy);
                /* Four fields describe an absolute menu cursor; five describe
                   a sequenced gameplay motion. Do not interpret one as both. */
                if(fields==4) {
                    int x=-1,y=-1;
                    if(sscanf(line,"%u %d %d %d",&key,&down,&x,&y)==4) {
                        test_cursor_x=x;test_cursor_y=y;
                    }
                }
            }
            if(fields<2 || key>=256) {key=0;down=0;}
            fclose(state);
        }
        if(test_state_key!=key || !down) pc_input_key_at(&input,test_state_key,0,GetTickCount64());
        test_state_key=key;
        pc_input_key_at(&input,key,down!=0,GetTickCount64());
        /* Optional sequence + raw counts allow a bounded test to aim after
           loading. Apply a motion update once, regardless of polling rate. */
        if(fields==5 && motion!=test_state_motion) {
            test_state_motion=motion;pc_input_motion(&input,dx,dy,now);
        }
    }
    if (!replay_started) replay_started = now;
    while (replay_next < replay_count && now - replay_started >= replay[replay_next].at) {
        struct Replay *r = &replay[replay_next++];
        if (!strcmp(r->type, "key")) pc_input_key_at(&input, (unsigned)r->a, r->b,GetTickCount64());
        else if (!strcmp(r->type, "cursor")) { test_cursor_x=r->a; test_cursor_y=r->b; }
        else if (!strcmp(r->type, "mouse")) pc_input_motion(&input, r->a, r->b, now);
        else if (!strcmp(r->type, "wheel")) wheel_input(r->a);
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

static void restore_window_clip(HWND window)
{
    RECT expected, actual;
    POINT origin = {0, 0};
    if (!window || GetForegroundWindow() != window ||
        !GetClientRect(window, &expected) ||
        !ClientToScreen(window, &origin) ||
        expected.right <= 0 || expected.bottom <= 0) return;
    OffsetRect(&expected, origin.x, origin.y);
    if (GetClipCursor(&actual) && !EqualRect(&expected, &actual))
        ClipCursor(&expected);
}

/* Window thread only: it owns cursor confinement and visibility. */
static void capture(HWND window, int on)
{
    AcquireSRWLockExclusive(&lock);
    pc_input_capture(&input, enabled && on && !replaying && !menu_mode);
    wheel_remainder = 0;
    menu_wheel = 0;
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
    pc_window_theme_apply(window);
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
    test_state_path=getenv("TSFP_PC_INPUT_STATE");
    if(test_state_path && *test_state_path) {
        replaying=1; pc_input_focus(&input,1); pc_input_capture(&input,1);
    } else test_state_path=NULL;
    fprintf(stderr, "[PC-INPUT] WASD/mouse controls ready; %.3f degrees/count, invert Y %s. F1 shows/hides the menu bar; F2 opens settings; F10 switches Classic/4K textures; F12 opens Debug; Alt+Enter toggles fullscreen; Escape releases mouse.\n",
            sensitivity, invert_y ? "on" : "off");
    if (window) { pc_settings_attach(window); SetTimer(window, PC_MOUSE_CAPTURE_TIMER, 250, NULL); }
    pc_debug_init(window);
    pc_crash_window(window);
    pc_discord_start();
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
    pc_crash_inhibit(1);
    if (debug) pc_debug_show(window); else pc_settings_show(window);
    pc_crash_inhibit(0);
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
    int captured, frontend;
    *result = 0;
    if (window != window_handle) return 0;
    if (pc_menu_theme_message(window,message,wp,lp,result)) return 1;
    if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED) pc_window_theme_apply(window);
    if (message == WM_TIMER && wp == PC_DEBUG_TIMER) { pc_debug_poll(); return 1; }
    if (message == WM_TIMER && wp == PC_MOUSE_CAPTURE_TIMER) {
        AcquireSRWLockShared(&lock); captured = input.captured; ReleaseSRWLockShared(&lock);
        if (captured) restore_window_clip(window);
        return 1;
    }
    if (message == WM_DESTROY) { KillTimer(window, PC_MOUSE_CAPTURE_TIMER); pc_discord_stop(); pc_crash_window(NULL); pc_settings_detach(window); pc_debug_shutdown(); }
    if(message==PC_MENU_MOUSE_MESSAGE) {
        /* Escape temporarily releases capture to open Pause. Returning to
         * gameplay restores the capture preference without another click.
         * Focus loss still prevents capture and does not steal focus. */
        AcquireSRWLockShared(&lock); frontend=menu_mode; ReleaseSRWLockShared(&lock);
        if(wp && !frontend) want_capture=1;
        if(!replaying && !InterlockedCompareExchange(&editing,0,0)) capture(window,want_capture);
        return 1;
    }
    if(replaying) return 0;
    /* Binding capture owns key events before text/menu and host shortcuts. */
    if((message==WM_KEYDOWN || message==WM_SYSKEYDOWN ||
        message==WM_KEYUP || message==WM_SYSKEYUP) && pc_settings_capture_blocked()) {
        AcquireSRWLockExclusive(&lock);
        pc_input_key_at(&input,(unsigned)wp,message==WM_KEYDOWN || message==WM_SYSKEYDOWN,GetTickCount64());
        if(input.focused && (message==WM_KEYDOWN || message==WM_SYSKEYDOWN))pc_settings_capture_key((int)wp);
        ReleaseSRWLockExclusive(&lock);
        return 1;
    }
    if(message==WM_CHAR || message==WM_UNICHAR) {
        if(message==WM_UNICHAR && wp==0xffff) { *result=TRUE;return 1; }
        AcquireSRWLockExclusive(&lock);
        int typing=text_input.owner!=0;
        if(typing && input.focused) pc_text_character(&text_input,(unsigned)wp);
        ReleaseSRWLockExclusive(&lock);
        if(typing)return 1;
    }
    if((message==WM_KEYDOWN || message==WM_KEYUP) && pc_text_key((unsigned)wp)) {
        AcquireSRWLockExclusive(&lock);
        int typing=text_input.owner!=0;
        if(typing) {
            input.keys[wp]=0;
            if(message==WM_KEYDOWN && input.focused)
                pc_text_command(&text_input,(unsigned)wp,!!(lp&((LPARAM)1<<30)),!!(GetKeyState(VK_CONTROL)&0x8000));
        }
        ReleaseSRWLockExclusive(&lock);
        if(typing) { if(message==WM_KEYDOWN)pc_prompts_keyboard();return 1; }
    }
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
        pc_crash_inhibit(1);
        capture(window, 0);
        break;
    case WM_EXITMENULOOP:
        pc_crash_inhibit(0);
        capture(window, want_capture);
        break;
    case WM_MOUSEACTIVATE:
        AcquireSRWLockShared(&lock); captured = input.captured; frontend=menu_mode; ReleaseSRWLockShared(&lock);
        if (!captured && enabled && !frontend) {
            want_capture = 1;
            *result = MA_ACTIVATEANDEAT; /* focus the game without an accidental shot */
            return 1;
        }
        break;
    case WM_ACTIVATEAPP:
        if (!wp) {
            capture(window, 0);
            AcquireSRWLockExclusive(&lock);
            pc_input_focus(&input, 0);
            ReleaseSRWLockExclusive(&lock);
        } else if (GetForegroundWindow() == window) {
            AcquireSRWLockExclusive(&lock);
            pc_input_focus(&input, 1);
            ReleaseSRWLockExclusive(&lock);
            capture(window, want_capture);
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
        text_input.head=text_input.count=0;
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
        pc_input_key_at(&input, (unsigned)wp, 1,GetTickCount64());
        ReleaseSRWLockExclusive(&lock);
        break;
    case WM_KEYUP: case WM_SYSKEYUP:
        if (wp == VK_F1 || wp == VK_F2 || wp == VK_F12 || wp == VK_F11 || wp == VK_F10) return 1;
        AcquireSRWLockExclusive(&lock);
        pc_input_key_at(&input, (unsigned)wp, 0,GetTickCount64());
        ReleaseSRWLockExclusive(&lock);
        break;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {
        unsigned key = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ? VK_LBUTTON :
                       message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ? VK_RBUTTON : VK_MBUTTON;
        int down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
        if (down) pc_prompts_keyboard();
        AcquireSRWLockShared(&lock); captured = input.captured; frontend=menu_mode; ReleaseSRWLockShared(&lock);
        if (!captured && message == WM_LBUTTONDOWN && enabled && !frontend) {
            want_capture = 1;
            capture(window, 1); /* recapture click must not fire a weapon */
            return 1;
        }
        AcquireSRWLockExclusive(&lock);
        pc_input_key_at(&input, key, down && (captured || menu_mode),GetTickCount64());
        if(down && input.focused)pc_settings_capture_key((int)key);
        ReleaseSRWLockExclusive(&lock);
        break;
    }
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        if (message == WM_XBUTTONDOWN) pc_prompts_keyboard();
        AcquireSRWLockExclusive(&lock);
        pc_input_key_at(&input, HIWORD(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2,
                     message == WM_XBUTTONDOWN && (input.captured || menu_mode),GetTickCount64());
        if(message==WM_XBUTTONDOWN && input.focused)pc_settings_capture_key(HIWORD(wp)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2);
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
                wheel_input(delta / WHEEL_DELTA);
                wheel_remainder = delta % WHEEL_DELTA;
            }
            ReleaseSRWLockExclusive(&lock);
            AcquireSRWLockShared(&lock); captured = input.captured; ReleaseSRWLockShared(&lock);
            if (captured) restore_window_clip(window);
        }
        break; /* DefWindowProc must perform WM_INPUT cleanup */
    }
    case WM_SETCURSOR:
        AcquireSRWLockShared(&lock); captured = input.captured; frontend=menu_mode; ReleaseSRWLockShared(&lock);
        if (captured && LOWORD(lp) == HTCLIENT) { SetCursor(NULL); *result = TRUE; return 1; }
        if (frontend && LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, IDC_ARROW)); *result = TRUE; return 1;
        }
        break;
    }
    return 0;
}

int pc_controls_weapon_slots_active(void)
{
    int active;
    AcquireSRWLockShared(&lock);
    active=!InterlockedCompareExchange(&editing,0,0) && !menu_mode && !text_input.owner &&
        input.focused && input.captured && (replaying || GetForegroundWindow()==window_handle);
    ReleaseSRWLockShared(&lock);
    return active;
}

void pc_controls_reload_sample(const unsigned char roles[256],int active,
                               int *reload,int *fire,int *secondary)
{
    AcquireSRWLockExclusive(&lock);
    replay_tick();
    active=active && !InterlockedCompareExchange(&editing,0,0) && !menu_mode && !text_input.owner &&
        input.focused && input.captured && (replaying || GetForegroundWindow()==window_handle);
    pc_reload_sample(&input.reload,roles,active,GetTickCount64(),reload,fire,secondary);
    ReleaseSRWLockExclusive(&lock);
}

int pc_controls_accept_pad(void)
{
    return !replaying && !InterlockedCompareExchange(&editing,0,0) &&
           window_handle && GetForegroundWindow()==window_handle;
}

int pc_controls_binding_keys(unsigned char keys[256])
{
    AcquireSRWLockExclusive(&lock);
    int focused=input.focused && (replaying || GetForegroundWindow()==window_handle);
    memset(keys,0,256);
    if(focused) {
        for(unsigned i=1;i<256;i++)keys[i]=(unsigned char)pc_input_down(&input,i,GetTickCount64());
        if(menu_mode && GetTickCount64()-menu_wheel_at<250) {
            keys[0x86]|=menu_wheel>0;keys[0x87]|=menu_wheel<0;
        }
    }
    ReleaseSRWLockExclusive(&lock);return focused;
}

int pc_controls_key_down(int key)
{
    int down;
    if (InterlockedCompareExchange(&editing, 0, 0)) return 0;
    AcquireSRWLockExclusive(&lock);
    replay_tick();
    down = pc_input_down(&input, (unsigned)key, GetTickCount64());
    /* Custom menus may read D-pad edges instead of the WASD analogue stick. */
    if(menu_mode) {
        unsigned alias=key==VK_LEFT?'A':key==VK_RIGHT?'D':
                       key==VK_UP?'W':key==VK_DOWN?'S':0;
        if(alias) down|=pc_input_down(&input,alias,GetTickCount64());
    }
    if(text_input.owner && pc_text_key((unsigned)key))down=0;
    down=down && (replaying || GetForegroundWindow()==window_handle);
    if(!input.keys[VK_LBUTTON]) menu_button_block &= ~1;
    if(!input.keys[VK_RBUTTON]) menu_button_block &= ~2;
    if((key==VK_LBUTTON && (menu_mode || (menu_button_block&1))) ||
       (key==VK_RBUTTON && (menu_mode || (menu_button_block&2)))) down=0;
    if (key == VK_LBUTTON && pc_input_terminal_suppresses_left(&input, input.keys[VK_LBUTTON])) down = 0;
    ReleaseSRWLockExclusive(&lock);
    return down;
}

void pc_controls_menu_mode(int active)
{
    int changed;
    AcquireSRWLockExclusive(&lock);
    changed=menu_mode!=!!active;
    if(changed) {
        menu_button_block=(input.keys[VK_LBUTTON]?1:0)|(input.keys[VK_RBUTTON]?2:0);
        menu_mode=!!active;
        pc_reload_reset(&input.reload);
        menu_wheel=0; wheel_remainder=0;
        input.dx=input.dy=0;
    }
    ReleaseSRWLockExclusive(&lock);
    if(changed && window_handle) PostMessage(window_handle,PC_MENU_MOUSE_MESSAGE,!active,0);
}

static PcMenuPointer menu_pointer(int consume)
{
    PcMenuPointer p={0};
    POINT point={0}; RECT client;
    AcquireSRWLockExclusive(&lock);
    if(consume) replay_tick();
    if(menu_mode && !InterlockedCompareExchange(&editing,0,0) &&
       (replaying || GetForegroundWindow()==window_handle) &&
       GetClientRect(window_handle,&client)) {
        p.width=(float)client.right; p.height=(float)client.bottom;
        if(replaying) { point.x=test_cursor_x; point.y=test_cursor_y; p.valid=point.x>=0 && point.y>=0; }
        else p.valid=GetCursorPos(&point) && ScreenToClient(window_handle,&point);
        p.x=(float)point.x; p.y=(float)point.y;
        p.valid=p.valid && p.x>=0 && p.y>=0 && p.x<p.width && p.y<p.height;
        p.left=input.keys[VK_LBUTTON]; p.right=input.keys[VK_RBUTTON];
        if(consume && p.valid && GetTickCount64()-menu_wheel_at<250) p.wheel=menu_wheel;
    }
    if(consume) menu_wheel=0;
    ReleaseSRWLockExclusive(&lock);
    if(pc_settings_capture_blocked())p.valid=p.left=p.right=p.wheel=0;
    return p;
}

PcMenuPointer pc_controls_menu_pointer(void) { return menu_pointer(1); }
/* Drawing must not consume wheel packets or advance the input replay. */
PcMenuPointer pc_controls_menu_hover_pointer(void) { return menu_pointer(0); }

int pc_controls_terminal_click(void)
{
    int down;
    if (InterlockedCompareExchange(&editing, 0, 0)) return 0;
    AcquireSRWLockExclusive(&lock);
    replay_tick();
    down = (replaying || GetForegroundWindow() == window_handle) &&
           pc_input_terminal_click(&input, input.keys[VK_LBUTTON]);
    ReleaseSRWLockExclusive(&lock);
    return down;
}

void pc_controls_apply_mouse(uint32_t player, uint32_t fov_object, ptrdiff_t memory_offset)
{
    float *yaw, *pitch, fov, old_yaw, old_pitch;
    int changed, terminal;
    if (InterlockedCompareExchange(&editing, 0, 0)) return;
    AcquireSRWLockExclusive(&lock);
    replay_tick();
    if (!(replaying || GetForegroundWindow() == window_handle)) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }
    /* No guest callbacks run before this local mapping snapshot expires. */
    PcGuestRegion region=pc_camera_mouse_region(player,fov_object,memory_offset);
    pc_reload_guest_context(&input.reload,player,memory_offset,GetTickCount64(),&region);
    terminal = pc_terminal_mouse_in_region(&input, GetTickCount64(), sensitivity, player, memory_offset, pc_native_rate()!=0,&region);
    pc_input_terminal_context(&input, terminal >= 0, input.keys[VK_LBUTTON]);
    if (terminal >= 0 || pc_mounted_mouse_in_region(&input, GetTickCount64(), sensitivity, invert_y,
            player, fov_object, memory_offset, pc_native_rate()!=0,&region) >= 0 || !pc_camera_fields_in_region(player,fov_object,memory_offset,&yaw,&pitch,&fov,&region)) {
        ReleaseSRWLockExclusive(&lock);
        return;
    }
    old_yaw = *yaw; old_pitch = *pitch;
    changed = pc_input_look(&input, GetTickCount64(), sensitivity, invert_y,
                            fov < 55 ? fov / 55 : 1, yaw, pitch);
    ReleaseSRWLockExclusive(&lock);
    if (changed && applied++ < 12)
        fprintf(stderr, "[PC-MOUSE] yaw %.3f -> %.3f, pitch %.3f -> %.3f, fov %.3f\n",
                old_yaw, *yaw, old_pitch, *pitch, fov);
}
