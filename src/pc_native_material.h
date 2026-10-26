#ifndef TSFP_PC_NATIVE_MATERIAL_H
#define TSFP_PC_NATIVE_MATERIAL_H
#include <math.h>

/* Fractional power of the authored material map:
 * v' = A*v + B; uv' = uv + C*v'. B is the forcing held by the caller,
 * C includes one raw tick's game speed and texture dimension. Normalize
 * velocity AND travel together; normalizing damping alone changes UV motion.
 * No history, guest writes, allocation or procedural-image scheduling here. */
typedef struct PcMaterialMotion {
    double decay,force_scale,velocity,forcing;
} PcMaterialMotion;
static inline int pc_material_motion(float raw,float game,float drag,PcMaterialMotion *out)
{
    if(!out || !isfinite(raw) || !isfinite(game) || !isfinite(drag) ||
       !(raw>0 && raw<1 && game>0) || drag<0)return 0;
    /* Retain the authored float subtraction at one raw tick. Nonpositive
     * bases have no supported real fractional power; preserve their old path. */
    float base=1.f-drag*(game/raw);
    if(!(base>0 && base<=1) || !isfinite(base))return 0;
    double r=raw,d=1.-base,g,k,a;
    if(d==0) {a=1;g=r;k=r*(1-r)/2;}
    else {
        double logarithm=log1p(-d);
        a=exp(r*logarithm);g=-expm1(r*logarithm)/d;
        /* H = A*(1-A^r)/((1-A)*A^r), K = (r-H)/(1-A).
         * Avoid cancellation for weak damping, including the zero-drag limit. */
        if(d<1e-6)k=r*(1-r)*(.5+(1+r)*d/6+(1+r)*(2+r)*d*d/24);
        else k=(r-(1-d)*g/a)/d;
    }
    PcMaterialMotion result={a,g/r,(r-d*k)/r,k/(r*g)};
    if(!isfinite(result.decay) || !isfinite(result.force_scale) ||
       !isfinite(result.velocity) || !isfinite(result.forcing))return 0;
    *out=result;return 1;
}
#endif
