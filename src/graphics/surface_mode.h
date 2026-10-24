#ifndef TSFP_SURFACE_MODE_H
#define TSFP_SURFACE_MODE_H
#include "surface_coupled.h"

/* Two immutable float4 texels per mode. Per-frame delta/scale are uniforms;
 * changing frame duration never requires a new coefficient image. */
typedef struct PcSurfaceMode { float eigen[4],fixed[4]; } PcSurfaceMode;
static inline int pc_surface_mode(double a,double b,double d,double k,
    double forcing,double bias,PcSurfaceMode *out)
{
    PcSurfaceCoupledStep check;
    if(!out || !pc_surface_coupled_step(a,b,d,k,forcing,bias,1,&check))return 0;
    double r=sqrt(d),x=(1+d*(1-a*b*k))/(2*r),angle=0,denominator=1;
    if(fabs(x-1)>=1e-12) {
        if(x<1) {angle=acos(x);denominator=sin(angle);}
        else {angle=-acosh(x);denominator=sinh(-angle);}
    }
    PcSurfaceMode result;
    result.eigen[0]=(float)((1/r-x)/denominator);
    result.eigen[1]=(float)(-a*k/(r*denominator));
    result.eigen[2]=(float)(d*b/(r*denominator));
    result.eigen[3]=(float)angle;
    double height=(forcing+bias/a)/k;
    result.fixed[0]=(float)((1-d)*height/(d*b)+bias);
    result.fixed[1]=(float)height;
    result.fixed[2]=(float)((1-d)/(d*b*k));
    result.fixed[3]=(float)(1/k);
    for(unsigned i=0;i<4;i++)if(!isfinite(result.eigen[i]) || !isfinite(result.fixed[i]))return 0;
    *out=result;return 1;
}
#endif
