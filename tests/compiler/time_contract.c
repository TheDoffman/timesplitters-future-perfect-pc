#include "guest_time.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"time line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void) {
    unsigned char row[40],input[40],output[40],before[40];size_t count;unsigned cases=0;
    const unsigned char epoch[16]={0x41,6,1,0,1,0};
    const unsigned char max_fields[16]={0x6c,0x78,9,0,14,0,2,0,48,0,5,0,0xdd,1,4,0};
    unsigned char ticks[8]={0};unsigned i;
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdin),_O_BINARY)!=-1);
#endif
    memset(output,0xa5,sizeof(output));CHECK(tsfp_time_fields_to_ticks(epoch,16,output+1,8));
    for(i=1;i<9;++i)CHECK(!output[i]);CHECK(output[0]==0xa5 && output[9]==0xa5);
    CHECK(tsfp_time_ticks_to_fields(ticks,8,output+1,16));CHECK(!memcmp(output+1,epoch,14) && output[15]==1);
    memset(ticks,0xff,8);ticks[7]=0x7f;CHECK(tsfp_time_ticks_to_fields(ticks,8,output+1,16));CHECK(!memcmp(output+1,max_fields,16));
    ticks[7]=0x80;memcpy(before,output,40);CHECK(!tsfp_time_ticks_to_fields(ticks,8,output+1,16));CHECK(!memcmp(before,output,40));
    CHECK(!tsfp_time_ticks_to_fields(NULL,8,output,16));CHECK(!tsfp_time_ticks_to_fields(ticks,7,output,16));
    CHECK(!tsfp_time_ticks_to_fields(ticks,8,NULL,16));CHECK(!tsfp_time_ticks_to_fields(ticks,8,output,15));
    CHECK(!tsfp_time_fields_to_ticks(NULL,16,output,8));CHECK(!tsfp_time_fields_to_ticks(epoch,15,output,8));
    CHECK(!tsfp_time_fields_to_ticks(epoch,16,NULL,8));CHECK(!tsfp_time_fields_to_ticks(epoch,16,output,7));
    CHECK(!memcmp(before,output,40));
    while((count=fread(row,1,40,stdin))!=0) {
        int success;size_t in_bytes=row[0]?8:16,out_bytes=row[0]?16:8;
        CHECK(count==40);memset(input,0xa5,40);memcpy(input+1,row+8,in_bytes);
        memset(output,0xa5,40);memcpy(before,output,40);
        success=row[0]?tsfp_time_ticks_to_fields(input+1,in_bytes,output+1,out_bytes):
            tsfp_time_fields_to_ticks(input+1,in_bytes,output+1,out_bytes);
        CHECK(success==row[1]);CHECK(!memcmp(input+1,row+8,in_bytes));
        if(success)CHECK(!memcmp(output+1,row+24,out_bytes));else CHECK(!memcmp(before,output,40));
        CHECK(output[0]==0xa5 && output[out_bytes+1]==0xa5);
        /* Same storage and partial overlap both consume input before publishing. */
        if(success) {
            CHECK(row[0]?tsfp_time_ticks_to_fields(input+1,in_bytes,input+1,out_bytes):tsfp_time_fields_to_ticks(input+1,in_bytes,input+1,out_bytes));
            CHECK(!memcmp(input+1,row+24,out_bytes));memcpy(input+1,row+8,in_bytes);
            CHECK(row[0]?tsfp_time_ticks_to_fields(input+1,in_bytes,input+3,out_bytes):tsfp_time_fields_to_ticks(input+1,in_bytes,input+3,out_bytes));
            CHECK(!memcmp(input+3,row+24,out_bytes));
        }
        ++cases;
    }
    CHECK(!ferror(stdin) && cases>100000);printf("PASS: %u independent calendar vectors, overlap, guards and failure preservation\n",cases);return 0;
}
