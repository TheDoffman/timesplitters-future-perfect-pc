#ifndef TSFP_PC_MOUSE_CAPTURE_H
#define TSFP_PC_MOUSE_CAPTURE_H
#include <windows.h>

/* Window-thread adapter; gameplay aim uses relative WM_INPUT counts. */
static int pc_mouse_clip_rect(HWND window, RECT *rect)
{
    POINT origin = {0, 0};
    if (!window || !GetClientRect(window, rect) ||
        !ClientToScreen(window, &origin) ||
        rect->right <= 0 || rect->bottom <= 0) return 0;
    /* Full-client clipping lets the hidden absolute pointer accumulate at
       the edges. Pin it inside the client instead; relative raw counts keep
       turning without ever routing clicks to a border or another window.
       Keep a nonempty tiny rectangle; DPI virtualization can round its
       physical coordinates onto adjacent logical pixels. */
    rect->left = origin.x + rect->right / 2;
    rect->top = origin.y + rect->bottom / 2;
    rect->right = rect->left + 1;
    rect->bottom = rect->top + 1;
    return 1;
}

static void clip_to_window(HWND window)
{
    RECT rect;
    if (GetForegroundWindow() == window && pc_mouse_clip_rect(window, &rect))
        ClipCursor(&rect);
}

static void restore_window_clip(HWND window)
{
    RECT expected, actual;
    if (GetForegroundWindow() != window || !pc_mouse_clip_rect(window, &expected)) return;
    if (!GetClipCursor(&actual) || !EqualRect(&expected, &actual))
        ClipCursor(&expected);
}
#endif
