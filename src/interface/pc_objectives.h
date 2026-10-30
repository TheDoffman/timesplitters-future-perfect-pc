#ifndef TSFP_PC_OBJECTIVES_H
#define TSFP_PC_OBJECTIVES_H

/* Fit the US health/shield geometry in its current guest viewport. Both
 * renderers map this viewport to the selected output resolution, so the same
 * fractional inset works at any resolution and in split-screen. Shrink only;
 * retain the original animated scale and use one factor for both axes. */
static inline float pc_objectives_scale(float scale, float left, float top,
        float right, float bottom, float cx, float cy, unsigned players,
        unsigned index)
{
    float mx, my, half_width, half_height, limit, fit;
    int narrow = players > 2 && !(players == 3 && index == 0);
    if (!(scale > 0) || !(right > left) || !(bottom > top) ||
        players < 1 || players > 4 || index >= players) return scale;
    half_width = narrow ? 300.f * 1.08f * .5f : 512.f * 1.2f * .5f;
    half_height = (players == 1 ? 340.f : 220.f) * .5f;
    mx = (right-left) * .04f;
    my = (bottom-top) * .04f;
    limit = (cx-left-mx)/half_width;
    fit = (right-cx-mx)/half_width; if (fit < limit) limit = fit;
    fit = (cy-top-my)/half_height; if (fit < limit) limit = fit;
    fit = (bottom-cy-my)/half_height; if (fit < limit) limit = fit;
    if (limit < 0) limit = 0;
    return scale > limit ? limit : scale;
}
#endif
