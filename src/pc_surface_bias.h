#ifndef PC_SURFACE_BIAS_H
#define PC_SURFACE_BIAS_H
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
/* Anchor to the current recompiled uploader (US 3D9520): clamp then
 * truncate to a byte. Do not change the established 60-FPS coefficient. */
static inline float pc_surface_packed_coefficient(float value)
{ return (float)(int32_t)(fminf(1.f,fmaxf(0.f,value))*255.f)/255.f; }
static inline void pc_surface_coefficients(float delta,const float authored[3],float out[3])
{
    out[0]=pc_surface_packed_coefficient(authored[0])*delta;
    out[1]=pc_surface_packed_coefficient(authored[1])*delta;
    out[2]=powf(pc_surface_packed_coefficient(authored[2]),delta);
}
static inline float pc_surface_bias(float delta)
{ return .5f+(128.f/255.f-.5f)*delta; }
static inline int pc_surface_bias_requested(void)
{
    static int checked,enabled;
    if(!checked) {
        const char *bias=getenv("TSFP_SURFACE_BIAS_TEST");
        const char *fractional=getenv("TSFP_SURFACE_FRACTIONAL_TEST");
        const char *precision=getenv("TSFP_SURFACE_FLOAT_TEST");
        enabled=bias && bias[0]=='1' && fractional && fractional[0]=='1' && precision && precision[0]=='1';checked=1;
    }
    return enabled;
}
static inline int pc_surface_live_requested(void)
{
    static int checked,enabled;
    if(!checked) {
        const char *live=getenv("TSFP_SURFACE_LIVE_TEST");
        enabled=live && live[0]=='1' && pc_surface_bias_requested();checked=1;
    }
    return enabled;
}
#endif
