/* Original leaf adapters; shared descriptor/allocation policy lives elsewhere. */
#include "host_text_codec.h"
#include <limits.h>
#if defined(_WIN32) && !defined(TSFP_TEST_LATIN1_CODEC)
#include <windows.h>
_Static_assert(sizeof(WCHAR)==2,"Windows codec uses 16-bit code units");
static uint32_t host_unit(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
static int host_convert(void *host,unsigned direction,const unsigned char *source,uint32_t bytes,unsigned char *out,uint32_t capacity,uint32_t *written) {
    tsfp_host_text_codec *state=host;int result;
    if(!written || !state || direction>TSFP_TEXT_TO_ANSI || bytes>INT_MAX)return 0;
    if(!bytes){*written=0;return 1;}
    if(!source || (direction==TSFP_TEXT_TO_ANSI && bytes%2))return 0;
    if(direction==TSFP_TEXT_TO_UNICODE) {
        if(out && capacity/2>INT_MAX)return 0;
        result=MultiByteToWideChar(state->codepage,0,(const char*)source,(int)bytes,(WCHAR*)out,out?(int)(capacity/2):0);
        if(result<=0)return 0;
        *written=(uint32_t)result*2;
    } else {
        if(out && capacity>INT_MAX)return 0;
        result=WideCharToMultiByte(state->codepage,0,(const WCHAR*)source,(int)(bytes/2),(char*)out,out?(int)capacity:0,NULL,NULL);
        if(result<=0)return 0;
        *written=(uint32_t)result;
    }
    return 1;
}
static uint32_t host_prefix(void *host,const unsigned char *source,uint32_t bytes,uint32_t limit) {
    tsfp_host_text_codec *state=host;uint32_t low=0,high=bytes/2,written=0;
    if(!state || bytes%2)return UINT32_MAX;
    while(low<high) {
        uint32_t middle=low+(high-low+1)/2,count=middle;
        if((state->codepage==65001 || state->codepage==54936) && count && count<bytes/2 &&
           host_unit(source+(count-1)*2)>=0xd800 && host_unit(source+(count-1)*2)<=0xdbff &&
           host_unit(source+count*2)>=0xdc00 && host_unit(source+count*2)<=0xdfff)--count;
        if(!host_convert(host,TSFP_TEXT_TO_ANSI,source,count*2,NULL,0,&written))return UINT32_MAX;
        if(written<=limit)low=middle;else high=middle-1;
    }
    if((state->codepage==65001 || state->codepage==54936) && low && low<bytes/2 &&
       host_unit(source+(low-1)*2)>=0xd800 && host_unit(source+(low-1)*2)<=0xdbff &&
       host_unit(source+low*2)>=0xdc00 && host_unit(source+low*2)<=0xdfff)--low;
    return host_convert(host,TSFP_TEXT_TO_ANSI,source,low*2,NULL,0,&written)?written:UINT32_MAX;
}
#else
static int host_convert(void *host,unsigned direction,const unsigned char *source,uint32_t bytes,unsigned char *out,uint32_t capacity,uint32_t *written) {
    uint32_t needed;(void)host;
    if(!written || direction>TSFP_TEXT_TO_ANSI || (bytes && !source) || (direction==TSFP_TEXT_TO_ANSI && bytes%2))return 0;
    if(direction==TSFP_TEXT_TO_UNICODE && bytes>UINT32_MAX/2)return 0;
    needed=direction==TSFP_TEXT_TO_UNICODE?bytes*2:bytes/2;
    if(out && capacity<needed)return 0;
    if(out)for(uint32_t i=0;i<needed;++i) {
        if(direction==TSFP_TEXT_TO_UNICODE)out[i]=i%2?0:source[i/2];
        else {uint32_t unit=(uint32_t)source[i*2]|((uint32_t)source[i*2+1]<<8);out[i]=(unsigned char)(unit<=255?unit:'?');}
    }
    *written=needed;return 1;
}
static uint32_t host_prefix(void *host,const unsigned char *source,uint32_t bytes,uint32_t limit) {
    (void)host;(void)source;return bytes%2?UINT32_MAX:(bytes/2<limit?bytes/2:limit);
}
#endif
void tsfp_host_text_codec_init(tsfp_host_text_codec *state,tsfp_text_codec *codec) {
#if defined(_WIN32) && !defined(TSFP_TEST_LATIN1_CODEC)
    state->codepage=GetACP();
#else
    state->codepage=0;
#endif
    codec->host=state;codec->convert=host_convert;codec->ansi_prefix=host_prefix;
}
