/* Shared replacement validation; renderer adapters own only GPU resources. */
#ifndef TSFP_TEXTURE_PACK_IO_H
#define TSFP_TEXTURE_PACK_IO_H
#include "texture_pack_format.h"
#include "texture_pack_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PACK_RESIDENT_BUDGET (UINT64_C(512)*1024*1024)
static unsigned char *tsfp_pack_read_file_traced(const char *path,uint32_t w,uint32_t h,
        uint64_t remaining,TsfpDDS *dds,TsfpTextureTrace *trace) {
    TSFP_TEXTURE_STAMP(trace,read_begin);
    FILE *f=fopen(path,"rb");TSFP_TEXTURE_STAMP(trace,open_end);
    if(!f)return NULL;
    long length;
    if(fseek(f,0,SEEK_END)||(length=ftell(f))<128||length>24*1024*1024||fseek(f,0,SEEK_SET)){
        fclose(f);TSFP_TEXTURE_STAMP(trace,read_end);return NULL;}
    unsigned char *data=malloc((size_t)length);
    if(!data){fclose(f);TSFP_TEXTURE_STAMP(trace,read_end);return NULL;}
    int ok=fread(data,1,(size_t)length,f)==(size_t)length;fclose(f);
    TSFP_TEXTURE_STAMP(trace,read_end);
    int valid=ok&&tsfp_dds_parse(data,(size_t)length,dds)&&
       (uint64_t)dds->width*h==(uint64_t)dds->height*w&&dds->width>=w&&dds->height>=h&&dds->bytes<=remaining;
    TSFP_TEXTURE_STAMP(trace,validated);
    if(!valid){free(data);return NULL;}
    return data;
}
/* An activated mod overlay contains only verified, flat texture-key filenames.
 * Selection happens on the same worker as the ordinary pack read. The main
 * pack is never modified or copied. Missing/invalid mod payloads fall back to
 * its original replacement path, and Classic never dispatches these reads. */
static unsigned char *tsfp_pack_read_traced(const char *path,uint32_t w,uint32_t h,
        uint64_t remaining,TsfpDDS *dds,TsfpTextureTrace *trace) {
    const char *root=getenv("TSFP_MOD_TEXTURE_DIR");
    if(root && *root) {
        const char *base=path;
        for(const char *at=path;*at;at++)if(*at=='/' || *at=='\\')base=at+1;
        int valid=strlen(base)==20 && strcmp(base+16,".dds")==0;
        for(unsigned i=0;valid && i<16;i++)
            if(!((base[i]>='0' && base[i]<='9') || (base[i]>='a' && base[i]<='f')))valid=0;
        if(valid) {
            char mod_path[4096];
            int length=snprintf(mod_path,sizeof mod_path,"%s/%s",root,base);
            if(length>0 && length<(int)sizeof mod_path && strcmp(mod_path,path)) {
                unsigned char *data=tsfp_pack_read_file_traced(mod_path,w,h,remaining,dds,trace);
                if(data)return data;
            }
        }
    }
    return tsfp_pack_read_file_traced(path,w,h,remaining,dds,trace);
}
static unsigned char *tsfp_pack_read(const char *path,uint32_t w,uint32_t h,
        uint64_t remaining,TsfpDDS *dds) {
    return tsfp_pack_read_traced(path,w,h,remaining,dds,NULL);
}
#endif
