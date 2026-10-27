/*
 * launcher.c -- the window a player opens before the game.
 *
 * Its whole job: show the handful of settings that are worth choosing,
 * write them to the file the runtime reads, start the game and get out
 * of the way. Everything else the runtime can be told stays where it
 * was, in environment variables, for people who want it.
 *
 * It can be driven entirely with a pad. That is not a flourish: the
 * front ends this ships inside -- a living-room build, GameNative and
 * the like -- may never see a keyboard, and a launcher that needs a
 * mouse to get past is a launcher that stops the game starting. D-pad
 * or stick to move, A to play, B to quit, shoulders to change tab.
 *
 * The drawing is all in launcher_theme.c, so this file is about what the
 * settings are and how they are moved between, not about pixels.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "launcher_theme.h"
#include "launcher_bindings.h"
#include "recomp_config.h"
#include "recomp_pad.h"

#ifndef LAUNCHER_GAME_EXE
#define LAUNCHER_GAME_EXE "game.exe"
#endif

#define WINDOW_W 900
#define WINDOW_H 862                    /* nine Video rows, their help, the footer */

/* The client area as it really is. On a display at 125% these are not
 * WINDOW_W/H: asking Windows not to scale us and then laying out
 * against the true size is the difference between a crisp launcher and
 * one stretched up from a smaller drawing. */
static int g_cw = WINDOW_W, g_ch = WINDOW_H;

/* ------------------------------------------------------------- settings */

typedef enum { ROW_INT, ROW_BOOL, ROW_CHOICE, ROW_TEXT } RowKind;

typedef struct Row {
    const char *label;
    const char *help;
    RowKind     kind;
    int         lo, hi;                 /* ROW_INT */
    const char *const *choices;         /* ROW_CHOICE, NULL terminated */
    int        *ival;                   /* ROW_INT / ROW_BOOL / ROW_CHOICE index */
    char       *text;                   /* ROW_TEXT */
    size_t      text_len;
} Row;

static const char *const k_frame_caps[] = { "adaptive", "60", "30", "0", NULL };

static RecompSettings g_settings;
static int            g_frame_cap_index;   /* into k_frame_caps */
static int            g_wide_camera;       /* hor_plus as a switch, see below */

static Row  g_video_rows[12];
static int  g_video_count;

/* ---- input ---- */

static BindConfig g_bind;
static int        g_bind_port;          /* 0..3, the controller being edited */
static int        g_bind_top;           /* first control shown: the list scrolls */
static int        g_bind_col;           /* 0 pad, 1 keyboard */
static int        g_capturing;          /* waiting for a press to bind */
static int        g_capture_armed;      /* everything released since we started */
static int        g_bind_dirty;

/* Which row of whichever list is showing. Declared here rather than with
 * the rest of the window state because the capture below reads it. */
static int        g_sel;

#define BIND_ROWS_VISIBLE 5

/* The input list starts with three rows that are not controls: which
 * controller is being edited, what it reads from, and putting it back.
 * They live in the list rather than off to one side so they are reached
 * the same way as everything else -- one way to move, one way to
 * change, and no second idea to learn. */
#define BIND_HEAD_ROWS 4
#define BIND_ROW_PORT   0
#define BIND_ROW_DEVICE 1
#define BIND_ROW_API    2       /* SDL3 or XInput, for every controller */
#define BIND_ROW_RESET  3
#define BIND_TOTAL_ROWS (BIND_HEAD_ROWS + BIND_CONTROLS)

static const char *frame_cap_label(int i)
{
    switch (i) {
    case 0: return "Adaptive (as the console)";
    case 1: return "60 frames a second";
    case 2: return "30 frames a second";
    default: return "Uncapped";
    }
}

static void build_rows(void)
{
    Row *r = g_video_rows;

    r->label = "Resolution";
    r->help  = "Render larger than the console, then filter back down. Costs little.";
    r->kind  = ROW_INT; r->lo = 1; r->hi = 8; r->ival = &g_settings.resolution_scale;
    r++;

#ifndef LAUNCHER_NO_WIDESCREEN
    /* Hidden for a title built with recomp_add_launcher(... NO_WIDESCREEN),
     * where a wider picture cannot be right; settings_save writes both off. */
    r->label = "Widescreen";
    r->help  = "Present at 16:9. This game has no widescreen mode of its own, so"
               " turn on Wide camera with it or the picture will be stretched.";
    r->kind  = ROW_BOOL; r->ival = &g_settings.widescreen;
    r++;

    r->label = "Wide camera";
    r->help  = "See more to the sides rather than the same view stretched.";
    r->kind  = ROW_BOOL; r->ival = &g_wide_camera;
    r++;
#endif

    r->label = "Texture sharpness";
    r->help  = "Sharpen textures seen at a glancing angle: floors, walls, roads.";
    r->kind  = ROW_INT; r->lo = 1; r->hi = 16; r->ival = &g_settings.anisotropy;
    r++;

    r->label = "Frame rate";
    r->help  = "Adaptive paces the game the way the console did. F10 changes it"
               " while playing.";
    r->kind  = ROW_CHOICE; r->choices = k_frame_caps; r->ival = &g_frame_cap_index;
    r++;

    r->label = "Show frame rate";
    r->help  = "Start with the counter on screen. F9 toggles it while playing.";
    r->kind  = ROW_BOOL; r->ival = &g_settings.fps_overlay;
    r++;

    r->label = "Fullscreen";
    r->help  = "Fill the screen, without a window frame. Alt+Enter switches in game.";
    r->kind  = ROW_BOOL; r->ival = &g_settings.fullscreen;
    r++;

    r->label = "Variable refresh rate";
    r->help  = "In fullscreen on a G-SYNC or FreeSync display: an even 60, no judder.";
    r->kind  = ROW_BOOL; r->ival = &g_settings.vrr;
    r++;

    r->label = "Frame interpolation";
    r->help  = "Draw frames between the game's own: smoother motion while the game"
               " still runs at 60. Best with Variable refresh rate.";
    r->kind  = ROW_INT; r->lo = 1; r->hi = 4; r->ival = &g_settings.frame_interp;
    r++;

    g_video_count = (int)(r - g_video_rows);
}

