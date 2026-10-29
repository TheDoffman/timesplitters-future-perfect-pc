#include "guest_io_request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"request line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char storage[4000],saved[4000];
static void unchanged_init(uint32_t address,uint32_t bytes,uint32_t stacks,size_t span) {
    memcpy(saved,storage,sizeof(storage));
    CHECK(!tsfp_io_request_initialize(storage+3,span,address,bytes,stacks,0x1234));
    CHECK(!memcmp(saved,storage,sizeof(storage)));
}
static void unchanged_advance(uint32_t address,size_t span) {
    uint32_t out=0xfeed;memcpy(saved,storage,sizeof(storage));
    CHECK(!tsfp_io_request_advance(storage+3,span,address,&out));
    CHECK(out==0xfeed && !memcmp(saved,storage,sizeof(storage)));
}
int main(void) {
    unsigned char *p=storage+3;uint32_t out=0;unsigned i;
    memset(storage,0xa7,sizeof(storage));
    unchanged_init(0,168,2,168);unchanged_init(UINT32_MAX-103,104,0,104);
    unchanged_init(0x1001,103,0,103);unchanged_init(0x1001,152,2,151);
    unchanged_init(0x1001,168,UINT32_MAX,168);unchanged_init(0x1001,65536,2,3997);
    CHECK(tsfp_io_request_initialize(p,168,0x1001,168,2,0x1234));
    CHECK(p[0]==0x34 && p[1]==0x12 && p[2]==0xa8 && p[3]==0);
    CHECK(p[0x18]==2 && p[0x19]==3);
    CHECK(p[0x5c]==0x99 && p[0x5d]==0x10 && !p[0x5e] && !p[0x5f]);
    CHECK(p[167]==0 && storage[2]==0xa7 && storage[171]==0xa7);
    unchanged_advance(0x1001,103);unchanged_advance(0x1001,167);
    p[0x5c]^=1;unchanged_advance(0x1001,168);p[0x5c]^=1;
    CHECK(tsfp_io_request_advance(p,168,0x1001,&out) && out==0x1081 && p[0x19]==2);
    CHECK(tsfp_io_request_advance(p,168,0x1001,&out) && out==0x1069 && p[0x19]==1);
    unchanged_advance(0x1001,168);
    CHECK(tsfp_io_request_initialize(p,104,0x1001,104,0,0x1234));
    unchanged_advance(0x1001,104);
    CHECK(tsfp_io_request_initialize(p,3152,0x1001,3152,127,0x1234));
    CHECK(p[0x19]==128);
    for(i=127;i>0;--i)CHECK(tsfp_io_request_advance(p,3152,0x1001,&out) && out==0x1001+104+(i-1)*24);
    unchanged_advance(0x1001,3152);
    puts("PASS: unaligned request initialization, bounded stack advance and failure atomicity");return 0;
}
