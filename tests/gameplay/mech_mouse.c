#include "pc_mounted.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);return 1;}}while(0)
#define U(a) (*(uint32_t*)(guest+(a)))
#define F(a) (*(float*)(guest+(a)))
#define MOVE(x,y) pc_input_motion(&input,(x),(y),1000)
#define APPLY() pc_mounted_mouse(&input,1001,.1f,0,0x10000,0x40000,offset,0)
int main(void) {
    unsigned char *guest=calloc(1,0x800000);CHECK(guest);
    ptrdiff_t offset=(ptrdiff_t)guest;PcInput input={0};
    pc_input_focus(&input,1);pc_input_capture(&input,1);
    U(0x10114)=0x8000;U(0x733108)=0x50000;U(0x502ac)=0x60000;
    U(0x60020)=8;U(0x6007c)=0x61000;U(0x61aa0)=0x63000;U(0x63050)=1;
    U(0x4e7944)=1;F(0x40384)=55;F(0x10100)=34;F(0x10104)=-12;
    for(int rate=60;rate<=240;rate+=60) for(int invert=0;invert<=1;invert++) {
        F(0x6302c)=F(0x63030)=0;U(0x4e7944)=rate==60;F(0x4e794c)=60.f/rate;
        MOVE(100,20);
        CHECK(pc_mounted_mouse(&input,1001,.1f,invert,0x10000,0x40000,offset,rate!=60)==1);
        CHECK(fabsf(F(0x63030)-350)<.001f && fabsf(F(0x6302c)-(invert?2:358))<.001f);
        CHECK(pc_mounted_mouse(&input,1001,.1f,invert,0x10000,0x40000,offset,rate!=60)==0);
        CHECK(F(0x10100)==34 && F(0x10104)==-12);
    }
    U(0x4e7944)=1;
    MOVE(0,100000);CHECK(APPLY()==1 && F(0x6302c)==280);
    MOVE(0,-100000);CHECK(APPLY()==1 && F(0x6302c)==10);
    F(0x6302c)=F(0x63030)=0;F(0x40384)=27.5f;
    MOVE(20,20);CHECK(APPLY()==1 && F(0x63030)==359 && F(0x6302c)==359);
    F(0x40384)=55;
    U(0x63050)=0;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);U(0x63050)=1;
    U(0x6309c)=1;MOVE(10,10);CHECK(APPLY()==0);U(0x6309c)=0;
    U(0x10004)=1;MOVE(10,10);CHECK(APPLY()==0);U(0x10004)=0;
    U(0x60020)=7;MOVE(10,10);CHECK(APPLY()==0);U(0x60020)=8;
    U(0x61aa0)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x61aa0)=0x63000;
    U(0x733108)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x733108)=0x50000;
    U(0x4e7944)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x4e7944)=1;
    input.focused=0;MOVE(10,10);CHECK(APPLY()==0);input.focused=1;
    input.captured=0;MOVE(10,10);CHECK(APPLY()==0);input.captured=1;
    MOVE(10,10);CHECK(pc_mounted_mouse(&input,1200,.1f,0,0x10000,0x40000,offset,0)==0);
    F(0x40384)=NAN;MOVE(10,10);CHECK(APPLY()==0);F(0x40384)=55;
    F(0x63030)=INFINITY;MOVE(10,10);CHECK(APPLY()==0);F(0x63030)=359;
    CHECK(F(0x6302c)==359 && !input.dx && !input.dy);
    U(0x10114)=2;MOVE(10,10);CHECK(APPLY()==-1 && input.dx==10);
    free(guest);puts("PASS: mounted mouse routing, Battle Mech guards and cadence");return 0;
}
