#include "guest_io_transfer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"transfer line%d %s\n",__LINE__,#c);exit(1);}}while(0)
int main(void) {
    static const unsigned char read_slot[24]={2,0x11,0x80,1,0,8,0,0,0x44,0x33,0x22,0x11,
        0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0xef,0xcd,0xab,0x89};
    unsigned char bytes[32],before_bytes[32];tsfp_io_transfer value={0},before;uint32_t address;unsigned major;
    CHECK(tsfp_io_transfer_decode(read_slot,24,&value));
    CHECK(value.major==2 && value.minor==0x11 && value.flags==0x80 && value.control==1);
    CHECK(value.length==2048 && value.buffer_word==UINT32_C(0x11223344));
    CHECK(value.byte_offset==UINT64_C(0x1122334455667788) && value.device==UINT32_C(0x89abcdef));
    memset(bytes,0xa7,sizeof(bytes));CHECK(tsfp_io_transfer_encode(bytes+3,24,&value));
    CHECK(!memcmp(bytes+3,read_slot,24) && bytes[2]==0xa7 && bytes[27]==0xa7);
    memset(&value,0xdd,sizeof(value));before=value;
    CHECK(!tsfp_io_transfer_decode(bytes+3,23,&value) && !memcmp(&value,&before,sizeof(value)));
    CHECK(!tsfp_io_transfer_decode(NULL,24,&value) && !tsfp_io_transfer_decode(read_slot,24,NULL));
    CHECK(tsfp_io_transfer_decode(bytes+3,24,&value));
    memcpy(before_bytes,bytes,sizeof(bytes));
    CHECK(!tsfp_io_transfer_encode(bytes+3,23,&value) && !memcmp(bytes,before_bytes,sizeof(bytes)));
    CHECK(!tsfp_io_transfer_encode(NULL,24,&value) && !tsfp_io_transfer_encode(bytes+3,24,NULL));
    for(major=0;major<256;++major) {
        bytes[3]=(unsigned char)major;before=value;
        if(major==2 || major==3) {
            CHECK(tsfp_io_transfer_decode(bytes+3,24,&value) && value.major==major);
            CHECK(value.length==2048 && value.buffer_word==UINT32_C(0x11223344) && value.device==UINT32_C(0x89abcdef));
            continue;
        }
        CHECK(!tsfp_io_transfer_decode(bytes+3,24,&value) && !memcmp(&value,&before,sizeof(value)));
        before.major=(unsigned char)major;memcpy(before_bytes,bytes,sizeof(bytes));
        CHECK(!tsfp_io_transfer_encode(bytes+3,24,&before) && !memcmp(bytes,before_bytes,sizeof(bytes)));
    }
    value.major=2;value.flags=0;value.buffer_word=0x20;value.length=0x30;address=0xfeedface;
    CHECK(tsfp_io_transfer_address(&value,0x1000,&address) && address==0x1020);
    value.flags=0x7f;CHECK(tsfp_io_transfer_address(&value,0x1000,&address) && address==0x1020);
    value.flags=0x80;CHECK(tsfp_io_transfer_address(&value,0xfffffff0,&address) && address==0x20);
    value.flags=0xff;CHECK(tsfp_io_transfer_address(&value,0xfffffff0,&address) && address==0x20);
    address=0xfeedface;value.flags=0;
    CHECK(!tsfp_io_transfer_address(&value,0xfffffff0,&address) && address==0xfeedface);
    value.buffer_word=0xfffffff0;value.flags=0x80;value.length=17;
    CHECK(!tsfp_io_transfer_address(&value,0,&address) && address==0xfeedface);
    value.length=16;CHECK(tsfp_io_transfer_address(&value,0,&address) && address==0xfffffff0);
    value.buffer_word=0;value.length=1;address=0xfeedface;
    CHECK(!tsfp_io_transfer_address(&value,0,&address) && address==0xfeedface);
    value.length=0;CHECK(tsfp_io_transfer_address(&value,0,&address) && !address);
    value.major=4;address=0xfeedface;
    CHECK(!tsfp_io_transfer_address(&value,0,&address) && address==0xfeedface);
    CHECK(!tsfp_io_transfer_address(NULL,0,&address) && !tsfp_io_transfer_address(&value,0,NULL));
    puts("PASS: US transfer fields, read/write only, exact unaligned bytes, direct/relative buffers and overflow/failure guards");return 0;
}
