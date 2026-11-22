/* Read the original complete model dictionary without loading another level.
 * The Story dictionaries remain the last overrides in the original loader. */
#ifndef PC_STORY_CATALOG_H
#define PC_STORY_CATALOG_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *story_catalog_read(FILE *file,uint32_t *bytes)
{
    uint32_t header[3],row[4];long size;unsigned char *data=NULL;
    if(!file || fseek(file,0,SEEK_END) || (size=ftell(file))<12 ||
       fseek(file,0,SEEK_SET) || fread(header,1,12,file)!=12 ||
       memcmp(header,"P5CK",4) || header[1]<12 || header[2]%16 ||
       header[2]>1024u*1024u || (uint64_t)header[1]+header[2]>(uint64_t)size)return NULL;
    if(fseek(file,(long)header[1],SEEK_SET))return NULL;
    for(uint32_t i=0;i<header[2]/16u;i++) {
        if(fread(row,1,16,file)!=16)return NULL;
        if(row[0]!=0xd9e2667au)continue; /* prop/table/prop_all_xbas.raw */
        if(row[3] || row[2]<8 || row[2]>256u*1024u || row[1]<12 ||
           (uint64_t)row[1]+row[2]>(uint64_t)size ||
           ((uint64_t)row[1]< (uint64_t)header[1]+header[2] &&
            (uint64_t)row[1]+row[2]>header[1]))return NULL;
        data=(unsigned char *)malloc(row[2]);if(!data)return NULL;
        if(fseek(file,(long)row[1],SEEK_SET) || fread(data,1,row[2],file)!=row[2])break;
        uint32_t magic,pool;memcpy(&magic,data,4);memcpy(&pool,data+4,4);
        if(magic!=0x3f214482u || pool>row[2]-8u || (row[2]-8u-pool)%40u)break;
        const uint32_t count=(row[2]-8u-pool)/40u;
        if(!count || count>2512u)break;
        for(uint32_t j=0;j<count;j++) {
            uint32_t record[10];memcpy(record,data+8u+pool+j*40u,40);
            if(record[0]>=2512u)goto invalid;
            for(unsigned k=2;k<=4;k++) {
                if(k==4 && record[k]==0xffffffffu)continue;
                if(record[k]>=pool || !memchr(data+8u+record[k],0,pool-record[k]))goto invalid;
            }
        }
        *bytes=row[2];return data;
    }
invalid:
    free(data);return NULL;
}
#endif
