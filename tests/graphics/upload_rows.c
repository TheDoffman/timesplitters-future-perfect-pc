#include "upload_copy.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void check(size_t bytes,size_t rows,size_t depth,size_t pitch,size_t slice,size_t copies)
{
    size_t source_size=(depth-1)*slice+(rows-1)*pitch+(bytes<pitch?bytes:pitch);
    size_t size=bytes*rows*depth;
    uint8_t *source=malloc(source_size+32),*actual=malloc(size+32),*expected=malloc(size+32);
    assert(source && actual && expected);
    for(size_t i=0;i<source_size+32;i++)source[i]=(uint8_t)(i*31+17);
    memset(actual,0xCD,size+32);memset(expected,0xCD,size+32);
    for(size_t z=0;z<depth;z++)for(size_t y=0;y<rows;y++)
        memcpy(expected+16+(z*rows+y)*bytes,source+16+z*slice+y*pitch,bytes<pitch?bytes:pitch);
    assert(tsfp_upload_rows(actual+16,source+16,bytes,rows,depth,pitch,slice,1)==copies);
    assert(!memcmp(actual,expected,size+32));
    memset(actual,0xCD,size+32);
    assert(tsfp_upload_rows(actual+16,source+16,bytes,rows,depth,pitch,slice,0)==rows*depth);
    assert(!memcmp(actual,expected,size+32));
    free(source);free(actual);free(expected);
}
int main(void)
{
    check(1024,256,4,1024,1024*256,1);
    check(1024,256,4,1024,1024*256+17,4);
    check(13,7,3,17,17*7+11,21);
    check(17,7,3,13,13*7+11,21);
    /* Block-compressed rows and single-row/single-slice uploads. */
    check(16,4,2,16,64,1);check(16,1,1,32,32,1);
    assert(tsfp_upload_rows(NULL,NULL,0,1,1,0,0,1)==0);
    assert(tsfp_upload_rows(NULL,NULL,16,0,1,16,0,1)==0);
    assert(tsfp_upload_rows(NULL,NULL,16,1,0,16,16,1)==0);
    uint32_t shadow[4]={0},data[4]={0};int dirty=0;
    assert(!tsfp_upload_uniform(shadow,data,sizeof data,&dirty) && !dirty);
    dirty=1;assert(!tsfp_upload_uniform(shadow,data,sizeof data,&dirty) && dirty);
    dirty=0;data[1]=0x80000000u;
    assert(tsfp_upload_uniform(shadow,data,sizeof data,&dirty) && dirty && !memcmp(shadow,data,sizeof data));
    dirty=0;data[1]=0x7fc00001u;
    assert(tsfp_upload_uniform(shadow,data,sizeof data,&dirty) && dirty);
    dirty=0;assert(!tsfp_upload_uniform(shadow,data,sizeof data,&dirty) && !dirty);
    data[1]=0x7fc00002u;
    assert(tsfp_upload_uniform(shadow,data,sizeof data,&dirty) && dirty);
    puts("PASS: packed, padded/short rows, slice gaps, compressed rows and allocation sentinels");
    return 0;
}
