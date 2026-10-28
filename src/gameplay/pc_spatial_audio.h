/* Stereo projection of the XDK's source/listener positions and orientation.
 * Distance, cone, mix-bin and headroom attenuation stay in the guest mixer. */
#ifndef RECOMP_AUDIO_SPATIAL_H
#define RECOMP_AUDIO_SPATIAL_H
#include <math.h>
static inline float recomp_spatial_pan(const float relative[3], const float front[3],
                                      const float up[3], unsigned mode)
{
    double length=0, side=0, right[3], norm=0;
    if(mode>1)return 0;
    for(int i=0;i<3;i++) {
        if(!isfinite(relative[i]) || !isfinite(front[i]) || !isfinite(up[i]))return 0;
        length+=(double)relative[i]*relative[i];
    }
    if(length<1e-12)return 0;
    if(mode==1)side=relative[0]; /* head-relative: +X is the listener's right */
    else {
        right[0]=(double)up[1]*front[2]-(double)up[2]*front[1];
        right[1]=(double)up[2]*front[0]-(double)up[0]*front[2];
        right[2]=(double)up[0]*front[1]-(double)up[1]*front[0];
        for(int i=0;i<3;i++){norm+=right[i]*right[i];side+=right[i]*relative[i];}
        if(norm<1e-12)return 0;
        side/=sqrt(norm);
    }
    double distance=sqrt(length);
    /* First-person muzzle/foley sounds are placed beside the camera (the US
       dual pistols measured 0.10 units). Do not turn that tiny offset into
       a hard left/right event. Blend into world direction from 1 to 2 units. */
    double width=fmax(0.0,fmin(1.0,distance-1.0));
    double pan=width*side/distance;
    return (float)fmax(-1.0,fmin(1.0,pan));
}
static inline float recomp_weapon_sound_range(float range)
{ return isfinite(range) && range>0 && range<=200.f ? range*3.f : range; }
static inline void recomp_spatial_gains(float pan, float *left, float *right)
{
    if(!isfinite(pan))pan=0;
    pan=fmaxf(-1,fminf(1,pan));
    *left=sqrtf(0.5f*(1-pan));*right=sqrtf(0.5f*(1+pan));
}
#endif
