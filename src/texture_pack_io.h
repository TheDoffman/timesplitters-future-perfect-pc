/* Shared replacement validation; renderer adapters own only GPU resources. */
#ifndef TSFP_TEXTURE_PACK_IO_H
#define TSFP_TEXTURE_PACK_IO_H
#include "texture_pack_format.h"
#include <stdio.h>
#include <stdlib.h>
#define TSFP_PACK_RESIDENT_BUDGET (UINT64_C(512)*1024*1024)
static unsigned char *tsfp_pack_read(const char *path,uint32_t w,uint32_t h,
                                     uint64_t remaining,TsfpDDS *dds) {
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    long length;
    if(fseek(f,0,SEEK_END)||(length=ftell(f))<128||length>24*1024*1024||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *data=malloc((size_t)length);
    if(!data){fclose(f);return NULL;}
    int ok=fread(data,1,(size_t)length,f)==(size_t)length;fclose(f);
    if(!ok||!tsfp_dds_parse(data,(size_t)length,dds)||
       (uint64_t)dds->width*h!=(uint64_t)dds->height*w||dds->width<w||dds->height<h||dds->bytes>remaining){free(data);return NULL;}
    return data;
}
#endif
