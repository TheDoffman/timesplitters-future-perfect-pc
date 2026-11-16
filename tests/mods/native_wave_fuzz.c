#include "mod_wave.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t state=0xc1a05517u;
static uint32_t random32(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void)
{
    static const uint8_t valid[]={
        'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,0x40,0x1f,0,0,0x80,0x3e,0,0,2,0,16,0,'d','a','t','a',2,0,0,0,0,0};
    uint8_t storage[257],*bytes=storage+1;unsigned i;
    for(i=0;i<200000;i++) {
        size_t size;unsigned edits,j;TsfpModWave output,zero;
        memset(bytes,0,256);memcpy(bytes,valid,sizeof valid);
        edits=random32()%12u;
        for(j=0;j<edits;j++)bytes[random32()%256u]=(uint8_t)random32();
        size=i%3u?sizeof valid:(size_t)(random32()%257u);
        if(i%4u==0 && size>=12) {
            uint32_t declared=(uint32_t)size-8u;
            bytes[4]=(uint8_t)declared;bytes[5]=(uint8_t)(declared>>8);
            bytes[6]=(uint8_t)(declared>>16);bytes[7]=(uint8_t)(declared>>24);
        }
        memset(&output,0xff,sizeof output);memset(&zero,0,sizeof zero);
        if(tsfp_mod_wave_parse(bytes,size,&output)==TSFP_MOD_WAVE_OK) {
            assert(output.pcm>=bytes && output.pcm+output.bytes<=bytes+size);
            assert(output.channels>=1 && output.channels<=2 && output.rate>=8000 && output.rate<=48000);
            assert(output.bytes==output.frames*output.channels*2u);
        }else assert(!memcmp(&output,&zero,sizeof output));
    }
    puts("PASS native WAV parser: 200000 mutations");return 0;
}
