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
    U(0x31000)=0x30000;U(0x31004)=0x4147554e;U(0x31010)=4;U(0x310d8)=0x20000;
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
    /* +68 is the AI target, not the operator. Targeting the player must not
     * grant mouse control; only the reciprocal +D8 operator link does. */
    U(0x31068)=0x20000;U(0x310d8)=0;MOVE(10,10);CHECK(APPLY()==0 && !input.dx);
    U(0x310d8)=0x22000;MOVE(10,10);CHECK(APPLY()==0);
    U(0x310d8)=0x20000;U(0x31068)=0;
    U(0x31000)=0x32000;MOVE(10,10);CHECK(APPLY()==0);U(0x31000)=0x30000;
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
    /* +4 is an authored instance name, not a gun class. A second gun with
     * another name must work after dismount/re-entry without cached ownership. */
    U(0x10114)=2;CHECK(APPLY()==-1);
    U(0x10114)=0x80;U(0x31004)=0x54414e4b;
    F(0x31034)=0;F(0x31038)=60;F(0x3101c)=30;F(0x31024)=0;
    MOVE(10,20);CHECK(APPLY()==1);
    CHECK(fabsf(F(0x31024)+1)<.001f && fabsf(F(0x3101c)-32)<.001f);
    /* Ordinary look and console pointers keep their input; exiting a gun
     * never replays rejected/paused motion into the on-foot camera. */
    U(0x10114)=2;CHECK(APPLY()==-1 && !input.dx && !input.dy);
    MOVE(10,20);CHECK(APPLY()==-1 && input.dx==10 && input.dy==20);
    CHECK(pc_input_look(&input,1001,.1f,0,1,(float*)(guest+0x10100),(float*)(guest+0x10104)));
    CHECK(F(0x10100)==33 && F(0x10104)==-14);
    U(0x10114)=0x2000;MOVE(10,20);CHECK(APPLY()==-1 && input.dx==10);
    input.dx=input.dy=0;
    /* Vehicle seat ownership is independent of C14 and character names. */
    U(0x10114)=0x400;U(0x2002c)=77;U(0x21c08)=0x400001;
    U(0x72f5e8)=0x70000;U(0x70314)=0x400001;U(0x70364)=0x71000;
    U(0x71140)=4;U(0x71164)=2;U(0x71154)=0x31000;
    const uint32_t seat=0x71070+3*0x34;
    U(seat)=77;U(seat+0x30)=4;U(seat+0x2c)=1;
    U(0x21c14)=0;U(0x310d8)=0;U(0x31010)=0;
    F(0x31034)=-90;F(0x31038)=90;F(0x3103c)=-180;F(0x31040)=180;
    for(int type=3;type<=4;type++)for(int invert=0;invert<=1;invert++) {
        U(seat+0x30)=type;
        for(int rate=60;rate<=240;rate+=60) {
            F(0x3101c)=F(0x31024)=0;U(0x4e7944)=rate==60;
            F(0x4e794c)=60.f/rate;
            MOVE(100,20);
            CHECK(pc_mounted_mouse(&input,1001,.1f,invert,0x10000,0x40000,offset,rate!=60)==1);
            CHECK(fabsf(F(0x31024)+10)<.001f && fabsf(F(0x3101c)-(invert?-2:2))<.001f);
            CHECK(pc_mounted_mouse(&input,1001,.1f,invert,0x10000,0x40000,offset,rate!=60)==0);
        }
    }
    U(0x4e7944)=1;F(0x3101c)=F(0x31024)=0;
    /* Equal vehicle limits mean unrestricted, not the scripted-gun axis lock. */
    F(0x31034)=F(0x31038)=0;F(0x3103c)=F(0x31040)=0;
    MOVE(10,20);CHECK(APPLY()==1 && F(0x31024)==-1 && F(0x3101c)==2);
    F(0x31034)=-90;F(0x31038)=90;F(0x3103c)=-180;F(0x31040)=180;
    F(0x31024)=179;MOVE(-100,0);CHECK(APPLY()==1 && F(0x31024)==180);
    float vehicle_yaw=F(0x31024),vehicle_pitch=F(0x3101c);
#define REJECT_VEHICLE() do {MOVE(10,20);CHECK(APPLY()==0 && !input.dx && !input.dy);CHECK(F(0x31024)==vehicle_yaw && F(0x3101c)==vehicle_pitch);} while(0)
    U(0x70314)++;REJECT_VEHICLE();U(0x70314)--; /* stale vehicle handle */
    U(seat)=78;REJECT_VEHICLE();U(seat)=77;
    U(0x71070)=77;REJECT_VEHICLE();U(0x71070)=0; /* duplicate owner */
    U(0x71140)=5;REJECT_VEHICLE();U(0x71140)=4;
    U(0x71164)=3;REJECT_VEHICLE();U(0x71164)=2;
    U(seat+0x2c)=2;REJECT_VEHICLE();U(seat+0x2c)=1;
    U(0x310d8)=0x22000;REJECT_VEHICLE();U(0x310d8)=0x20000;
    U(0x3007c)=0;REJECT_VEHICLE();U(0x3007c)=0x31000;
    U(0x31010)=13;REJECT_VEHICLE();U(0x31010)=14;REJECT_VEHICLE();U(0x31010)=0;
    U(0x4e7944)=0;REJECT_VEHICLE();U(0x4e7944)=1;
    input.focused=0;REJECT_VEHICLE();input.focused=1;
    F(0x40384)=NAN;REJECT_VEHICLE();F(0x40384)=55;
    U(0x71154)=0x7ffff0;REJECT_VEHICLE();U(0x71154)=0x31000;
    for(int type=0;type<=2;type++) {
        U(seat+0x30)=type;MOVE(10,20);CHECK(APPLY()==-1 && input.dx==10);
        input.dx=input.dy=0;
    }
    /* A new seat/gun binding is followed immediately; no stale gun cache. */
    U(seat)=0;U(0x71070)=77;U(0x710a0)=3;U(0x7109c)=0;U(0x71144)=0x32000;
    U(0x32000)=0x33000;U(0x3307c)=0x32000;
    F(0x32034)=-90;F(0x32038)=90;F(0x3203c)=-180;F(0x32040)=180;
    MOVE(10,20);CHECK(APPLY()==1 && F(0x32024)==-1 && F(0x3201c)==2);
    CHECK(F(0x31024)==vehicle_yaw && F(0x3101c)==vehicle_pitch);
    free(guest);
    puts("PASS: mounted mouse routing, authored limits, ownership and gameplay return");
    return 0;
}