static void row_value(const Row *r, char *out, size_t n)
{
    switch (r->kind) {
    case ROW_BOOL:
        snprintf(out, n, "%s", *r->ival ? "On" : "Off");
        break;
    case ROW_INT:
        if (r->ival == &g_settings.resolution_scale)
            snprintf(out, n, *r->ival == 1 ? "Console (640x480)" : "%dx (%dx%d)",
                     *r->ival, 640 * *r->ival, 480 * *r->ival);
        else if (*r->ival == 1)
            snprintf(out, n, "Off");
        else if (r->ival == &g_settings.frame_interp)
            snprintf(out, n, "%dx (%d frames a second)", *r->ival, 60 * *r->ival);
        else
            snprintf(out, n, "%dx", *r->ival);
        break;
    case ROW_CHOICE:
        snprintf(out, n, "%s", frame_cap_label(*r->ival));
        break;
    default:
        snprintf(out, n, "%s", r->text && r->text[0] ? r->text : "Beside the game");
        break;
    }
}

static void row_move(Row *r, int delta)
{
    switch (r->kind) {
    case ROW_BOOL:
        *r->ival = !*r->ival;
        break;
    case ROW_INT:
        *r->ival += delta;
        if (*r->ival < r->lo) *r->ival = r->lo;
        if (*r->ival > r->hi) *r->ival = r->hi;
        break;
    case ROW_CHOICE: {
        int n = 0;
        while (r->choices[n]) n++;
        *r->ival = (*r->ival + delta + n) % n;
        break;
    }
    default:
        break;
    }
}

static int row_can(const Row *r, int delta)
{
    switch (r->kind) {
    case ROW_INT:
        return delta < 0 ? *r->ival > r->lo : *r->ival < r->hi;
    default:
        return 1;
    }
}

/* ------------------------------------------------------------- the title */

static char g_title_name[128] = "Xbox game";
static uint32_t g_title_id;
static char g_xbe_found[MAX_PATH];      /* empty: the game's files were not found */

/* The name and id out of the game's own XBE, so the launcher says what it
 * launches without being told at build time -- and, more to the point, so
 * it writes the settings file the game will read, which is named after the
 * title id. A launcher that cannot find the XBE writes default.conf, which
 * the game never reads, so every setting chosen here would quietly do
 * nothing.
 *
 * Looked for exactly where the game's find_game() looks, in its order:
 * RECOMP_GAME_DIR alone if it is set, else a "game" folder beside the
 * executable, else the title's YOUR_GAME_DIR (LAUNCHER_GAME_DIR, read out
 * of its main.c by recomp_add_launcher) -- all relative to this executable,
 * not the working directory, as the game's are. */
static int read_cert(const char *xbe)
{
    FILE *f = fopen(xbe, "rb");
    unsigned char head[0x200], cert[0xA4];
    uint32_t cert_va, base_va, off;

    if (!f)
        return 0;
    if (fread(head, 1, sizeof head, f) != sizeof head) { fclose(f); return 0; }
    base_va = *(uint32_t *)(head + 0x0104);
    cert_va = *(uint32_t *)(head + 0x0118);
    if (cert_va < base_va) { fclose(f); return 0; }
    off = cert_va - base_va;
    if (fseek(f, (long)off, SEEK_SET) != 0 ||
        fread(cert, 1, sizeof cert, f) != sizeof cert) { fclose(f); return 0; }
    fclose(f);

    g_title_id = *(uint32_t *)(cert + 0x08);
    {   /* wszTitleName: 40 UTF-16 units at +0x0C, padded not terminated */
        int j, k = 0;
        for (j = 0; j < 40 && k < (int)sizeof g_title_name - 1; j++) {
            unsigned c = cert[0x0C + j * 2] | (cert[0x0C + j * 2 + 1] << 8);
            if (!c) break;
            if (c < 0x80) g_title_name[k++] = (char)c;
        }
        while (k > 0 && g_title_name[k - 1] == ' ') k--;
        g_title_name[k] = '\0';
        if (!k) snprintf(g_title_name, sizeof g_title_name, "Xbox game");
    }
    if (!GetFullPathNameA(xbe, sizeof g_xbe_found, g_xbe_found, NULL))
        snprintf(g_xbe_found, sizeof g_xbe_found, "%s", xbe);
    return 1;
}

static void read_title(void)
{
    const char *env = getenv("RECOMP_GAME_DIR");
    char dir[MAX_PATH], xbe[MAX_PATH], *slash;

    /* Set means only this, as it does for the game: finding the XBE
     * somewhere else would name a settings file for a different copy. */
    if (env && *env) {
        snprintf(xbe, sizeof xbe, "%s\\default.xbe", env);
        read_cert(xbe);
        return;
    }

    if (!GetModuleFileNameA(NULL, dir, sizeof dir))
        return;
    slash = strrchr(dir, '\\');
    if (slash) *slash = '\0';

    snprintf(xbe, sizeof xbe, "%s\\game\\default.xbe", dir);
    if (read_cert(xbe))
        return;
#ifdef LAUNCHER_GAME_DIR
    snprintf(xbe, sizeof xbe, "%s\\%s\\default.xbe", dir, LAUNCHER_GAME_DIR);
    read_cert(xbe);
#endif
}

/* ------------------------------------------------------------- capture */

