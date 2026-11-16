#include "mod_wave.h"
#include <string.h>

static uint16_t read16(const uint8_t *p)
{return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}
static uint32_t read32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}

TsfpModWaveError tsfp_mod_wave_parse(const void *input,size_t size,TsfpModWave *output)
{
    const uint8_t *data=(const uint8_t *)input,*format=NULL,*pcm=NULL;
    size_t at=12;uint32_t chunks=0,pcm_size=0,channels,rate,align,frames;
    TsfpModWave result;
    if(!output)return TSFP_MOD_WAVE_ARGUMENT;
    memset(output,0,sizeof *output);
    if(!data)return TSFP_MOD_WAVE_ARGUMENT;
    if(size>TSFP_MOD_WAVE_MAX_BYTES)return TSFP_MOD_WAVE_LIMIT;
    if(size<12 || memcmp(data,"RIFF",4) || memcmp(data+8,"WAVE",4) ||
       (size_t)read32(data+4)!=size-8)return TSFP_MOD_WAVE_CONTAINER;
    while(at<size) {
        const uint8_t *name,*body;uint32_t length;
        if(++chunks>TSFP_MOD_WAVE_MAX_CHUNKS)return TSFP_MOD_WAVE_LIMIT;
        if(size-at<8)return TSFP_MOD_WAVE_CHUNK;
        name=data+at;length=read32(name+4);at+=8;
        if((size_t)length>size-at)return TSFP_MOD_WAVE_CHUNK;
        body=data+at;
        if(!memcmp(name,"fmt ",4)) {
            /* PCM's optional WAVEFORMATEX extension must be exactly empty. */
            if(format || (length!=16 && length!=18) || (length==18 && read16(body+16)))
                return TSFP_MOD_WAVE_FORMAT;
            format=body;
        }else if(!memcmp(name,"data",4)) {
            if(!format || pcm || !length)return TSFP_MOD_WAVE_DATA;
            pcm=body;pcm_size=length;
        }
        at+=(size_t)length;
        if(length&1u){if(at==size)return TSFP_MOD_WAVE_CHUNK;at++;}
    }
    if(!format || !pcm)return TSFP_MOD_WAVE_DATA;
    channels=read16(format+2);rate=read32(format+4);align=read16(format+12);
    if(read16(format)!=1 || (channels!=1 && channels!=2) || rate<8000 || rate>48000 ||
       read16(format+14)!=16 || align!=channels*2u || read32(format+8)!=rate*align)
        return TSFP_MOD_WAVE_FORMAT;
    if(pcm_size%align)return TSFP_MOD_WAVE_DATA;
    frames=pcm_size/align;
    if(frames>rate*TSFP_MOD_WAVE_MAX_SECONDS)return TSFP_MOD_WAVE_LIMIT;
    memset(&result,0,sizeof result);result.pcm=pcm;result.bytes=pcm_size;
    result.frames=frames;result.rate=rate;result.channels=channels;
    *output=result;return TSFP_MOD_WAVE_OK;
}

const char *tsfp_mod_wave_error(TsfpModWaveError error)
{
    switch(error) {
    case TSFP_MOD_WAVE_OK:return "Valid PCM WAV";
    case TSFP_MOD_WAVE_ARGUMENT:return "Missing WAV snapshot or output";
    case TSFP_MOD_WAVE_CONTAINER:return "Expected an exact RIFF/WAVE container";
    case TSFP_MOD_WAVE_CHUNK:return "Truncated WAV chunk or missing padding";
    case TSFP_MOD_WAVE_FORMAT:return "Expected one PCM16 mono/stereo format at 8 to 48 kHz";
    case TSFP_MOD_WAVE_DATA:return "Expected one nonempty frame-aligned data chunk after fmt";
    case TSFP_MOD_WAVE_LIMIT:return "WAV exceeds the byte, chunk or duration limit";
    default:return "Invalid WAV error code";
    }
}
