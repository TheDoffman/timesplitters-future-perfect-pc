#include <math.h>
#include <stdio.h>
#include <string.h>
#include "graphics/surface_coupled.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)

static void step(const PcSurfaceCoupledStep *m,double state[2])
{
    double next[2];
    for(unsigned i=0;i<2;i++)next[i]=m->row[i][0]*state[0]+m->row[i][1]*state[1]+m->row[i][2];
    memcpy(state,next,sizeof next);
}
int main(void)
{
    const double a=26./255, b=165./255, d=249./255, bias=128./255-.5;
    const unsigned rates[]={60,120,144,180,240};
    /* Include the fixed-neighbor fixture and all clamped diagonal-stencil
     * spectral restoring strengths, not just the uniform/low-frequency mode. */
    double max_error=0;
    for(unsigned mode=0;mode<=64;mode++) {
        double k=mode==64?1.:43./255+8.*mode/63;
        PcSurfaceCoupledStep original;
        CHECK(pc_surface_coupled_step(a,b,d,k,.5,bias,1,&original));
        double reference[2]={.01,.65};
        for(unsigned tick=0;tick<60;tick++)step(&original,reference);
        for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) {
            unsigned fps=rates[rate];PcSurfaceCoupledStep fractional;
            CHECK(pc_surface_coupled_step(a,b,d,k,.5,bias,60./fps,&fractional));
            double value[2]={.01,.65};
            for(unsigned frame=0;frame<fps;frame++)step(&fractional,value);
            for(unsigned i=0;i<2;i++) {
                double error=fabs(value[i]-reference[i]);
                if(error>max_error)max_error=error;
                CHECK(error<.00003);
            }
        }
        PcSurfaceCoupledStep first,second;
        CHECK(pc_surface_coupled_step(a,b,d,k,.5,bias,.17,&first));
        CHECK(pc_surface_coupled_step(a,b,d,k,.5,bias,.83,&second));
        double split[2]={-.03,.62},whole[2]={-.03,.62};
        step(&first,split);step(&second,split);step(&original,whole);
        CHECK(fabs(split[0]-whole[0])<.000001 && fabs(split[1]-whole[1])<.000001);
    }
    /* Exercise real, repeated and complex eigenvalues independently of the
     * authored range. Compare with the original equations, not delta=1 output
     * from the helper, so a broken one-tick branch cannot mask a regression. */
    const double dampings[]={.2,.5,.98,1};
    for(unsigned di=0;di<4;di++)for(unsigned mode=0;mode<3;mode++) {
        double damp=dampings[di],r=sqrt(damp);
        double critical=(1-r)*(1-r)/(damp*a*b);
        double restore=critical>0?critical*(mode==0?.25:mode==1?1:1.2):.17;
        double force=.5*restore,offset=.001;
        for(unsigned rate=0;rate<sizeof rates/sizeof rates[0];rate++) {
            PcSurfaceCoupledStep fractional;
            CHECK(pc_surface_coupled_step(a,b,damp,restore,force,offset,60./rates[rate],&fractional));
            double reference[2]={.01,.65},value[2]={.01,.65};
            for(unsigned tick=0;tick<60;tick++) {
                double v=reference[0]+a*(force-restore*reference[1]);
                reference[0]=v+offset;reference[1]=damp*(reference[1]+b*v);
            }
            for(unsigned frame=0;frame<rates[rate];frame++)step(&fractional,value);
            for(unsigned i=0;i<2;i++)CHECK(fabs(value[i]-reference[i])<.00003);
        }
    }
    PcSurfaceCoupledStep identity,untouched,before;
    /* Original first-pass rounding is a held input. Verify both state entries
     * against an independent full-step equation, across authored spatial modes.
     * Recompute the fractional operator for pieces that cross a 144-Hz frame. */
    for(unsigned mode=0;mode<65;mode++) {
        double restore=43./255+8.*mode/64,force=.5*restore;
        for(unsigned variant=0;variant<3;variant++) {
            double residual=(variant-1.)/510.;
            double value[2]={.01,.61},expected[2];
            double v=value[0]+a*(force+residual-restore*value[1]);
            expected[0]=v+bias;expected[1]=d*(value[1]+b*v);
            const double pieces[]={5./12,5./12,2./12};
            for(unsigned i=0;i<3;i++) {
                PcSurfaceCoupledStep m;
                CHECK(pc_surface_coupled_forced_step(a,b,d,restore,force,bias,pieces[i],&m));
                double old[2]={value[0],value[1]};
                for(unsigned c=0;c<2;c++)value[c]=m.row[c][0]*old[0]+m.row[c][1]*old[1]+
                    m.row[c][2]+m.row[c][3]*residual;
            }
            CHECK(fabs(value[0]-expected[0])<1e-6 && fabs(value[1]-expected[1])<1e-6);
        }
    }
    CHECK(pc_surface_coupled_step(a,b,d,1,.5,bias,0,&identity));
    CHECK(identity.row[0][0]==1 && identity.row[1][1]==1 && identity.row[0][2]==0);
    memset(&untouched,0x5a,sizeof untouched);before=untouched;
    CHECK(!pc_surface_coupled_step(a,b,d,1,.5,bias,NAN,&untouched));
    CHECK(!pc_surface_coupled_step(a,b,0,1,.5,bias,.5,&untouched));
    CHECK(!pc_surface_coupled_step(a,b,d,0,.5,bias,.5,&untouched));
    CHECK(!pc_surface_coupled_step(a,b,d,100,.5,bias,.5,&untouched));
    CHECK(!pc_surface_coupled_step(a,b,d,1,.5,bias,1.1,&untouched));
    CHECK(!memcmp(&untouched,&before,sizeof before));
    CHECK(!pc_surface_coupled_forced_step(a,b,d,1,.5,bias,NAN,&untouched));
    CHECK(!memcmp(&untouched,&before,sizeof before));
    printf("PASS: coupled fractional steps preserve 65 controlled modes; max error %.9g\n",max_error);
    return 0;
}