/* What is pressed right now, as a source string, or NULL. Reads the pad
 * and the keyboard together: which one a person reaches for is the
 * answer to "what do you want this to be", and asking them to say first
 * is a question with no purpose.
 *
 * Sticks count as four directions rather than two axes, because that is
 * what the file binds and what a person means when they push one. */
static const char *capture_pad(int pad_index)
{
    static char out[BIND_SOURCE_LEN];
    RecompPadState st;
    static const struct { WORD mask; const char *name; } k_buttons[] = {
        { RECOMP_PAD_A, "a" }, { RECOMP_PAD_B, "b" },
        { RECOMP_PAD_X, "x" }, { RECOMP_PAD_Y, "y" },
        { RECOMP_PAD_LEFT_SHOULDER, "lshoulder" },
        { RECOMP_PAD_RIGHT_SHOULDER, "rshoulder" },
        { RECOMP_PAD_START, "start" }, { RECOMP_PAD_BACK, "back" },
        { RECOMP_PAD_LEFT_THUMB, "lthumb" },
        { RECOMP_PAD_RIGHT_THUMB, "rthumb" },
        { RECOMP_PAD_DPAD_UP, "dpad_up" },
        { RECOMP_PAD_DPAD_DOWN, "dpad_down" },
        { RECOMP_PAD_DPAD_LEFT, "dpad_left" },
        { RECOMP_PAD_DPAD_RIGHT, "dpad_right" },
        { 0, NULL }
    };
    const SHORT push = 22000;             /* well past any resting stick */
    int i;

    if (!recomp_pad_read(pad_index, &st))
        return NULL;

    for (i = 0; k_buttons[i].name; i++)
        if (st.buttons & k_buttons[i].mask) {
            snprintf(out, sizeof out, "pad:%s", k_buttons[i].name);
            return out;
        }
    if (st.left_trigger > 160)  { snprintf(out, sizeof out, "pad:lt"); return out; }
    if (st.right_trigger > 160) { snprintf(out, sizeof out, "pad:rt"); return out; }

    if (st.lx >  push) { snprintf(out, sizeof out, "pad:lx+"); return out; }
    if (st.lx < -push) { snprintf(out, sizeof out, "pad:lx-"); return out; }
    if (st.ly >  push) { snprintf(out, sizeof out, "pad:ly+"); return out; }
    if (st.ly < -push) { snprintf(out, sizeof out, "pad:ly-"); return out; }
    if (st.rx >  push) { snprintf(out, sizeof out, "pad:rx+"); return out; }
    if (st.rx < -push) { snprintf(out, sizeof out, "pad:rx-"); return out; }
    if (st.ry >  push) { snprintf(out, sizeof out, "pad:ry+"); return out; }
    if (st.ry < -push) { snprintf(out, sizeof out, "pad:ry-"); return out; }
    return NULL;
}

/* The key names the runtime knows by name; everything else is a letter
 * or a digit and spells itself. Escape is not offered: it is how a
 * person gets out of the capture. */
static const struct { int vk; const char *name; } k_named_keys[] = {
    { VK_RETURN, "RETURN" }, { VK_BACK, "BACK" }, { VK_SPACE, "SPACE" },
    { VK_TAB, "TAB" }, { VK_UP, "UP" }, { VK_DOWN, "DOWN" },
    { VK_LEFT, "LEFT" }, { VK_RIGHT, "RIGHT" },
    { VK_LSHIFT, "LSHIFT" }, { VK_RSHIFT, "RSHIFT" },
    { VK_LCONTROL, "LCTRL" }, { VK_RCONTROL, "RCTRL" },
    { 0, NULL }
};

static const char *capture_key(void)
{
    static char out[BIND_SOURCE_LEN];
    int i, vk;

    for (i = 0; k_named_keys[i].name; i++)
        if (GetAsyncKeyState(k_named_keys[i].vk) & 0x8000) {
            snprintf(out, sizeof out, "key:%s", k_named_keys[i].name);
            return out;
        }
    for (vk = 'A'; vk <= 'Z'; vk++)
        if (GetAsyncKeyState(vk) & 0x8000) {
            snprintf(out, sizeof out, "key:%c", (char)vk);
            return out;
        }
    for (vk = '0'; vk <= '9'; vk++)
        if (GetAsyncKeyState(vk) & 0x8000) {
            snprintf(out, sizeof out, "key:%c", (char)vk);
            return out;
        }
    return NULL;
}

/* Nothing at all held. Entering the capture with A still down would
 * otherwise bind A to whatever the person was trying to rebind. */
static int everything_released(int pad_index)
{
    return capture_pad(pad_index) == NULL && capture_key() == NULL &&
           !(GetAsyncKeyState(VK_ESCAPE) & 0x8000);
}

static void capture_tick(void)
{
    const BindPort *bp = &g_bind.port[g_bind_port];
    int pad = bp->device == BIND_DEV_PAD ? bp->pad : 0;
    const char *got;

    if (!g_capture_armed) {
        g_capture_armed = everything_released(pad);
        return;
    }
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        g_capturing = 0;
        return;
    }
    got = (g_bind_col == 0) ? capture_pad(pad) : capture_key();
    if (!got)
        return;

    {
        int k = g_bind_top + g_sel - BIND_HEAD_ROWS;

        if (k >= 0 && k < BIND_CONTROLS) {
            char *dst = (g_bind_col == 0) ? g_bind.port[g_bind_port].pad_src[k]
                                          : g_bind.port[g_bind_port].key_src[k];
            snprintf(dst, BIND_SOURCE_LEN, "%s", got);
            g_bind_dirty = 1;
        }
    }
    g_capturing = 0;
}

/* ------------------------------------------------------------ persistence */

