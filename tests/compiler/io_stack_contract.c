#include "guest_io_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"stack line%d %s\n",__LINE__,#c);exit(1);}}while(0)
int main(void) {
    static const unsigned char expected[24]={2,0x11,0x80,1,0,8,0,0,0x44,0x33,0x22,0x11,
        0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0xef,0xcd,0xab,0x89};
    tsfp_io_stack source={0},decoded,before;
    unsigned char bytes[32],saved[32];unsigned i,j;
    source.major=2;source.minor=0x11;source.flags=0x80;source.control=1;
    memcpy(source.opaque,expected+4,20);
    memset(bytes,0xa7,sizeof(bytes));
    CHECK(tsfp_io_stack_encode(bytes+3,24,&source));
    CHECK(!memcmp(bytes+3,expected,24));
    for(i=0;i<sizeof(bytes);++i)if(i<3 || i>=27)CHECK(bytes[i]==0xa7);
    CHECK(tsfp_io_stack_decode(expected,24,&decoded));
    CHECK(decoded.major==2 && decoded.minor==0x11 && decoded.flags==0x80 && decoded.control==1);
    CHECK(!memcmp(decoded.opaque,expected+4,20));
    CHECK(tsfp_io_stack_decode(bytes+3,24,&decoded));
    memcpy(saved,bytes,sizeof(bytes));memset(&decoded,0xa7,sizeof(decoded));memcpy(&before,&decoded,sizeof(decoded));
    CHECK(!tsfp_io_stack_decode(bytes+3,23,&decoded) && !memcmp(&before,&decoded,sizeof(decoded)));
    CHECK(!tsfp_io_stack_encode(bytes+3,23,&source) && !memcmp(saved,bytes,sizeof(bytes)));
    CHECK(!tsfp_io_stack_encode(NULL,24,&source) && !tsfp_io_stack_encode(bytes,24,NULL));
    CHECK(!tsfp_io_stack_decode(NULL,24,&decoded) && !tsfp_io_stack_decode(bytes,24,NULL));
    /* Independently vary every opaque byte; none is an implicit device tag or
     * host integer. Preserve all other bytes and the unaligned span's guards. */
    for(i=0;i<20;++i)for(j=0;j<256;++j) {
        unsigned char literal[24];memcpy(literal,expected,24);literal[4+i]=(unsigned char)j;
        CHECK(tsfp_io_stack_decode(literal,24,&decoded));
        CHECK(decoded.major==2 && decoded.minor==0x11 && decoded.flags==0x80 && decoded.control==1);
        CHECK(!memcmp(decoded.opaque,literal+4,20));
        memset(bytes,0xa7,sizeof(bytes));CHECK(tsfp_io_stack_encode(bytes+3,24,&decoded));
        CHECK(!memcmp(bytes+3,literal,24) && bytes[2]==0xa7 && bytes[27]==0xa7);
    }
    puts("PASS: dispatch prefix, all5120 opaque byte variants, exact unaligned spans and failure preservation");return 0;
}
