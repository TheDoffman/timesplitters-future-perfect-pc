#include "pc_input.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);exit(1); } } while(0)
#define U(a) (*(uint32_t*)(guest+(a)))
#define F(a) (*(float*)(guest+(a)))
static unsigned char *guest;
static ptrdiff_t offset;
static int legacy;
static uint64_t consume(PcInput *input,uint64_t now,double *dx,double *dy)
{
    if(legacy==2) {
        *dx=input->dx;*dy=input->dy;input->dx=input->dy=0;
        return now-input->motion_at;
    }
    return pc_input_consume_motion(input,now,dx,dy);
}
#define pc_input_consume_motion consume
#include "pc_mounted.h"
#undef pc_input_consume_motion

static void setup(PcInput *input)
{
    memset(guest,0,0x800000);memset(input,0,sizeof *input);
    pc_input_focus(input,1);pc_input_capture(input,1);
    U(0x10000)=15;U(0x10004)=0;U(0x10114)=0x80;U(0x10014)=0x20000;
    U(0x2007c)=0x21000;U(0x21c14)=0x30000;U(0x3007c)=0x31000;
    U(0x31000)=0x30000;U(0x31004)=0x4147554e;U(0x31010)=4;U(0x310d8)=0x20000;
    F(0x40384)=55;F(0x10100)=34;F(0x10104)=-12;
    F(0x31034)=-90;F(0x31038)=90;F(0x3103c)=-179;F(0x31040)=179;
}
static int apply(PcInput *input,uint64_t now,int native)
{ return pc_mounted_mouse(input,now,.1f,0,0x10000,0x40000,offset,native && legacy!=1); }

static void cadence(unsigned rate,unsigned packets,unsigned slow,int jitter)
{
    PcInput input;setup(&input);
    unsigned packet=1,previous_ticks=0,changes=0,frames_with_packets=0;
    for(unsigned frame=1;frame<=rate;frame++) {
        uint64_t due=(uint64_t)frame*1000000/rate;
        if(jitter && frame<rate)due=(uint64_t)((int64_t)due+(frame%3==0?2000:frame%3==1?-2000:0));
        unsigned ticks=(unsigned)(due*60/(1000000*slow));
        U(0x4e7944)=rate==60?1:ticks-previous_ticks;previous_ticks=ticks;
        F(0x4e794c)=60.f/(rate*slow);
        unsigned first=packet;
        while((uint64_t)packet*1000000<=due*packets) {
            pc_input_motion(&input,1000.0/packets,250.0/packets,1000+packet*1000/packets);
            packet++;
        }
        frames_with_packets+=packet!=first;
        changes+=apply(&input,1000+due/1000,rate!=60)>0;
        CHECK(!input.dx && !input.dy);
        CHECK(F(0x10100)==34 && F(0x10104)==-12);
    }
    printf("mounted fps %u packets %u slow %u jitter %d: yaw %.6f pitch %.6f updates %u/%u\n",
           rate,packets,slow,jitter,F(0x31024),F(0x3101c),changes,frames_with_packets);
    CHECK(fabsf(F(0x31024)+100)<.003f && fabsf(F(0x3101c)-25)<.003f);
    CHECK(changes==frames_with_packets);
    CHECK(apply(&input,2100,rate!=60)==0); /* no replay after input stops */
}
static void gates(void)
{
    PcInput input;setup(&input);
    const float invalid[]={0,-1,NAN,INFINITY};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++) {
        U(0x4e7944)=1;F(0x4e794c)=invalid[i];
        pc_input_motion(&input,10,20,5000);
        CHECK(apply(&input,5000,1)==0 && !input.dx && !input.dy);
        CHECK(F(0x31024)==0 && F(0x3101c)==0);
    }
    F(0x4e794c)=.125f;U(0x4e7944)=0;
    CHECK(apply(&input,5001,1)==0); /* paused motion was consumed */
    pc_input_motion(&input,10,20,5002);CHECK(apply(&input,5002,1)==1);
    CHECK(F(0x31024)==-1 && F(0x3101c)==2);
    /* Original whole-time gate and native-to-original handoff retain their rules. */
    pc_input_motion(&input,10,20,5003);CHECK(apply(&input,5003,0)==0);
    CHECK(F(0x31024)==-1 && F(0x3101c)==2 && !input.dx);
    U(0x4e7944)=1;F(0x4e794c)=0;
    pc_input_motion(&input,10,20,5004);CHECK(apply(&input,5004,0)==1);
    CHECK(F(0x31024)==-2 && F(0x3101c)==4);
    U(0x10114)=2;pc_input_motion(&input,10,20,5005);
    CHECK(apply(&input,5005,1)==-1 && input.dx==10 && input.dy==20);
}
int main(int argc,char **argv)
{
    legacy=argc>1?(!strcmp(argv[1],"legacy-clock")?1:2):0;setbuf(stdout,NULL);
    guest=calloc(1,0x800000);CHECK(guest);offset=(ptrdiff_t)guest;
    const unsigned rates[]={60,120,144,180,240},packets[]={125,1000,8000};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++)for(unsigned p=0;p<3;p++)for(unsigned slow=1;slow<=4;slow*=4)
        for(int jitter=0;jitter<2;jitter++)cadence(rates[r],packets[p],slow,jitter);
    gates();free(guest);puts("PASS: mounted mouse routing at native cadence, pause and handoff");return 0;
}
