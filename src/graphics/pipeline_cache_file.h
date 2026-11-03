#ifndef TSFP_PIPELINE_CACHE_FILE_H
#define TSFP_PIPELINE_CACHE_FILE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Disk envelope is deliberately separate from the opaque driver's bytes.
 * Reject raw old files: they can be regenerated, but cannot be verified. */
#define TSFP_PIPELINE_CACHE_LIMIT (256u * 1024u * 1024u)
static uint64_t tsfp_pipeline_checksum(const void *data,size_t size)
{
    const unsigned char *p=(const unsigned char *)data;
    uint64_t hash=UINT64_C(14695981039346656037);
    while(size--){hash^=*p++;hash*=UINT64_C(1099511628211);}
    return hash;
}
static void tsfp_pipeline_u64(unsigned char *p,uint64_t value)
{for(unsigned i=0;i<8;i++){p[i]=(unsigned char)value;value>>=8;}}
static uint64_t tsfp_pipeline_read_u64(const unsigned char *p)
{uint64_t value=0;for(unsigned i=0;i<8;i++)value|=(uint64_t)p[i]<<(i*8);return value;}
static int tsfp_pipeline_file_write(FILE *f,const void *data,size_t size)
{
    unsigned char header[24]={ 'T','S','P','C',1,0,0,0 };
    if(!f||!data||size<32||size>=TSFP_PIPELINE_CACHE_LIMIT)return 0;
    tsfp_pipeline_u64(header+8,size);
    tsfp_pipeline_u64(header+16,tsfp_pipeline_checksum(data,size));
    return fwrite(header,1,sizeof header,f)==sizeof header && fwrite(data,1,size,f)==size;
}
static void *tsfp_pipeline_file_read(FILE *f,size_t *bytes)
{
    static const unsigned char magic[8]={'T','S','P','C',1,0,0,0};
    unsigned char header[24];uint64_t size;void *data;
    if(!bytes)return NULL;
    *bytes=0;
    if(!f||fread(header,1,sizeof header,f)!=sizeof header||memcmp(header,magic,8))return NULL;
    size=tsfp_pipeline_read_u64(header+8);
    if(size<32||size>=TSFP_PIPELINE_CACHE_LIMIT||size>SIZE_MAX)return NULL;
    data=malloc((size_t)size);if(!data)return NULL;
    if(fread(data,1,(size_t)size,f)!=(size_t)size||fgetc(f)!=EOF||ferror(f)||
       tsfp_pipeline_checksum(data,(size_t)size)!=tsfp_pipeline_read_u64(header+16)){
        free(data);return NULL;
    }
    *bytes=(size_t)size;return data;
}
#endif
