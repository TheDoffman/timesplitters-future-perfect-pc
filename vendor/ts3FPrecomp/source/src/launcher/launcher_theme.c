/*
 * launcher_theme.c -- see launcher_theme.h.
 *
 * The background and the panels are written pixel by pixel into a 32-bit
 * DIB rather than drawn with GDI shapes. That is not perversity: GDI has
 * no antialiasing and no alpha on its primitives, and this look is made
 * of soft edges and washes. Arithmetic over a buffer gives both, costs a
 * few milliseconds on a window this size, and needs nothing beyond what
 * every Windows install already has.
 *
 * Text is the exception and goes through GDI, which can draw into the
 * same DIB and already knows how to shape and hint a font.
 */
#include "launcher_theme.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static HDC      g_dc;
static HBITMAP  g_bmp, g_old;
static uint32_t *g_px;
static int      g_w, g_h;

int theme_begin(int width, int height)
{
    BITMAPINFO bi;

    if (g_dc && width == g_w && height == g_h)
        return 1;
    theme_shutdown();

    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize        = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth       = width;
    bi.bmiHeader.biHeight      = -height;          /* top-down */
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    g_dc = CreateCompatibleDC(NULL);
    if (!g_dc)
        return 0;
    g_bmp = CreateDIBSection(g_dc, &bi, DIB_RGB_COLORS, (void **)&g_px, NULL, 0);
    if (!g_bmp) {
        DeleteDC(g_dc);
        g_dc = NULL;
        return 0;
    }
    g_old = (HBITMAP)SelectObject(g_dc, g_bmp);
    g_w = width;
    g_h = height;
    SetBkMode(g_dc, TRANSPARENT);
    return 1;
}

void theme_shutdown(void)
{
    if (g_dc) {
        SelectObject(g_dc, g_old);
        DeleteObject(g_bmp);
        DeleteDC(g_dc);
    }
    g_dc = NULL;
    g_bmp = NULL;
    g_px = NULL;
    g_w = g_h = 0;
}

void theme_present(HDC dc)
{
    if (g_dc)
        BitBlt(dc, 0, 0, g_w, g_h, g_dc, 0, 0, SRCCOPY);
}

/* ------------------------------------------------------------- pixels */

static double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

static void blend(int x, int y, int r, int g, int b, double a)
{
    uint32_t *p;
    int sr, sg, sb;

    if (x < 0 || y < 0 || x >= g_w || y >= g_h || a <= 0)
        return;
    if (a > 1) a = 1;
    p = &g_px[(size_t)y * g_w + x];
    sb = (int)(*p & 0xFF);
    sg = (int)((*p >> 8) & 0xFF);
    sr = (int)((*p >> 16) & 0xFF);
    sr = (int)(sr + (r - sr) * a);
    sg = (int)(sg + (g - sg) * a);
    sb = (int)(sb + (b - sb) * a);
    *p = ((uint32_t)sr << 16) | ((uint32_t)sg << 8) | (uint32_t)sb;
}

void theme_background(double pulse)
{
    /* Two glows behind the content, the larger one breathing. The
     * dashboard's light came from behind and below; this is that, not a
     * copy of it. */
    const double cx = g_w * 0.5, cy = g_h * 0.52;
    const double big = g_w * (0.62 + 0.03 * sin(pulse * 6.2831853));
    const double small = g_w * 0.26;
    int x, y;

    if (!g_px)
        return;

    for (y = 0; y < g_h; y++) {
        for (x = 0; x < g_w; x++) {
            double dx = x - cx, dy = (y - cy) * 1.25;
            double d = sqrt(dx * dx + dy * dy);
            double a = clamp01(1.0 - d / big);
            double b = clamp01(1.0 - d / small);
            double v = a * a * 0.22 + b * b * 0.28;
            int r = (int)(0x18 + v * 0x30);
            int g = (int)(0x1E + v * 0xB0);
            int bl = (int)(0x12 + v * 0x22);

            g_px[(size_t)y * g_w + x] =
                ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
        }
    }

    /* A vignette, so the edges fall away and the eye goes to the middle. */
    for (y = 0; y < g_h; y++)
        for (x = 0; x < g_w; x++) {
            double nx = (x - cx) / (g_w * 0.5), ny = (y - cy) / (g_h * 0.5);
            double d = sqrt(nx * nx + ny * ny);
            double v = clamp01((d - 0.75) / 0.75);

            if (v > 0)
                blend(x, y, 0, 0, 0, v * 0.85);
        }
}

/* Distance from a point to a rounded rectangle: negative inside,
 * positive outside, and the magnitude is how far. Soft edges and the
 * glow around them both fall out of it. */
static double rrect_distance(double px, double py, ThemeRect r, double radius)
{
    double hx = r.w * 0.5, hy = r.h * 0.5;
    double cx = r.x + hx, cy = r.y + hy;
    double dx = fabs(px - cx) - (hx - radius);
    double dy = fabs(py - cy) - (hy - radius);
    double ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
    double inside = (dx > dy ? dx : dy);

    if (inside > 0)
        inside = 0;
    return sqrt(ox * ox + oy * oy) + inside - radius;
}

