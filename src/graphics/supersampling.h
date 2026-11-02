#ifndef TSFP_SUPERSAMPLING_H
#define TSFP_SUPERSAMPLING_H
#include <stdint.h>

/* Indices are persisted; 0 retains the original rendering path. Percentages
 * scale both axes: 200% is four times the pixels. Never change output shape. */
static inline unsigned tsfp_supersampling_percent(int choice)
{
    static const unsigned percent[]={100,125,150,200};
    return choice>=0 && choice<4 ? percent[choice] : 100;
}

/* Bound scene allocation independently of output resolution. Fall back in
 * whole presets, without rewriting the requested preference. Existing larger
 * base targets remain untouched at 100%. Integer rounding is deterministic. */
static inline int tsfp_supersampling_size(unsigned w,unsigned h,int choice,
                                         unsigned *out_w,unsigned *out_h)
{
    if(choice<0 || choice>3)choice=0;
    while(choice>0) {
        unsigned p=tsfp_supersampling_percent(choice);
        uint64_t sw=((uint64_t)w*p+99)/100,sh=((uint64_t)h*p+99)/100;
        if(w && h && sw<=7680 && sh<=4320 && sw*sh<=33177600) {
            if(out_w)*out_w=(unsigned)sw;
            if(out_h)*out_h=(unsigned)sh;
            return choice;
        }
        --choice;
    }
    if(out_w)*out_w=w;
    if(out_h)*out_h=h;
    return 0;
}
#endif
