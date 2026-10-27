#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/gameplay/pc_native_material.h"

int main(void)
{
    const float speeds[]={.25f,1,2},drags[]={0,1e-8f,1e-6f,.1f,.4f};
    const float pieces[][4]={{.5f,.5f,0,0},{.25f,.25f,.25f,.25f},
                            {5.f/12,5.f/12,1.f/6,0},{.1f,.2f,.3f,.4f}};
    unsigned cases=0;
    for(unsigned s=0;s<3;s++)for(unsigned d=0;d<5;d++)
    for(unsigned p=0;p<4;p++)for(int force=-1;force<=1;force++) {
        double v=2,x=.1,b=.75*force*speeds[s];
        float base=1.f-drags[d]*speeds[s];
        /* Independent one-step authored Euler map, no call to the helper. */
        double expected_v=base*v+b,expected_x=x+speeds[s]/60.*expected_v;
        for(unsigned i=0;i<4;i++)if(pieces[p][i]>0) {
            float raw=pieces[p][i];PcMaterialMotion c;
            assert(pc_material_motion(raw,raw*speeds[s],drags[d],&c));
            double forcing=b*raw*c.force_scale;
            v=c.decay*v+forcing;
            x+=raw*speeds[s]/60.*(c.velocity*v+c.forcing*forcing);
            assert(isfinite(v) && isfinite(x));
        }
        assert(fabs(v-expected_v)<.000002 && fabs(x-expected_x)<.000002);
        cases++;
    }
    const float invalid[][3]={{0,1,0},{-1,1,0},{1,1,0},{2,1,0},
                             {.5f,0,0},{.5f,-1,0},{.5f,1,-1},{.5f,.5f,1},
                             {.5f,1,1},{NAN,1,0},{.5f,INFINITY,0},{.5f,1,NAN}};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++) {
        PcMaterialMotion c={1,2,3,4},saved=c;
        assert(!pc_material_motion(invalid[i][0],invalid[i][1],invalid[i][2],&c));
        assert(!memcmp(&c,&saved,sizeof c));
    }
    assert(!pc_material_motion(.5f,.5f,0,NULL));
    printf("PASS: %u forced/damped material partitions, weak-drag limits and untouched fallback\n",cases);
}