void theme_panel(ThemeRect r, double lit)
{
    const double radius = 10.0;
    /* Room for the glow to reach nothing. Too little and it is cut off
     * square, which reads as a box drawn round the selected row rather
     * than light coming off it -- the falloff below has to have faded
     * to nothing by the time it reaches this edge. */
    const int pad = 52;
    int x0 = r.x - pad, y0 = r.y - pad;
    int x1 = r.x + r.w + pad, y1 = r.y + r.h + pad;
    int x, y;

    if (!g_px)
        return;
    lit = clamp01(lit);

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            double d = rrect_distance(x + 0.5, y + 0.5, r, radius);

            if (d < 0) {
                /* Inside: a dark slab that greens as it lights up. */
                double edge = clamp01(-d);            /* antialias the rim */
                double fill = 0.55 + 0.20 * lit;

                blend(x, y, (int)(0x0A + 0x18 * lit),
                            (int)(0x12 + 0x3C * lit),
                            (int)(0x08 + 0x0E * lit), fill * edge);
            } else {
                /* Outside: the glow, which only exists when lit. */
                double g = exp(-d / (4.0 + 7.0 * lit)) * (0.07 + 0.42 * lit);

                blend(x, y, 0x9B, 0xCE, 0x1E, g);
            }
            /* The rim itself, brightest right on the boundary. */
            if (d > -2.0 && d < 1.0) {
                double rim = 1.0 - fabs(d + 0.5) / 1.5;

                if (rim > 0)
                    blend(x, y, 0x9B, 0xCE, 0x1E, rim * (0.25 + 0.6 * lit));
            }
        }
    }
}

void theme_tab_underline(ThemeRect from, ThemeRect to, double t)
{
    double e = clamp01(t);
    int x0, x1, y, x;

    /* Ease, so it settles rather than stopping dead. */
    e = e * e * (3.0 - 2.0 * e);
    x0 = (int)(from.x + (to.x - from.x) * e);
    x1 = x0 + (int)(from.w + (to.w - from.w) * e);
    y = from.y;

    /* Drawn rather than panelled: a panel brings a panel's glow, and a
     * three pixel bar wearing a twenty-six pixel halo sits under the
     * whole strip instead of under one tab. */
    for (x = x0; x < x1; x++) {
        double edge = 1.0;
        int run = x - x0, left = x1 - x;

        if (run < 10) edge = run / 10.0;        /* fade the ends */
        if (left < 10) edge = left / 10.0;
        blend(x, y,     0x9B, 0xCE, 0x1E, 0.95 * edge);
        blend(x, y + 1, 0x9B, 0xCE, 0x1E, 0.95 * edge);
        blend(x, y + 2, 0x9B, 0xCE, 0x1E, 0.45 * edge);
        blend(x, y + 3, 0x9B, 0xCE, 0x1E, 0.18 * edge);
        blend(x, y - 1, 0x9B, 0xCE, 0x1E, 0.25 * edge);
    }
}

void theme_arrows(ThemeRect r, int can_left, int can_right, double lit)
{
    const int size = 7;
    int cy = r.y + r.h / 2, i;

    for (i = 0; i < size; i++) {
        double a = (0.25 + 0.65 * lit);
        int run = i + 1;                 /* tip at the outside edge */
        int j;

        for (j = -run / 2; j <= run / 2; j++) {
            if (can_left)
                blend(r.x + i, cy + j, 0x9B, 0xCE, 0x1E, a);
            if (can_right)
                blend(r.x + r.w - 1 - i, cy + j, 0x9B, 0xCE, 0x1E, a);
        }
    }
}

/* --------------------------------------------------------------- text */

void theme_text(ThemeRect r, const char *text, int size, int weight,
                COLORREF colour, ThemeAlign align)
{
    HFONT font, old;
    RECT rc;
    UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;

    if (!g_dc || !text)
        return;

    /* A face that is on every Windows install, and is not the console's.
     * Segoe UI where it exists, Arial where it does not. */
    font = CreateFontA(-MulDiv(size, 96, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    if (!font)
        return;
    old = (HFONT)SelectObject(g_dc, font);
    SetTextColor(g_dc, colour);

    rc.left = r.x;
    rc.top = r.y;
    rc.right = r.x + r.w;
    rc.bottom = r.y + r.h;
    if (align == THEME_CENTRE) flags |= DT_CENTER;
    else if (align == THEME_RIGHT) flags |= DT_RIGHT;

    DrawTextA(g_dc, text, -1, &rc, flags);
    SelectObject(g_dc, old);
    DeleteObject(font);
}

void theme_text_wrapped(ThemeRect r, const char *text, int size, COLORREF colour)
{
    HFONT font, old;
    RECT rc;

    if (!g_dc || !text)
        return;
    font = CreateFontA(-MulDiv(size, 96, 72), 0, 0, 0, 400, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    if (!font)
        return;
    old = (HFONT)SelectObject(g_dc, font);
    SetTextColor(g_dc, colour);

    rc.left = r.x;
    rc.top = r.y;
    rc.right = r.x + r.w;
    rc.bottom = r.y + r.h;
    DrawTextA(g_dc, text, -1, &rc, DT_WORDBREAK | DT_TOP | DT_NOPREFIX);
    SelectObject(g_dc, old);
    DeleteObject(font);
}
