/* Xbox scissors include the last pixel; host APIs use exclusive upper bounds. */
#ifndef TSFP_GUEST_SCISSOR_H
#define TSFP_GUEST_SCISSOR_H
#include <stdint.h>
#include <math.h>
#include <limits.h>

static int32_t pc_scissor_integer(double value)
{
    if(value <= INT32_MIN)return INT32_MIN;
    if(value >= INT32_MAX)return INT32_MAX;
    return (int32_t)value;
}

/* Convert before scaling, and round outwards so fractional resolution scales
 * cannot discard a row/column of a portal. Keep the stored guest rect intact.
 * Anchors/scales are the same transforms used for the accompanying viewport. */
static void pc_guest_scissor(const int32_t rect[4], double sx, double sy,
                             double kx, double ky, double ax, double ay,
                             int32_t out[4])
{
    const double scale[2]={sx,sy}, squeeze[2]={kx,ky}, anchor[2]={ax,ay};
    for(int i=0;i<2;i++) {
        double lo=anchor[i]+(rect[i]*scale[i]-anchor[i])*squeeze[i];
        double hi=anchor[i]+(((double)rect[i+2]+1.0)*scale[i]-anchor[i])*squeeze[i];
        if(!isfinite(lo) || !isfinite(hi)) {out[i]=out[i+2]=0;continue;}
        out[i]=pc_scissor_integer(floor(lo));
        out[i+2]=rect[i+2]<rect[i]?out[i]:pc_scissor_integer(ceil(hi));
    }
}
#endif
