#ifndef TSFP_PC_PRESENT_CAPTURE_H
#define TSFP_PC_PRESENT_CAPTURE_H
/* Opt-in diagnostic capture of the completed common-renderer frame, including
 * host overlays. Called only on the render thread, immediately before present.
 * The caller supplies an output prefix; only its two rotating BMPs are written. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static inline void pc_capture_store32(unsigned char *p,uint32_t value)
{for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
static inline void pc_present_capture(int (*readback)(void *,uint32_t,uint32_t *,uint32_t *))
{
    static int configured;static const char *prefix;static unsigned every=6,slot;
    static uint64_t frames;static unsigned char *pixels;static size_t allocated;
    if(!configured) {
        configured=1;prefix=getenv("TSFP_PRESENT_CAPTURE");
        const char *e=getenv("TSFP_PRESENT_CAPTURE_EVERY");
        if(e && *e) {char *end;unsigned long n=strtoul(e,&end,10);if(!*end && n>=1 && n<=600)every=(unsigned)n;}
    }
    if(!prefix || !*prefix || !readback || ++frames%every)return;
    uint32_t w=0,h=0;
    if(readback(NULL,0,&w,&h) || !w || !h || w>8192 || h>8192 || (uint64_t)w*h>16u*1024u*1024u)return;
    const size_t bytes=(size_t)w*h*4;
    if(bytes>allocated) {
        unsigned char *next=(unsigned char *)realloc(pixels,bytes);if(!next)return;
        pixels=next;allocated=bytes;
    }
    uint32_t actual_w=w,actual_h=h;
    if(readback(pixels,w*4,&actual_w,&actual_h) || actual_w!=w || actual_h!=h)return;
    char path[4096],temporary[4100];
    int n=snprintf(path,sizeof path,"%s%03u.bmp",prefix,slot);
    if(n<0 || (size_t)n>=sizeof path)return;
    snprintf(temporary,sizeof temporary,"%s.tmp",path);
    const uint32_t pitch=(w*3+3)&~3u;
    unsigned char *row=(unsigned char *)calloc(pitch,1);if(!row)return;
    FILE *file=fopen(temporary,"wb");if(!file){free(row);return;}
    unsigned char header[54]={0};header[0]='B';header[1]='M';
    pc_capture_store32(header+2,54+pitch*h);pc_capture_store32(header+10,54);
    pc_capture_store32(header+14,40);pc_capture_store32(header+18,w);pc_capture_store32(header+22,h);
    header[26]=1;header[28]=24;
    int ok=fwrite(header,1,sizeof header,file)==sizeof header;
    for(uint32_t y=h;ok && y--;) {
        const unsigned char *source=pixels+(size_t)y*w*4;
        for(uint32_t x=0;x<w;x++){row[x*3]=source[x*4+2];row[x*3+1]=source[x*4+1];row[x*3+2]=source[x*4];}
        ok=fwrite(row,1,pitch,file)==pitch;
    }
    if(fclose(file))ok=0;free(row);
    if(ok) {remove(path);if(!rename(temporary,path))slot^=1u;else remove(temporary);}
    else remove(temporary);
}
#endif
