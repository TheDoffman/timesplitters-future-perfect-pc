#ifndef TSFP_PC_INPUT_ICON_LAYOUT_H
#define TSFP_PC_INPUT_ICON_LAYOUT_H
#include <math.h>
/* Deliberate aspect families, in the title's aspect-correct 640x480 UI space.
 * Output height determines physical size; extra monitor width never stretches
 * a circular controller icon into an oval. */
static inline float pc_input_icon_aspect(unsigned width,unsigned height)
{
    if(!width || !height)return 1.f;
    double aspect=(double)width/height;
    return aspect<1.5?1.f:aspect<1.7?1.05f:aspect<2.1?1.1f:aspect<3.?1.15f:1.2f;
}
static inline float pc_input_icon_pixels_per_unit(unsigned height)
{ return height>=480?(float)height/480.f:1.f; }
static inline float pc_input_icon_canvas(unsigned width,unsigned height,float base,float limit,int compact)
{
    if(!(base>0.f) || !(limit>0.f))return 0.f;
    float pixels=pc_input_icon_pixels_per_unit(height);
    float target=base*(compact?1.f:pc_input_icon_aspect(width,height));
    /* Four physical-pixel steps replace fractional, continuously changing sizes.
     * Native row/clip constraints take precedence over an upward size step. */
    target=floorf(target*pixels/4.f+.5f)*4.f/pixels;
    return fminf(limit,fmaxf(4.f/pixels,target));
}
static inline float pc_input_icon_fit(float canvas,float available,unsigned height)
{
    if(!(canvas>0.f) || !(available>0.f))return 0.f;
    float pixels=pc_input_icon_pixels_per_unit(height);
    return floorf(fminf(canvas,available)*pixels+.0001f)/pixels;
}
static inline float pc_input_icon_align(float position,unsigned height)
{
    float pixels=pc_input_icon_pixels_per_unit(height);
    return floorf(position*pixels+.5f)/pixels;
}
#endif
