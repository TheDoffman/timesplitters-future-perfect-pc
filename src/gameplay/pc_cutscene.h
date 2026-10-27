#ifndef TSFP_PC_CUTSCENE_H
#define TSFP_PC_CUTSCENE_H

/* Apply the same full-width policy to the title's attract reel and logos.
 * Unrecognized movies retain the runtime's default presentation. */
static inline int pc_cutscene_movie_fill_width(const char *path)
{
    static const char *const movies[] = {"attract.xmv", "eag_e.xmv",
        "eag_f.xmv", "eag_i.xmv", "eag_s.xmv", "frd.xmv"};
    const char *name = path;
    if (!path) return 0;
    for (const char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') name = p + 1;
    for (unsigned i = 0; i < sizeof movies / sizeof *movies; ++i) {
        const char *p = name, *expected = movies[i];
        while (*p && *expected) {
            unsigned char c = (unsigned char)*p;
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c != (unsigned char)*expected) break;
            ++p; ++expected;
        }
        if (!*p && !*expected) return 1;
    }
    return 0;
}

/* The supported US attract reel pads its 640-pixel picture inside 720
 * encoded columns. Exclude that padding only when filling a wide display;
 * the movie renderer retains the original complete picture at 4:3. */
static inline void pc_cutscene_movie_crop(unsigned width, unsigned height,
                                         unsigned *left, unsigned *right)
{
    *left = *right = 0;
    if (width == 720 && height == 480) { *left = 38; *right = 42; }
}

/* The US game's camera/viewport coordinates have a 4:3 reference shape.
 * Preserve the original fully-open picture's aspect, then scale the current
 * animated matte by the resulting height. Work in guest pixels: each renderer
 * already maps those coordinates to the selected output resolution.
 * Wider-than-cinematic displays fill horizontally with a proportional crop
 * of the vertical aperture, never a side matte or a stretched projection.
 * A zero output mode retains the original/legacy presentation. */
static inline int pc_cutscene_bar_height(unsigned width, unsigned height,
                                         int viewport_height, int full_bar,
                                         int animated_bar)
{
    double factor, full, adjusted;
    if (!width || !height || viewport_height <= 0 || full_bar <= 0 ||
        full_bar >= viewport_height / 2 || animated_bar < 0 || animated_bar > full_bar)
        return animated_bar;
    factor = (double)width * 3.0 / ((double)height * 4.0);
    full = (viewport_height - (viewport_height - 2.0 * full_bar) * factor) / 2.0;
    if (full <= 0) return 0;
    adjusted = animated_bar * full / full_bar;
    return (int)(adjusted + .5);
}

static inline int pc_cutscene_bar_width(unsigned width, unsigned height,
                                        int viewport_width, int viewport_height,
                                        int full_bar, int animated_bar)
{
    /* Retain the wrapper's common interface: neither story nor in-game
     * cinematic rectangles should turn into vertical side bars. */
    (void)width; (void)height; (void)viewport_width; (void)viewport_height;
    (void)full_bar; (void)animated_bar;
    return 0;
}

static inline float pc_cutscene_camera_zoom(unsigned width, unsigned height,
                                             int viewport_height, int full_bar,
                                             int animated_bar)
{
    int adjusted;
    float zoom, fill;
    if (viewport_height <= 0 || animated_bar <= 0 ||
        animated_bar >= viewport_height / 2 || full_bar <= 0 ||
        animated_bar > full_bar || full_bar >= viewport_height / 2)
        return 1.f;
    adjusted = pc_cutscene_bar_height(width,height,viewport_height,full_bar,animated_bar);
    zoom = (float)(viewport_height - 2 * adjusted) /
           (float)(viewport_height - 2 * animated_bar);
    if (!width || !height) return zoom;
    /* Keep the original horizontal camera aperture when filling an ultrawide
     * display. Widening it instead exposes the edge of authored backdrops.
     * Blend with the existing matte animation to avoid a camera snap. */
    fill = (float)width * 3.f / ((float)height * 4.f);
    if (fill <= (float)viewport_height / (viewport_height - 2 * full_bar))
        return zoom;
    fill = 1.f + (fill - 1.f) *
                 (float)animated_bar / (float)full_bar;
    return zoom > fill ? zoom : fill;
}
#endif
