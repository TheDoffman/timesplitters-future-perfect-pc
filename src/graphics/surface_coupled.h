#ifndef TSFP_SURFACE_COUPLED_H
#define TSFP_SURFACE_COUPLED_H
#include <math.h>

/* Experimental linear feedback step. State is (green - .5, blue).
 * The authored step is v = g + a*(forcing - restoring*b),
 * g' = v + bias, b' = damping*(b + travel*v).
 * Fractional powers keep these coupled, including the affine centering term.
 * A spatial solver must supply each mode's restoring coefficient; applying one
 * matrix independently to world texels is NOT an equivalent spatial update.
 * Clipping, storage quantization, disturbances and smoothing are outside this
 * linear operator. This helper is not enabled by the game runtime. */
typedef struct PcSurfaceCoupledStep { float row[2][4]; } PcSurfaceCoupledStep;

static inline int pc_surface_coupled_step(double acceleration, double travel,
    double damping, double restoring, double forcing, double bias, double delta,
    PcSurfaceCoupledStep *out)
{
    if(!out || !isfinite(acceleration) || !isfinite(travel) ||
       !isfinite(damping) || !isfinite(restoring) || !isfinite(forcing) ||
       !isfinite(bias) || !isfinite(delta) || acceleration<=0 || travel<=0 ||
       damping<=0 || damping>1 || restoring<=0 || delta<0 || delta>1)
        return 0;
    double a=acceleration, b=travel, d=damping, k=restoring;
    double m[2][2]={{1,-a*k},{d*b,d*(1-a*b*k)}};
    double c[2]={a*forcing+bias,d*b*a*forcing};
    double r=sqrt(d), x=(m[0][0]+m[1][1])/(2*r);
    /* Negative real eigenvalues do not have the required real principal step.
     * Reject unstable authored modes rather than silently alter their dynamics. */
    if(!isfinite(x) || x<=-1 || x>(1+d)/(2*r)+1e-12) return 0;
    PcSurfaceCoupledStep result={0};
    if(delta==0) {
        result.row[0][0]=result.row[1][1]=1;
    } else if(delta==1) {
        for(unsigned i=0;i<2;i++) {
            for(unsigned j=0;j<2;j++)result.row[i][j]=(float)m[i][j];
            result.row[i][2]=(float)c[i];
        }
    } else {
        double scale=pow(r,delta), ratio, center;
        if(fabs(x-1)<1e-12) { ratio=delta;center=1; }
        else if(x<1) {
            double theta=acos(x);
            ratio=sin(delta*theta)/sin(theta);center=cos(delta*theta);
        } else {
            double theta=acosh(x);
            ratio=sinh(delta*theta)/sinh(theta);center=cosh(delta*theta);
        }
        double p[2][2];
        for(unsigned i=0;i<2;i++)for(unsigned j=0;j<2;j++)
            p[i][j]=scale*((i==j?center:0)+ratio*(m[i][j]/r-(i==j?x:0)));
        /* Fixed point of the affine authored map, shared by every power. */
        double q[2];q[1]=c[0]/(a*k);
        q[0]=((1-m[1][1])*q[1]-c[1])/(d*b);
        for(unsigned i=0;i<2;i++) {
            for(unsigned j=0;j<2;j++)result.row[i][j]=(float)p[i][j];
            result.row[i][2]=(float)((1-p[i][i])*q[i]-p[i][1-i]*q[1-i]);
        }
    }
    for(unsigned i=0;i<2;i++)for(unsigned j=0;j<3;j++)
        if(!isfinite(result.row[i][j]))return 0;
    *out=result;
    return 1;
}
#endif
