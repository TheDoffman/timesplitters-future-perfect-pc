/* Independent mathematical and native SSE references for shader byte anchors. */
#include <assert.h>
#include <fenv.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "pc_surface_bias.h"
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#define HAVE_NATIVE_SSE 1
#endif

static unsigned reference(float scaled,int mode) {
    double lower=floor((double)scaled),fraction=(double)scaled-lower;
    if(mode==FE_UPWARD)return (unsigned)ceil((double)scaled);
    if(mode==FE_DOWNWARD || mode==FE_TOWARDZERO)return (unsigned)lower;
    return (unsigned)lower+(fraction>.5 || (fraction==.5 && ((unsigned)lower&1u)));
}
int main(void) {
    int saved=fegetround();assert(saved!=-1 && !fesetround(FE_TONEAREST));
    float inputs[800]={-.1f,0.f,.105f,.65f,.98f,.5f,1.f,1.1f,INFINITY,-INFINITY,NAN};
    unsigned count=11;
    /* Check both sides of every byte midpoint, including ties to even. */
    for(unsigned i=0;i<255;i++) {
        float midpoint=(i+.5f)/255.f;
        inputs[count++]=nextafterf(midpoint,0.f);
        inputs[count++]=midpoint;
        inputs[count++]=nextafterf(midpoint,1.f);
    }
    int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for(unsigned m=0;m<4;m++) {
        assert(!fesetround(modes[m]));
        for(unsigned i=0;i<count;i++) {
            volatile float input=inputs[i];
            float clamped=fminf(1.f,fmaxf(0.f,input));
            volatile float scaled=clamped*255.f;
            unsigned expected=reference(scaled,modes[m]);
            unsigned actual=pc_surface_coefficient_byte(input);
            if(actual!=expected) {
                fprintf(stderr,"coefficient mismatch mode=%u input=%u got=%u expected=%u\n",m,i,actual,expected);
                return 1;
            }
#ifdef HAVE_NATIVE_SSE
            __m128 value=_mm_min_ss(_mm_max_ss(_mm_set_ss(input),_mm_set_ss(0.f)),_mm_set_ss(1.f));
            unsigned hardware=(unsigned)_mm_cvtss_si32(_mm_mul_ss(value,_mm_set_ss(255.f)));
            if(actual!=hardware) {
                fprintf(stderr,"native CVTSS2SI mismatch mode=%u input=%u got=%u expected=%u\n",m,i,actual,hardware);
                return 2;
            }
#endif
            assert(pc_surface_packed_coefficient(input)==expected/255.f);
        }
    }
    assert(!fesetround(FE_TONEAREST));
    assert(pc_surface_coefficient_byte(.105f)==27);
    assert(pc_surface_coefficient_byte(.65f)==166);
    assert(pc_surface_coefficient_byte(.98f)==250);
    assert(pc_surface_coefficient_byte(.5f)==128);
    assert(!fesetround(saved));
    puts("PASS: byte anchors match mathematical rounding and available native SSE in all four modes");
    return 0;
}
