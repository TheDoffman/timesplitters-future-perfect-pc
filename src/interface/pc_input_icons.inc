/* Aspect-sized native icons; original artwork and fallback runs are compiled. */
#include "pc_input_icon_layout.h"
#ifndef TSFP_INPUT_ICON_SIZE
#define TSFP_INPUT_ICON_SIZE 32
#endif
/* Only binding value cells opt in; captions and footer text retain their scale. */
static RECOMP_TLS float input_icon_cell_height,input_icon_cell_width;
#ifndef TSFP_KEYCAP_TEST
extern int tsfp_input_icon_begin(const void *spans,unsigned count,unsigned size,unsigned pixels);
extern void tsfp_input_icon_end(void);
#endif
static int input_icon_id(int key)
{
    return key>=0 && key<256?input_icon_lookup[key]:-1;
}
static void input_icon_image(PcInputIconImage image,float x,float y,float w,float h,uint32_t tint,unsigned pixels)
{
    extern void sub_0001A2E0(void);
#ifdef TSFP_KEYCAP_TEST
    (void)pixels; /* Span-only fixture has no GPU upload size. */
#endif
#ifndef TSFP_KEYCAP_TEST
    if(tsfp_input_icon_begin(input_icon_spans+image.first,image.count,TSFP_INPUT_ICON_SIZE,pixels)) {
        extern void sub_0001B7B0(void);
        /* Native textured quad preserves the title's placement and scissor.
         * Flush it inside the texture scope, before another font draw. */
        guest_call(sub_0001B7B0,9,bits(x),bits(y),bits(x+w),bits(y+h),0u,0u,2048u,2048u,tint);
        guest_call(sub_00022FF0,0);tsfp_input_icon_end();return;
    }
#endif
    for(unsigned i=0;i<image.count;i++) {
        const PcInputIconSpan *s=&input_icon_spans[image.first+i];
        unsigned a=(unsigned)s->a*(tint&255u)/255u;
        uint32_t colour=((uint32_t)(s->r*((tint>>24)&255u)/255u)<<24)|
                        ((uint32_t)(s->g*((tint>>16)&255u)/255u)<<16)|
                        ((uint32_t)(s->b*((tint>>8)&255u)/255u)<<8)|a;
        guest_call(sub_0001A2E0,5,bits(x+w*s->x/(float)TSFP_INPUT_ICON_SIZE),bits(y+h*s->y/(float)TSFP_INPUT_ICON_SIZE),
                   bits(x+w*(s->x+s->w)/(float)TSFP_INPUT_ICON_SIZE),bits(y+h*(s->y+s->h)/(float)TSFP_INPUT_ICON_SIZE),colour);
    }
}
static void input_icon_paint(const PcKeycapCap *cap)
{
    /* The supplied PNGs include transparent margins; compensate so their
     * visible key faces remain legible beside the game's original lettering. */
    /* The profile picker reserves a shorter, fixed two-line clip. */
    PcDisplayMode output=pc_display_effective();
    unsigned width=output.width?output.width:640u,height=output.height?output.height:480u;
    float base=(cap->b-cap->y)*(cap->controller?1.15f:menu_mouse_profile_icons?1.f:1.15f);
    float limit=(cap->b-cap->y)*(menu_mouse_profile_icons?1.f:1.35f);
    float available=fmaxf(base,(float)(cap->r-cap->x)+4.f),face_scale=1.f;
#ifdef TSFP_INPUT_ICON_METRICS
    if(!cap->controller)face_scale=input_icons[cap->icon*2+!!cap->pressed].scale;
#endif
    if(input_icon_cell_height>0.f && input_icon_cell_width>0.f) {
        base=fminf(22.f,input_icon_cell_height);limit=input_icon_cell_height;
        available=input_icon_cell_width;
    }
    float h=pc_input_icon_canvas(width,height,base,limit,menu_mouse_profile_icons);
    h=pc_input_icon_fit(h*face_scale,available,height);
    if(!(h>0.f))return;
    float x=pc_input_icon_align((cap->x+cap->r-h)*.5f,height);
    float y=pc_input_icon_align((cap->y+cap->b-h)*.5f,height);
    input_icon_image(input_icons[cap->icon*2+!!cap->pressed],x,y,h,h,cap->colour,(unsigned)floorf(h*pc_input_icon_pixels_per_unit(height)+.5f));
}
