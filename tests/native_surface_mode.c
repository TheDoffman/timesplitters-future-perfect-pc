#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "graphics/surface_mode.h"

/* Independent scalar execution of the packed GPU inputs. GPU tests compile the
 * shared shader itself; this portable test covers eigenvalue branches and ABI. */
static void evolve(const PcSurfaceMode *m,double damping,double delta,float residual,float state[2])
{
    float t=(float)delta,scale=(float)pow(damping,delta*.5),angle=fabsf(m->eigen[3])*t;
    float center=m->eigen[3]==0?1:m->eigen[3]>0?cosf(angle):coshf(angle);
    float amount=m->eigen[3]==0?t:m->eigen[3]>0?sinf(angle):sinhf(angle);
    float p00=scale*(center+amount*m->eigen[0]),p01=scale*amount*m->eigen[1];
    float p10=scale*amount*m->eigen[2],p11=scale*(center-amount*m->eigen[0]);
    float q0=m->fixed[0]+m->fixed[2]*residual,q1=m->fixed[1]+m->fixed[3]*residual;
    float next[2]={p00*state[0]+p01*state[1]+(1-p00)*q0-p01*q1,
                  p10*state[0]+p11*state[1]+(1-p11)*q1-p10*q0};
    memcpy(state,next,sizeof next);
}
int main(void)
{
    const double a=26./255,b=165./255;
    const double dampings[]={.2,.5,249./255,1};
    const double deltas[]={0,.00001,.17,.413,.999,1};
    double worst=0;unsigned real=0,repeated=0,complex=0;
    for(unsigned di=0;di<4;di++)for(unsigned mode=0;mode<68;mode++) {
        double d=dampings[di],r=sqrt(d),critical=(1-r)*(1-r)/(d*a*b);
        double k=mode<65?43./255+8.*mode/64:critical*(mode==65?.25:mode==66?1:1.2);
        if(k<=0)k=.17;
        for(unsigned variant=0;variant<3;variant++) {
            double force=.5*k,bias=.001*(variant-1.);float residual=.01f*((int)variant-1);
            PcSurfaceMode m;assert(pc_surface_mode(a,b,d,k,force,bias,&m));
            real+=m.eigen[3]<0;repeated+=m.eigen[3]==0;complex+=m.eigen[3]>0;
            for(unsigned ti=0;ti<6;ti++) {
                float value[2]={.01f,.61f};PcSurfaceCoupledStep ref;
                assert(pc_surface_coupled_step(a,b,d,k,force+residual,bias,deltas[ti],&ref));
                double expected[2];
                for(unsigned i=0;i<2;i++)expected[i]=ref.row[i][0]*value[0]+ref.row[i][1]*value[1]+ref.row[i][2];
                evolve(&m,d,deltas[ti],residual,value);
                for(unsigned i=0;i<2;i++) {double e=fabs(value[i]-expected[i]);if(e>worst)worst=e;assert(e<1e-5);}
            }
            float split[2]={.01f,.61f};
            double v=split[0]+a*(force+residual-k*split[1]);
            double expected[2]={v+bias,d*(split[1]+b*v)};
            const double pieces[]={.413,.001,.407,.179};
            for(unsigned i=0;i<4;i++)evolve(&m,d,pieces[i],residual,split);
            assert(fabs(split[0]-expected[0])<1e-5 && fabs(split[1]-expected[1])<1e-5);
        }
    }
    assert(real && repeated && complex);
    PcSurfaceMode out,before;memset(&out,0x5a,sizeof out);before=out;
    assert(!pc_surface_mode(a,b,0,1,1,0,&out));
    assert(!pc_surface_mode(a,b,1,-1,1,0,&out));
    assert(!pc_surface_mode(a,b,1,1,NAN,0,&out));
    assert(!pc_surface_mode(a,b,1,100,1,0,&out));
    assert(!pc_surface_mode(a,b,1,1e-100,0,0,&out)); /* Packed fixed point overflows. */
    assert(!pc_surface_mode(a,b,1,1,1,0,NULL));
    assert(!memcmp(&out,&before,sizeof out));
    printf("PASS: immutable mode packing; real=%u repeated=%u complex=%u max_error=%.9g\n",real,repeated,complex,worst);
    return 0;
}
