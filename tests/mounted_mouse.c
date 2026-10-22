#include "pc_mounted.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(0)
#define U(a) (*(uint32_t*)(guest+(a)))
#define F(a) (*(float*)(guest+(a)))
#define MOVE(x,y) pc_input_motion(&input,(x),(y),1000)
#define APPLY() pc_mounted_mouse(&input,1001,.1f,0,0x10000,0x40000,offset,0)
int main(void)
{
    unsigned char *guest=calloc(1,0x800000);
    ptrdiff_t offset=(ptrdiff_t)guest;
    PcInput input={0};
    CHECK(guest);
    pc_input_focus(&input,1);pc_input_capture(&input,1);
    U(0x10000)=15;U(0x10004)=0;U(0x10114)=0x80;U(0x10014)=0x20000;
    U(0x2007c)=0x21000;U(0x21c14)=0x30000;U(0x3007c)=0x31000;
    U(0x31000)=0x30000;U(0x31004)=0x4147554e;U(0x31010)=4;U(0x31068)=0x20000;
    U(0x4e7944)=1;F(0x40384)=55;
    F(0x10100)=34;F(0x10104)=-12;
    F(0x3101c)=30;F(0x31024)=0;
    F(0x31034)=0;F(0x31038)=60;F(0x3103c)=-90;F(0x31040)=90;
    MOVE(100,20);CHECK(APPLY()==1);
    CHECK(fabsf(F(0x31024)+10)<.001f && fabsf(F(0x3101c)-32)<.001f);
    CHECK(F(0x10100)==34 && F(0x10104)==-12);
    CHECK(APPLY()==0); /* consume once; no drift when mouse stops */
    MOVE(100000,100000);CHECK(APPLY()==1);
    CHECK(F(0x31024)==-90 && F(0x3101c)==60);
    MOVE(-100000,-100000);CHECK(APPLY()==1);
    CHECK(F(0x31024)==90 && F(0x3101c)==0);
    F(0x3101c)=30;F(0x31024)=0;F(0x40384)=27.5f;
    U(0x31008)=0x1000000;F(0x3105c)=1.6f;
    MOVE(10,10);CHECK(pc_mounted_mouse(&input,1001,.1f,1,0x10000,0x40000,offset,0)==1);
    CHECK(fabsf(F(0x31024)+.8f)<.001f && fabsf(F(0x3101c)-29.2f)<.001f);
    /* Exact authored fixed axes and full-circle guns retain their semantics. */
    U(0x31008)=0;F(0x40384)=55;F(0x31034)=F(0x31038)=15;
    F(0x3103c)=-180;F(0x31040)=180;F(0x31024)=179;
    MOVE(-20,100);CHECK(APPLY()==1);
    CHECK(fabsf(F(0x31024)+179)<.001f && F(0x3101c)==15);
    /* A mode/owner mismatch must not move an AI gun or the hidden player. */
    U(0x31068)=0;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);U(0x31068)=0x20000;
    U(0x31004)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x31004)=0x4147554e;
    U(0x31010)=1;MOVE(10,10);CHECK(APPLY()==0);
    U(0x31010)=9;MOVE(10,10);CHECK(APPLY()==0);U(0x31010)=4;
    U(0x10004)=1;MOVE(10,10);CHECK(APPLY()==0);U(0x10004)=0;
    U(0x3007c)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x3007c)=0x31000;
    U(0x4e7944)=0;MOVE(10,10);CHECK(APPLY()==0);U(0x4e7944)=1;
    input.focused=0;MOVE(10,10);CHECK(APPLY()==0);input.focused=1;
    input.captured=0;MOVE(10,10);CHECK(APPLY()==0);input.captured=1;
    MOVE(10,10);CHECK(pc_mounted_mouse(&input,1200,.1f,0,0x10000,0x40000,offset,0)==0);
    F(0x40384)=NAN;MOVE(10,10);CHECK(APPLY()==0);F(0x40384)=55;
    input.dx=INFINITY;CHECK(APPLY()==0 && !input.dx);
    CHECK(F(0x10100)==34 && F(0x10104)==-12 && F(0x31024)==-179);
    /* Ordinary look and console pointers keep their input; exiting a gun
     * never replays rejected/paused motion into the on-foot camera. */
    U(0x10114)=2;CHECK(APPLY()==-1 && !input.dx && !input.dy);
    MOVE(10,20);CHECK(APPLY()==-1 && input.dx==10 && input.dy==20);
    CHECK(pc_input_look(&input,1001,.1f,0,1,(float*)(guest+0x10100),(float*)(guest+0x10104)));
    CHECK(F(0x10100)==33 && F(0x10104)==-14);
    U(0x10114)=0x2000;MOVE(10,20);CHECK(APPLY()==-1 && input.dx==10);
    free(guest);
    puts("PASS: mounted mouse routing, authored limits, ownership and gameplay return");
    return 0;
}
