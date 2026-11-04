#ifndef TSFP_SSAO_PROJECTION_H
#define TSFP_SSAO_PROJECTION_H
#include <math.h>
/* Row-vector inverse, with partial pivoting. Reject invalid/orthographic lenses. */
static int tsfp_ssao_inverse(const float *projection,float *inverse)
{
    double a[4][8];
    if(!projection||!inverse||fabsf(projection[11])<.0001f)return 0;
    for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++) {
        if(!isfinite(projection[r*4+c]))return 0;
        a[r][c]=projection[r*4+c];a[r][c+4]=r==c?1:0;
    }
    for(unsigned c=0;c<4;c++) {
        unsigned pivot=c;for(unsigned r=c+1;r<4;r++)if(fabs(a[r][c])>fabs(a[pivot][c]))pivot=r;
        if(fabs(a[pivot][c])<1e-12)return 0;
        for(unsigned i=0;i<8;i++){double t=a[c][i];a[c][i]=a[pivot][i];a[pivot][i]=t;}
        double divisor=a[c][c];for(unsigned i=0;i<8;i++)a[c][i]/=divisor;
        for(unsigned r=0;r<4;r++)if(r!=c) {
            double factor=a[r][c];for(unsigned i=0;i<8;i++)a[r][i]-=factor*a[c][i];
        }
    }
    for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++) {
        float value=(float)a[r][c+4];if(!isfinite(value))return 0;inverse[r*4+c]=value;
    }
    return 1;
}
#endif
