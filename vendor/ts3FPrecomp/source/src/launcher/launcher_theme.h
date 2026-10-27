/*
 * launcher_theme.h -- how the launcher looks, kept behind an interface.
 *
 * Everything drawn goes through here, so the whole of the platform's
 * drawing lives in one file. Today that is GDI into a 32-bit DIB;
 * whenever the launcher needs to run somewhere other than Windows, this
 * is the file that is replaced rather than the launcher.
 *
 * The look is the original Xbox dashboard's: black, a green glow that
 * comes from behind, panels with soft edges, and the selected thing
 * lit rather than outlined. None of the console's own artwork is used
 * or reproduced -- no logos, no wordmarks, no fonts, no ripped
 * textures. What is here is drawn from arithmetic at run time.
 */
#ifndef XBOXRECOMP_LAUNCHER_THEME_H
#define XBOXRECOMP_LAUNCHER_THEME_H

#include <windows.h>

typedef struct ThemeRect { int x, y, w, h; } ThemeRect;

/* The Xbox green, and the darker shades that go with it. */
#define THEME_GREEN      RGB(0x9B, 0xCE, 0x1E)
#define THEME_GREEN_DIM  RGB(0x4E, 0x6C, 0x14)
#define THEME_TEXT       RGB(0xE8, 0xF2, 0xD8)
#define THEME_TEXT_DIM   RGB(0x8A, 0x97, 0x7C)

/* A back buffer the size of the window. Everything below draws into it
 * and theme_present puts it on screen in one go, so nothing flickers. */
int  theme_begin(int width, int height);
void theme_present(HDC dc);
void theme_shutdown(void);

/* The background: black, with the glow behind where the content sits.
 * `pulse` runs 0..1 and drifts, so the glow breathes as the dashboard's
 * did rather than sitting still. */
void theme_background(double pulse);

/* A panel. `lit` from 0 (resting) to 1 (selected) takes it from a dark
 * translucent slab to one edged and washed with green. */
void theme_panel(ThemeRect r, double lit);

/* Text, left/centre/right aligned within `r`. `size` is a point size;
 * `weight` is 400 for normal and 700 for bold. */
typedef enum { THEME_LEFT, THEME_CENTRE, THEME_RIGHT } ThemeAlign;
void theme_text(ThemeRect r, const char *text, int size, int weight,
                COLORREF colour, ThemeAlign align);

/* The same, wrapped to the rectangle and top aligned, for the lines
 * of explanation. theme_text is single line: a newline in its text
 * draws as nothing and the line runs off the edge. */
void theme_text_wrapped(ThemeRect r, const char *text, int size,
                        COLORREF colour);

/* The tab strip's underline, which slides rather than jumping: `from`
 * and `to` are tab rectangles and `t` runs 0..1 between them. */
void theme_tab_underline(ThemeRect from, ThemeRect to, double t);

/* A left/right chevron pair beside a value, dimmed when the value is at
 * the end of its range and there is nowhere further to go. */
void theme_arrows(ThemeRect r, int can_left, int can_right, double lit);

#endif /* XBOXRECOMP_LAUNCHER_THEME_H */