static void settings_load(void)
{
    char path[1024];

    recomp_settings_defaults(&g_settings);
    if (recomp_settings_path(g_title_id, path, sizeof path))
        recomp_settings_read(path, &g_settings);

    g_wide_camera = g_settings.hor_plus > 0.01;
    g_frame_cap_index = 0;
    {
        int i;
        for (i = 0; k_frame_caps[i]; i++)
            if (_stricmp(g_settings.frame_cap, k_frame_caps[i]) == 0) {
                g_frame_cap_index = i;
                break;
            }
    }
}

static void bindings_load(void)
{
    char path[1024];

    bind_defaults(&g_bind);
    if (bind_config_path(path, sizeof path))
        bind_load(path, &g_bind);
    recomp_pad_set_api(g_bind.pad_api == BIND_PAD_API_XINPUT
                       ? RECOMP_PAD_API_XINPUT : RECOMP_PAD_API_SDL);
    g_bind_dirty = 0;
}

static void bindings_save(void)
{
    char path[1024];

    if (g_bind_dirty && bind_config_path(path, sizeof path))
        if (bind_save(path, &g_bind))
            g_bind_dirty = 0;
}

static int settings_save(void)
{
    char path[1024];

    /* The camera widening is one switch here and two numbers in the file:
     * a player should not have to know that 0.75 is (4/3)/(16/9), and the
     * register is a per-game constant they have no way to choose. */
    g_settings.hor_plus = g_wide_camera ? 0.75 : 0.0;
#ifdef LAUNCHER_NO_WIDESCREEN
    /* Not offered, so not left on by an older file either. */
    g_settings.widescreen = 0;
    g_settings.hor_plus = 0.0;
#endif
    snprintf(g_settings.frame_cap, sizeof g_settings.frame_cap, "%s",
             k_frame_caps[g_frame_cap_index]);

    if (!recomp_settings_path(g_title_id, path, sizeof path))
        return 0;
    return recomp_settings_write(path, &g_settings);
}

/* ---------------------------------------------------------------- launch */

