#ifndef PC_AUDIO_GAIN_H
#define PC_AUDIO_GAIN_H
#include <stdint.h>
#include <math.h>
/* DirectSound hundredths-of-dB attenuation. The minimum means silence,
 * rather than a small residual gain. Both output adapters use this contract. */
static inline float pc_audio_db_gain(int32_t level)
{
    if(level<=-10000)return 0.f;
    if(level>=0)return 1.f;
    return powf(10.f,level/2000.f);
}
#endif
