#include "guest_device_body.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"device body line%d %s\n",__LINE__,#c);exit(1);}}while(0)
static unsigned char memory[600],before[600];
static void rejected(uint32_t address,uint32_t driver,uint32_t extension,uint32_t type,size_t span) {
    memcpy(before,memory,sizeof(memory));
    CHECK(!tsfp_device_body_initialize(memory+3,span,address,driver,extension,type,1,1));
    CHECK(!memcmp(before,memory,sizeof(memory)));
}
int main(void) {
    static const unsigned char expected[72]={
        3,0,0xb8,1, 0,0,0,0, 0,0x10,0x20,0x30, 0,0,0,0,
        0,0,0,0, 0x1a,0,0,0, 0x49,0x10,0,0, 0x3a,0,1,0,
        0,0,0,0, 0,0,0,0, 0x14,0,12,0, 0x2d,0x10,0,0,
        0x2d,0x10,0,0, 1,0,4,0, 1,0,0,0, 0x3d,0x10,0,0,
        0x3d,0x10,0,0, 0,0,0,0
    };
    unsigned i;unsigned char *p=memory+3;
    memset(memory,0xa7,sizeof(memory));
    rejected(0,1,0,0x3a,72);rejected(0x1001,0,0,0x3a,72);
    rejected(0x1001,1,0x170,0x3a,439);rejected(0x1001,1,UINT32_MAX,0x3a,597);
    rejected(0x1001,1,0,256,72);rejected(UINT32_MAX-70,1,0,0x3a,72);
    CHECK(tsfp_device_body_initialize(p,440,0x1001,0x30201000,0x170,0x3a,1,1));
    CHECK(!memcmp(p,expected,sizeof(expected)));
    for(i=72;i<440;++i)CHECK(!p[i]);
    CHECK(memory[2]==0xa7 && memory[443]==0xa7);
    memcpy(before,memory,sizeof(memory));
    CHECK(!tsfp_device_body_mark_deleted(p,439));CHECK(!memcmp(before,memory,sizeof(memory)));
    CHECK(tsfp_device_body_mark_deleted(p,440));before[3+0x1f]=1;
    CHECK(!memcmp(before,memory,sizeof(memory)));CHECK(tsfp_device_body_mark_deleted(p,440));
    p[0]=7;memcpy(before,memory,sizeof(memory));CHECK(!tsfp_device_body_mark_deleted(p,440));
    CHECK(!memcmp(before,memory,sizeof(memory)));
    CHECK(tsfp_device_body_initialize(p,72,UINT32_MAX-71,1,0,0,0,0));
    CHECK(p[2]==72 && !p[3] && p[0x14]==0x10 && !p[0x18] && !p[0x19] && !p[0x1a] && !p[0x1b]);
    CHECK(tsfp_device_body_mark_deleted(p,72));
    puts("PASS: literal unaligned device layout, extension clearing, self-links and bounded deletion");return 0;
}