static int launch_game(void)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char exe[MAX_PATH], dir[MAX_PATH], *slash;

    /* Beside this executable, whatever the working directory is: a
     * shortcut on a desktop must work as well as a double-click. */
    GetModuleFileNameA(NULL, dir, sizeof dir);
    slash = strrchr(dir, '\\');
    if (slash) *slash = '\0';
    snprintf(exe, sizeof exe, "%s\\%s", dir, LAUNCHER_GAME_EXE);

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);

    if (!CreateProcessA(exe, NULL, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        char msg[MAX_PATH + 160];

        snprintf(msg, sizeof msg,
                 "The game could not be started.\n\n%s\n\n"
                 "The launcher expects it beside itself.", exe);
        MessageBoxA(NULL, msg, "Cannot start the game", MB_OK | MB_ICONERROR);
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}

/* ------------------------------------------------------------------- pad */

/* A pad held in a direction should repeat, but not instantly and not at
 * the polling rate, or one nudge runs the value to its end. */
typedef struct PadNav {
    WORD  last;
    DWORD held_since, last_repeat;
    int   last_dir_x, last_dir_y;
} PadNav;

static PadNav g_pad;

#define PAD_REPEAT_DELAY 380
#define PAD_REPEAT_RATE  110

static int pad_axis(SHORT v)
{
    const SHORT dead = 16000;
    return v > dead ? 1 : (v < -dead ? -1 : 0);
}

/* Fills dx/dy/accept/cancel/tab with edges: each is non-zero for one
 * frame per press, or repeatedly while a direction is held. */
static int pad_poll(int *dx, int *dy, int *accept, int *cancel, int *tab)
{
    RecompPadState st;
    DWORD now = GetTickCount();
    WORD b;
    int x, y, i, connected = 0;

    *dx = *dy = *accept = *cancel = *tab = 0;

    memset(&st, 0, sizeof st);
    for (i = 0; i < RECOMP_PAD_SLOTS; i++)
        if (recomp_pad_read(i, &st)) { connected = 1; break; }
    if (!connected) {
        g_pad.last = 0;
        return 0;
    }

    b = st.buttons;
    x = pad_axis(st.lx);
    y = -pad_axis(st.ly);                          /* screen y grows downward */
    if (b & RECOMP_PAD_DPAD_LEFT)  x = -1;
    if (b & RECOMP_PAD_DPAD_RIGHT) x = 1;
    if (b & RECOMP_PAD_DPAD_UP)    y = -1;
    if (b & RECOMP_PAD_DPAD_DOWN)  y = 1;

    if (x != g_pad.last_dir_x || y != g_pad.last_dir_y) {
        g_pad.last_dir_x = x;
        g_pad.last_dir_y = y;
        g_pad.held_since = now;
        g_pad.last_repeat = now;
        *dx = x;
        *dy = y;
    } else if ((x || y) && now - g_pad.held_since > PAD_REPEAT_DELAY &&
               now - g_pad.last_repeat > PAD_REPEAT_RATE) {
        g_pad.last_repeat = now;
        *dx = x;
        *dy = y;
    }

    /* Buttons on the press, not the release. */
    if ((b & RECOMP_PAD_A) && !(g_pad.last & RECOMP_PAD_A)) *accept = 1;
    if ((b & RECOMP_PAD_B) && !(g_pad.last & RECOMP_PAD_B)) *cancel = 1;
    if ((b & RECOMP_PAD_RIGHT_SHOULDER) &&
        !(g_pad.last & RECOMP_PAD_RIGHT_SHOULDER)) *tab = 1;
    if ((b & RECOMP_PAD_LEFT_SHOULDER) &&
        !(g_pad.last & RECOMP_PAD_LEFT_SHOULDER)) *tab = -1;
    if ((b & RECOMP_PAD_START) && !(g_pad.last & RECOMP_PAD_START)) *accept = 1;

    g_pad.last = b;
    return 1;
}

/* ----------------------------------------------------------------- state */

typedef enum { TAB_VIDEO, TAB_INPUT, TAB_ABOUT, TAB_COUNT } Tab;
static const char *const k_tab_names[TAB_COUNT] = { "VIDEO", "INPUT", "ABOUT" };

static Tab    g_tab;
static Tab    g_tab_from;
static double g_tab_t = 1.0;
static int    g_rows_on_tab;
static double g_pulse;
static int    g_pad_seen;
static HWND   g_hwnd;

static ThemeRect tab_rect(int i)
{
    ThemeRect r;
    r.w = 130;
    r.h = 40;
    r.x = 60 + i * (r.w + 14);
    r.y = 92;
    return r;
}

static ThemeRect row_rect(int i)
{
    ThemeRect r;
    r.x = 60;
    r.y = 168 + i * 62;
    r.w = g_cw - 120;
    r.h = 52;
    return r;
}

/* Where a row's arrows are drawn: theme_arrows puts the left one at this
 * box's left edge and the right one at its right edge. Drawing and clicking
 * both come from here -- the click test once kept its own numbers, which
 * put the drawn left arrow inside its "right" zone, so clicking it moved a
 * setting the wrong way. */
static ThemeRect arrows_rect(ThemeRect row)
{
    ThemeRect a = row;

    a.x = row.x + row.w - 46;
    a.w = 30;
    return a;
}

/* Which arrow a click at mx on this row means: -1 the left, +1 the right,
 * 0 neither. Each target is half the arrows' box, stretched outward -- the
 * left back to where the value column ends, the right out to the row's
 * edge -- so a 7-pixel triangle is not all there is to hit. */
static int hit_arrow(ThemeRect row, int mx)
{
    ThemeRect a = arrows_rect(row);
    int mid = a.x + a.w / 2;

    if (mx >= row.x + row.w - 60 && mx < mid)
        return -1;
    if (mx >= mid && mx < row.x + row.w)
        return 1;
    return 0;
}

static void set_tab(Tab t)
{
    if (t == g_tab)
        return;
    g_tab_from = g_tab;
    g_tab = t;
    g_tab_t = 0.0;
    g_sel = 0;
}

static void rows_for_tab(void)
{
    g_rows_on_tab = (g_tab == TAB_VIDEO) ? g_video_count
                  : (g_tab == TAB_INPUT) ? BIND_ROWS_VISIBLE : 0;
    if (g_sel >= g_rows_on_tab)
        g_sel = g_rows_on_tab ? g_rows_on_tab - 1 : 0;
}

static void do_play(void)
{
    settings_save();
    bindings_save();
    if (launch_game())
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
}

/* The input list is longer than the screen, so moving off either end
 * scrolls it rather than wrapping: a list that wraps from "A" to "right
 * stick press" is a list nobody can walk down. */
static void input_move(int dy)
{
    int k = g_bind_top + g_sel + dy;

    if (k < 0) k = 0;
    if (k >= BIND_TOTAL_ROWS) k = BIND_TOTAL_ROWS - 1;
    if (k < g_bind_top)
        g_bind_top = k;
    else if (k >= g_bind_top + BIND_ROWS_VISIBLE)
        g_bind_top = k - BIND_ROWS_VISIBLE + 1;
    g_sel = k - g_bind_top;
}

static void nav(int dx, int dy, int accept, int cancel, int tabdelta)
{
    if (g_capturing) {
        /* While waiting for a press, nothing else means anything: the
         * press is the answer. Cancel is handled in capture_tick, which
         * watches Escape and the pad together. */
        if (cancel)
            g_capturing = 0;
        return;
    }
    if (tabdelta)
        set_tab((Tab)((g_tab + tabdelta + TAB_COUNT) % TAB_COUNT));

    if (g_tab == TAB_INPUT) {
        int k = g_bind_top + g_sel;

        if (dy) {
            input_move(dy);
            return;
        }
        if (k == BIND_ROW_PORT) {
            if (dx) {
                g_bind_port = (g_bind_port + dx + BIND_PORTS) % BIND_PORTS;
                g_bind_col = 0;
            }
        } else if (k == BIND_ROW_DEVICE) {
            if (dx) {
                /* One list, in the order a person would try them:
                 * the four pads, then the keyboard, then nothing. */
                BindPort *bp = &g_bind.port[g_bind_port];
                int cur = (bp->device == BIND_DEV_PAD) ? bp->pad
                        : (bp->device == BIND_DEV_KEYBOARD) ? BIND_PORTS
                        : BIND_PORTS + 1;

                cur = (cur + dx + BIND_PORTS + 2) % (BIND_PORTS + 2);
                if (cur < BIND_PORTS) {
                    bp->device = BIND_DEV_PAD;
                    bp->pad = cur;
                } else if (cur == BIND_PORTS) {
                    bp->device = BIND_DEV_KEYBOARD;
                    bp->pad = -1;
                } else {
                    bp->device = BIND_DEV_NONE;
                    bp->pad = -1;
                }
                g_bind_dirty = 1;
            }
        } else if (k == BIND_ROW_API) {
            if (dx || accept) {
                g_bind.pad_api = g_bind.pad_api == BIND_PAD_API_XINPUT
                                 ? BIND_PAD_API_SDL : BIND_PAD_API_XINPUT;
                recomp_pad_set_api(g_bind.pad_api == BIND_PAD_API_XINPUT
                                   ? RECOMP_PAD_API_XINPUT : RECOMP_PAD_API_SDL);
                g_bind_dirty = 1;
            }
        } else if (k == BIND_ROW_RESET) {
            if (accept) {
                BindConfig d;

                /* This controller only: a person resetting controller 2
                 * has said nothing about the other three. */
                bind_defaults(&d);
                g_bind.port[g_bind_port] = d.port[g_bind_port];
                g_bind_dirty = 1;
            }
        } else {
            if (dx)
                g_bind_col = g_bind_col ? 0 : 1;
            if (accept) {
                g_capturing = 1;
                g_capture_armed = 0;    /* wait for the accept to be let go */
            }
        }
        if (cancel)
            PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        return;
    }

    if (dy && g_rows_on_tab)
        g_sel = (g_sel + dy + g_rows_on_tab) % g_rows_on_tab;
    if (dx && g_tab == TAB_VIDEO && g_rows_on_tab)
        row_move(&g_video_rows[g_sel], dx);
    if (accept)
        do_play();
    if (cancel)
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
}

/* ------------------------------------------------------------------ draw */

static void draw(void)
{
    ThemeRect r;
    char value[128];
    int i;

    theme_background(g_pulse);

    /* Title. The game's own name, from its certificate. */
    r.x = 60; r.y = 30; r.w = g_cw - 120; r.h = 46;
    theme_text(r, g_title_name, 26, 700, THEME_TEXT, THEME_LEFT);

    for (i = 0; i < TAB_COUNT; i++) {
        ThemeRect t = tab_rect(i);

        theme_text(t, k_tab_names[i], 12, 700,
                   i == g_tab ? THEME_GREEN : THEME_TEXT_DIM, THEME_CENTRE);
    }
    {
        ThemeRect a = tab_rect(g_tab_from), b = tab_rect(g_tab);

        a.y += 34; b.y += 34;
        theme_tab_underline(a, b, g_tab_t);
    }

    if (g_tab == TAB_VIDEO) {
        for (i = 0; i < g_video_count; i++) {
            Row *row = &g_video_rows[i];
            ThemeRect rr = row_rect(i), lr, vr, ar;
            double lit = (i == g_sel) ? 1.0 : 0.0;

            theme_panel(rr, lit);

            lr = rr; lr.x += 22; lr.w = 260;
            theme_text(lr, row->label, 13, 600,
                       lit > 0.5 ? THEME_TEXT : THEME_TEXT_DIM, THEME_LEFT);

            row_value(row, value, sizeof value);
            vr = rr; vr.x += 300; vr.w = rr.w - 360;
            theme_text(vr, value, 13, 400,
                       lit > 0.5 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);

            ar = arrows_rect(rr);
            theme_arrows(ar, row_can(row, -1), row_can(row, 1), lit);
        }

        /* The selected row's explanation, under the list. */
        r.x = 62; r.y = 168 + g_video_count * 62 + 10;
        r.w = g_cw - 124; r.h = 46;
        if (g_sel < g_video_count)
            theme_text(r, g_video_rows[g_sel].help, 11, 400, THEME_TEXT_DIM, THEME_LEFT);
    } else if (g_tab == TAB_INPUT) {
        BindPort *bp = &g_bind.port[g_bind_port];
        char line[160];
        int i;

        /* Column headings, so the two sides are not a guess. */
        {
            ThemeRect h2 = row_rect(0);

            h2.y -= 24; h2.h = 20;
            h2.x += 300; h2.w = 200;
            theme_text(h2, "CONTROLLER", 10, 700,
                       g_bind_col == 0 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);
            h2.x += 210;
            theme_text(h2, "KEYBOARD", 10, 700,
                       g_bind_col == 1 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);
        }

        for (i = 0; i < BIND_ROWS_VISIBLE; i++) {
            int k = g_bind_top + i;
            ThemeRect rr, lr, cv;
            double lit;
            const char *label = NULL;

            if (k >= BIND_TOTAL_ROWS)
                break;
            rr = row_rect(i);
            rr.h = 46;
            lit = (i == g_sel) ? 1.0 : 0.0;
            theme_panel(rr, lit);

            lr = rr; lr.x += 22; lr.w = 270;
            cv = rr; cv.x += 300; cv.w = 400;

            if (k == BIND_ROW_PORT) {
                snprintf(line, sizeof line, "Controller %d", g_bind_port + 1);
                theme_text(lr, "Editing", 12, 600,
                           lit > 0.5 ? THEME_TEXT : THEME_TEXT_DIM, THEME_LEFT);
                theme_text(cv, line, 12, 400,
                           lit > 0.5 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);
            } else if (k == BIND_ROW_DEVICE) {
                if (bp->device == BIND_DEV_PAD) {
                    char name[64];

                    recomp_pad_name(bp->pad, name, sizeof name);
                    if (name[0])
                        snprintf(line, sizeof line, "Gamepad %d  (%s)", bp->pad + 1, name);
                    else
                        snprintf(line, sizeof line, "Gamepad %d  (not plugged in)", bp->pad + 1);
                } else if (bp->device == BIND_DEV_KEYBOARD) {
                    snprintf(line, sizeof line, "Keyboard");
                } else {
                    snprintf(line, sizeof line, "Nothing");
                }
                theme_text(lr, "Read from", 12, 600,
                           lit > 0.5 ? THEME_TEXT : THEME_TEXT_DIM, THEME_LEFT);
                theme_text(cv, line, 12, 400,
                           lit > 0.5 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);
            } else if (k == BIND_ROW_API) {
                theme_text(lr, "Controllers", 12, 600,
                           lit > 0.5 ? THEME_TEXT : THEME_TEXT_DIM, THEME_LEFT);
                theme_text(cv, g_bind.pad_api == BIND_PAD_API_XINPUT
                                   ? "Xbox controllers only (XInput)"
                                   : "All controllers (SDL3)",
                           12, 400, lit > 0.5 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);
            } else if (k == BIND_ROW_RESET) {
                theme_text(lr, "Reset this controller", 12, 600,
                           lit > 0.5 ? THEME_TEXT : THEME_TEXT_DIM, THEME_LEFT);
                theme_text(cv, "Back to the defaults", 12, 400,
                           lit > 0.5 ? THEME_GREEN : THEME_TEXT_DIM, THEME_LEFT);
            } else {
                int c = k - BIND_HEAD_ROWS;

                label = bind_control_labels[c];
                theme_text(lr, label, 12, 600,
                           lit > 0.5 ? THEME_TEXT : THEME_TEXT_DIM, THEME_LEFT);

                cv.w = 200;
                theme_text(cv,
                           (g_capturing && lit > 0.5 && g_bind_col == 0)
                               ? "press something..." : bind_source_label(bp->pad_src[c]),
                           12, 400,
                           (lit > 0.5 && g_bind_col == 0) ? THEME_GREEN : THEME_TEXT_DIM,
                           THEME_LEFT);
                cv.x += 210;
                theme_text(cv,
                           (g_capturing && lit > 0.5 && g_bind_col == 1)
                               ? "press a key..." : bind_source_label(bp->key_src[c]),
                           12, 400,
                           (lit > 0.5 && g_bind_col == 1) ? THEME_GREEN : THEME_TEXT_DIM,
                           THEME_LEFT);
            }

            if (lit > 0.5 && k <= BIND_ROW_API)
                theme_arrows(arrows_rect(rr), 1, 1, lit);
        }

        /* Where we are in a list longer than the screen. */
        {
            ThemeRect sb;

            sb.x = g_cw - 48;
            sb.w = 4;
            sb.h = (BIND_ROWS_VISIBLE * 62) * BIND_ROWS_VISIBLE / BIND_TOTAL_ROWS;
            sb.y = row_rect(0).y +
                   (BIND_ROWS_VISIBLE * 62 - sb.h) * g_bind_top /
                   (BIND_TOTAL_ROWS - BIND_ROWS_VISIBLE);
            theme_panel(sb, 0.5);
        }

        r.x = 62; r.y = row_rect(0).y + BIND_ROWS_VISIBLE * 62 + 2;
        r.w = g_cw - 124; r.h = 40;
        {
            int k = g_bind_top + g_sel;

            theme_text_wrapped(r,
                g_capturing ? "Press what you want this to be. Escape cancels."
                : k == BIND_ROW_PORT   ? "Which of the four controllers these bindings are for."
                : k == BIND_ROW_DEVICE ? "What this controller reads: a gamepad, the keyboard, or nothing."
                : k == BIND_ROW_API    ? "All controllers reads Xbox, PlayStation, Switch and most other pads. "
                                         "Xbox controllers only is the older XInput path, for a pad that misbehaves. "
                                         "Every game uses this."
                : k == BIND_ROW_RESET  ? "Put this controller back to the built-in mapping."
                : "Left and right choose the controller or the keyboard column. "
                  "A or Enter rebinds. Bindings are shared by every game.",
                11, THEME_TEXT_DIM);
        }
    } else {
        ThemeRect p = row_rect(0);
        char line[1024];

        p.h = 190;
        theme_panel(p, 0.25);
        p.x += 24; p.y += 18; p.w -= 48; p.h = 26;
        theme_text(p, g_title_name, 15, 700, THEME_TEXT, THEME_LEFT);

        p.y += 32;
        snprintf(line, sizeof line, "Title id  %08X", g_title_id);
        theme_text(p, line, 11, 400, THEME_TEXT_DIM, THEME_LEFT);

        p.y += 24;
        {
            char path[1024];
            if (recomp_settings_path(g_title_id, path, sizeof path))
                snprintf(line, sizeof line, "Settings  %s", path);
            else
                snprintf(line, sizeof line, "Settings  (nowhere to write)");
        }
        theme_text(p, line, 11, 400, THEME_TEXT_DIM, THEME_LEFT);

        p.y += 24;
        snprintf(line, sizeof line, "Game      %s", LAUNCHER_GAME_EXE);
        theme_text(p, line, 11, 400, THEME_TEXT_DIM, THEME_LEFT);

        /* Where the title id came from, or that it did not: without it the
         * settings above go to default.conf, which the game does not read. */
        p.y += 24;
        if (g_xbe_found[0])
            snprintf(line, sizeof line, "Files     %s", g_xbe_found);
        else
            snprintf(line, sizeof line, "Files     not found -- settings will not reach the game");
        theme_text(p, line, 11, 400, g_xbe_found[0] ? THEME_TEXT_DIM : THEME_TEXT,
                   THEME_LEFT);

        p.y += 34; p.h = 60;
        theme_text_wrapped(p,
            "A native build of an Xbox game, not an emulator. Settings "
            "chosen here are written to a file the game reads when it starts.",
            11, THEME_TEXT_DIM);
    }

    /* The footer says how to drive it, and changes with what is plugged in. */
    r.x = 60; r.y = g_ch - 44; r.w = g_cw - 260; r.h = 24;
    theme_text(r, g_pad_seen
                  ? "A  Play      B  Quit      LB RB  Change tab      D-pad  Move"
                  : "Enter  Play      Esc  Quit      Tab  Change tab      Arrows  Move",
               11, 400, THEME_TEXT_DIM, THEME_LEFT);

    {   /* Play, always on screen, always the obvious thing to press. */
        ThemeRect b;

        b.w = 150; b.h = 42;
        b.x = g_cw - 60 - b.w;
        b.y = g_ch - 74;
        theme_panel(b, 0.85);
        theme_text(b, "PLAY", 13, 700, THEME_TEXT, THEME_CENTRE);
    }
}

/* ---------------------------------------------------------------- window */

static int hit_row(int mx, int my)
{
    int i;

    if (g_tab != TAB_VIDEO)
        return -1;
    for (i = 0; i < g_video_count; i++) {
        ThemeRect r = row_rect(i);

        if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h)
            return i;
    }
    return -1;
}

