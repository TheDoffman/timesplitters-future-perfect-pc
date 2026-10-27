/* Position inside the authored canvas, shared by both renderer adapters. */
#ifndef TSFP_PC_UI_LAYOUT_H
#define TSFP_PC_UI_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
/* The crane vignette's full-canvas Xbox clip uses inclusive last pixels.
 * Convert only this tagged effect to host-exclusive bounds, before scaling.
 * Keep partial clips, other widgets and the stored guest scissor unchanged. */
static void pc_ui_vignette_clip(uint32_t tag,int placement,int32_t rect[4])
{
    if(tag!=0x00137010u || placement!=1 || rect[0]!=0 || rect[1]!=0 ||
       (rect[2]!=639 && rect[2]!=640) || (rect[3]!=479 && rect[3]!=480))return;
    rect[2]=640;rect[3]=480;
}
/* US menu health draw: state 8 is the completed-match/results phase.
 * Keep Story objectives and the ordinary pause HUD unchanged. */
static int pc_ui_hide_results_health(unsigned caller,unsigned phase,unsigned mode)
{
    return caller==0x002e58c9u && phase==8u && mode!=10u;
}
/* A textured full-canvas quad is a screen effect too (e.g. animated noise),
 * even when it does not sample a framebuffer copy. Inspect one complete quad,
 * not a batch's bounds: separate HUD pieces can span all four screen edges.
 * Accept the title's inclusive last pixel, and unaligned vertex declarations. */
static int pc_ui_full_canvas_quad(const void *vertices,unsigned stride,unsigned count,
        unsigned offset,float width,float height)
{
    const unsigned char *p=(const unsigned char *)vertices;
    unsigned corners=0;
    if(!p || count<4 || stride<8 || offset>stride-8 || width<2 || height<2)return 0;
    for(unsigned i=0;i<4;i++) {
        float xy[2];int x,y;
        memcpy(xy,p+(size_t)i*stride+offset,sizeof xy);
        x=xy[0]>=-1 && xy[0]<=1?0:
          xy[0]>=width-1 && xy[0]<=width+1?1:-1;
        y=xy[1]>=-1 && xy[1]<=1?0:
          xy[1]>=height-1 && xy[1]<=height+1?1:-1;
        if(x<0 || y<0)return 0;
        corners|=1u<<(y*2+x);
    }
    return corners==15;
}

static float pc_ui_side_anchor(float left,float right,float width)
{
    float centre=(left+right)*.5f;
    if(width<=0) return .5f;
    return centre<width/3.f?0.f:centre>width*2.f/3.f?1.f:.5f;
}
/* Ratio between a 4:3 authored canvas and the output. Fit either axis,
 * keeping backgrounds on the complete output canvas. */
static float pc_ui_scale_x(float ratio) { return ratio<1.f?ratio:1.f; }
static float pc_ui_scale_y(float ratio) { return ratio>1.f?1.f/ratio:1.f; }
/* The health/armor mesh is wider than its nominal 512-unit span. Fit the
 * complete curves before anchoring their two halves, including menu zoom. */
static float pc_ui_health_scale(float scale,float left,float top,float right,
        float bottom,float cx,float cy)
{
    float mx=(right-left)*.04f,my=(bottom-top)*.04f,limit,fit;
    if(!(scale>0) || !(right>left) || !(bottom>top))return scale;
    limit=(cx-left-mx)/307.2f;
    fit=(right-cx-mx)/307.2f;if(fit<limit)limit=fit;
    fit=(cy-top-my)/170.f;if(fit<limit)limit=fit;
    fit=(bottom-cy-my)/170.f;if(fit<limit)limit=fit;
    if(limit<0)limit=0;
    return scale<limit?scale:limit;
}
/* Keep a scope aperture proportional while its opaque outer rectangles reach
 * the output edges. Render the result with STRETCH: interior coordinates use
 * the same fit as the reticle; only each mask's designated outside edge stays
 * on the complete output canvas. side: left=0, top=1, right=2, bottom=3. */
static void pc_ui_scope_mask(int32_t rect[4],unsigned side,float ratio)
{
    float sx=pc_ui_scale_x(ratio),sy=pc_ui_scale_y(ratio);
    for(unsigned i=0;i<4;i++) {
        float centre=(i&1)?240.f:320.f,scale=(i&1)?sy:sx;
        float value=centre+((float)rect[i]-centre)*scale;
        /* Round outward to overlap the textured aperture by at least the
           original one-pixel seam allowance. */
        rect[i]=(int32_t)((i<2)?floorf(value):ceilf(value));
    }
    if(side==0){rect[0]=0;rect[1]=0;rect[3]=480;}
    if(side==1){rect[0]=0;rect[1]=0;rect[2]=640;}
    if(side==2){rect[2]=640;rect[1]=0;rect[3]=480;}
    if(side==3){rect[0]=0;rect[3]=480;rect[2]=640;}
}
/* Apply the canvas fit around the aim point, not the screen centre. This
 * also preserves rotated quads and keeps the original texture UVs intact. */
static void pc_ui_aim_shape(float *xy,unsigned points,float ratio)
{
    float cx=0,cy=0,sx=pc_ui_scale_x(ratio),sy=pc_ui_scale_y(ratio);
    if(!points || !(ratio>0.f))return;
    for(unsigned i=0;i<points;i++){cx+=xy[i*2];cy+=xy[i*2+1];}
    cx/=points;cy/=points;
    for(unsigned i=0;i<points;i++){
        xy[i*2]=cx+(xy[i*2]-cx)*sx;
        xy[i*2+1]=cy+(xy[i*2+1]-cy)*sy;
    }
}
#endif
