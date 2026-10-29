#include "guest_io_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"stack line%d %s\n",__LINE__,#c);exit(1);}}while(0)
int main(void) {
    static const unsigned char expected[24]={2,0x11,0x80,1,0,8,0,0,0x44,0x33,0x22,0x11,
        0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0xef,0xcd,0xab,0x89};
    tsfp_io_stack source={2,0x11,0x80,1,2048,UINT32_C(0x11223344),UINT64_C(0x1122334455667788),UINT32_C(0x89abcdef)},decoded,before;
    unsigned char bytes[32],saved[32];unsigned i;
    memset(bytes,0xa7,sizeof(bytes));
    CHECK(tsfp_io_stack_encode(bytes+3,24,&source));
    CHECK(!memcmp(bytes+3,expected,24));
    for(i=0;i<sizeof(bytes);++i)if(i<3 || i>=27)CHECK(bytes[i]==0xa7);
    CHECK(tsfp_io_stack_decode(expected,24,&decoded));
    CHECK(decoded.major==2 && decoded.minor==0x11 && decoded.flags==0x80 && decoded.control==1);
    CHECK(decoded.length==2048 && decoded.key==UINT32_C(0x11223344));
    CHECK(decoded.offset==UINT64_C(0x1122334455667788) && decoded.device==UINT32_C(0x89abcdef));
    CHECK(tsfp_io_stack_decode(bytes+3,24,&decoded));
    memcpy(saved,bytes,sizeof(bytes));memset(&decoded,0xa7,sizeof(decoded));memcpy(&before,&decoded,sizeof(decoded));
    CHECK(!tsfp_io_stack_decode(bytes+3,23,&decoded) && !memcmp(&before,&decoded,sizeof(decoded)));
    CHECK(!tsfp_io_stack_encode(bytes+3,23,&source) && !memcmp(saved,bytes,sizeof(bytes)));
    CHECK(!tsfp_io_stack_encode(NULL,24,&source) && !tsfp_io_stack_encode(bytes,24,NULL));
    CHECK(!tsfp_io_stack_decode(NULL,24,&decoded) && !tsfp_io_stack_decode(bytes,24,NULL));
    puts("PASS: explicit stack bytes, 64-bit offset, unaligned access and failure guards");return 0;
}