/* The Input tab's visible row under the mouse, or -1. Its rows are shorter
 * than the Video tab's (46, not 52) and the list scrolls. */
static int hit_bind_row(int mx, int my)
{
    int j;

    if (g_tab != TAB_INPUT)
        return -1;
    for (j = 0; j < BIND_ROWS_VISIBLE && g_bind_top + j < BIND_TOTAL_ROWS; j++) {
        ThemeRect r = row_rect(j);

        r.h = 46;
        if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h)
            return j;
    }
    return -1;
}

static int hit_play(int mx, int my)
{
    return mx >= g_cw - 60 - 150 && mx < g_cw - 60 &&
           my >= g_ch - 74 && my < g_ch - 32;
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);

        draw();
        theme_present(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;                       /* everything is painted */
    case WM_KEYDOWN:
        switch (wp) {
        case VK_UP:     nav(0, -1, 0, 0, 0); break;
        case VK_DOWN:   nav(0,  1, 0, 0, 0); break;
        case VK_LEFT:   nav(-1, 0, 0, 0, 0); break;
        case VK_RIGHT:  nav(1,  0, 0, 0, 0); break;
        case VK_RETURN: nav(0, 0, 1, 0, 0); break;
        case VK_ESCAPE: nav(0, 0, 0, 1, 0); break;
        case VK_TAB:    nav(0, 0, 0, 0, (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1); break;
        default: break;
        }
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        int mx = LOWORD(lp), my = HIWORD(lp), i;

        for (i = 0; i < TAB_COUNT; i++) {
            ThemeRect t = tab_rect(i);

            if (mx >= t.x && mx < t.x + t.w && my >= t.y && my < t.y + t.h)
                set_tab((Tab)i);
        }
        if (g_tab == TAB_INPUT) {
            int j;

            for (j = 0; j < BIND_ROWS_VISIBLE; j++) {
                ThemeRect rr = row_rect(j);

                rr.h = 46;
                if (my >= rr.y && my < rr.y + rr.h && mx >= rr.x &&
                    mx < rr.x + rr.w && g_bind_top + j < BIND_TOTAL_ROWS) {
                    int k = g_bind_top + j;

                    g_sel = j;
                    if (k < BIND_HEAD_ROWS) {
                        /* The head rows change on a click: back or forward
                         * on the arrows, forward anywhere else on the row.
                         * (Splitting the whole row in half put both drawn
                         * arrows in the forward half.) */
                        int dir = hit_arrow(rr, mx);

                        nav(dir ? dir : 1, 0, k == BIND_ROW_RESET, 0, 0);
                    } else {
                        g_bind_col = (mx >= rr.x + 510) ? 1 : 0;
                        g_capturing = 1;
                        g_capture_armed = 0;
                    }
                }
            }
            if (hit_play(mx, my))
                do_play();
            InvalidateRect(h, NULL, FALSE);
            return 0;
        }
        i = hit_row(mx, my);
        if (i >= 0) {
            int dir = hit_arrow(row_rect(i), mx);

            g_sel = i;
            /* Back or forward on the arrows; anywhere else on the row steps
             * it forward, which is how a toggle is clicked. */
            row_move(&g_video_rows[i], dir ? dir : 1);
        }
        if (hit_play(mx, my))
            do_play();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int i = hit_row(LOWORD(lp), HIWORD(lp));

        /* The highlight follows the mouse on both lists -- but not away from
         * a row waiting for the key to bind to it. */
        if (i < 0 && !g_capturing)
            i = hit_bind_row(LOWORD(lp), HIWORD(lp));
        if (i >= 0 && i != g_sel) {
            g_sel = i;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }
    case WM_TIMER: {
        int dx, dy, a, b, t;

        g_pulse += 0.006;
        if (g_pulse > 1.0) g_pulse -= 1.0;
        if (g_tab_t < 1.0) {
            g_tab_t += 0.12;
            if (g_tab_t > 1.0) g_tab_t = 1.0;
        }
        g_pad_seen = pad_poll(&dx, &dy, &a, &b, &t);
        if (g_capturing)
            capture_tick();             /* the press is the answer, not a move */
        else if (g_pad_seen && (dx || dy || a || b || t))
            nav(dx, dy, a, b, t);
        rows_for_tab();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_SIZE:
        g_cw = LOWORD(lp);
        g_ch = HIWORD(lp);
        if (g_cw > 0 && g_ch > 0)
            theme_begin(g_cw, g_ch);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc;
    MSG msg;
    RECT r;

    (void)prev; (void)cmd;

    /* Before any window exists: otherwise Windows draws us smaller and
     * scales the result up, which on a look made of soft edges and
     * glows shows as a blur over all of it. */
    SetProcessDPIAware();

    read_title();
    settings_load();
    bindings_load();
    build_rows();
    rows_for_tab();

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "xboxrecomp_launcher";
    if (!RegisterClassA(&wc))
        return 1;

    r.left = r.top = 0;
    r.right = WINDOW_W;
    r.bottom = WINDOW_H;
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    {
        char caption[160];

        snprintf(caption, sizeof caption, "%s", g_title_name);
        g_hwnd = CreateWindowA(wc.lpszClassName, caption,
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               r.right - r.left, r.bottom - r.top,
                               NULL, NULL, inst, NULL);
    }
    if (!g_hwnd)
        return 1;
    if (!theme_begin(WINDOW_W, WINDOW_H))
        return 1;

    ShowWindow(g_hwnd, show);
    SetTimer(g_hwnd, 1, 16, NULL);          /* the pad, and the glow */

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    theme_shutdown();
    return 0;
}
