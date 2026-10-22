#include "pc_terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); exit(1); } } while (0)
#define U(a) (*(uint32_t *)(guest+(a)))
#define F(a) (*(float *)(guest+(a)))

static unsigned char *guest;
static ptrdiff_t offset;

static void setup(PcInput *input)
{
    memset(input,0,sizeof *input);
    pc_input_focus(input,1);pc_input_capture(input,1);
    U(0x10000)=15;U(0x10004)=2;U(0x10114)=0x2000;
    U(0x76c27c+8)=0x20000;U(0x2007c)=0x21000;
    U(0x21014)=0x10000;U(0x21030)=0x22000;
    U(0x21058)=0;U(0x22014)=0;
    F(0x21038)=100;F(0x2103c)=100;
    F(0x10100)=34;F(0x10104)=-12;
}

static int cadence(unsigned rate,unsigned packets,int native,unsigned slow,int jitter)
{
    PcInput input;
    unsigned packet=1,previous_ticks=0,changes=0,frames_with_packets=0;
    setup(&input);
    for (unsigned frame=1;frame<=rate;++frame) {
        uint64_t due_us=(uint64_t)frame*1000000/rate;
        if(jitter && frame<rate)
            due_us=(uint64_t)((int64_t)due_us+(frame%3==0?2000:frame%3==1?-2000:0));
        unsigned ticks=(unsigned)(due_us*60/(1000000*slow));
        U(0x4e7944)=native ? ticks-previous_ticks : 1;
        F(0x4e794c)=60.f/(rate*slow);
        previous_ticks=ticks;
        unsigned first_packet=packet;
        while ((uint64_t)packet*1000000<=due_us*packets) {
            pc_input_motion(&input,1000.0/packets,250.0/packets,1000+packet*1000/packets);
            ++packet;
        }
        frames_with_packets+=packet!=first_packet;
        U(0x21020)=1234;
        int changed=pc_terminal_mouse(&input,1000+due_us/1000,.1f,0x10000,offset,native)>0;
        changes+=changed;
        CHECK(U(0x21020)==(changed?0:1234));
        CHECK(!input.dx && !input.dy);
        CHECK(F(0x10100)==34 && F(0x10104)==-12);
    }
    printf("terminal native %d updates %u packets %u slow %u jitter %d: x %.6f y %.6f changes %u\n",
           native,rate,packets,slow,jitter,F(0x21038),F(0x2103c),changes);
    return fabs(F(0x21038)-200)<.003 && fabs(F(0x2103c)-125)<.003 &&
           changes==frames_with_packets;
}

static void gates(void)
{
    PcInput input;
    uint64_t now=5000;
    const float invalid[]={0,-1,NAN,INFINITY};
    setup(&input);
    /* Native paused/stale-whole and invalid-clock images consume input but
       must move neither the pointer nor the hidden player. */
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;++i) {
        U(0x4e7944)=1;F(0x4e794c)=invalid[i];
        pc_input_motion(&input,10,20,now);
        CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==0);
        CHECK(!input.dx && !input.dy && F(0x21038)==100 && F(0x2103c)==100);
        now+=7;
    }
    U(0x4e7944)=0;F(0x4e794c)=60.f/144;
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==0); /* no paused motion replay */
    pc_input_motion(&input,10,20,now);
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,0)==0); /* original whole gate */
    CHECK(F(0x21038)==100);
    for(unsigned port=0;port<4;++port) {
        U(0x10004)=port;U(0x76c27c+port*4)=0x20000;
        pc_input_motion(&input,10,20,now);
        CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==1);
        CHECK(fabs(F(0x21038)-(101+port))<.001);
        now+=7;
    }
    /* Catch-up time must not multiply raw counts. */
    U(0x4e7944)=3;F(0x4e794c)=3;
    pc_input_motion(&input,10,20,now);
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==1);
    CHECK(F(0x21038)==105 && F(0x2103c)==110);
    U(0x21058)=1;
    pc_input_motion(&input,100,100,now);
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==0 && !input.dx);
    U(0x21058)=0;U(0x22014)=4;
    pc_input_motion(&input,100,100,now);
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==0 && !input.dx);
    U(0x22014)=0;U(0x21014)=0x10010;
    pc_input_motion(&input,100,100,now);
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==0 && !input.dx);
    U(0x21014)=0x10000;
    pc_input_motion(&input,100,100,now);
    CHECK(pc_terminal_mouse(&input,now+150,.1f,0x10000,offset,1)==0 && !input.dx);
    CHECK(F(0x21038)==105 && F(0x2103c)==110);
    CHECK(F(0x10100)==34 && F(0x10104)==-12);
    U(0x10114)=2;now+=151;
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==-1);
    CHECK(!pc_input_look(&input,now,.1f,0,1,(float *)(guest+0x10100),(float *)(guest+0x10104)));
    pc_input_motion(&input,10,20,now);
    CHECK(pc_terminal_mouse(&input,now,.1f,0x10000,offset,1)==-1);
    CHECK(pc_input_look(&input,now,.1f,0,1,(float *)(guest+0x10100),(float *)(guest+0x10104)));
    CHECK(F(0x10100)==33 && F(0x10104)==-14);
}

int main(void)
{
    const unsigned rates[]={60,120,144},packets[]={125,1000,8000};
    unsigned failures=0;
    guest=calloc(1,0x800000);CHECK(guest);offset=(ptrdiff_t)guest;
    for(int jitter=0;jitter<=1;++jitter) {
        for (unsigned p=0;p<3;++p) failures+=!cadence(60,packets[p],0,1,jitter);
        for (unsigned r=0;r<3;++r)
            for (unsigned p=0;p<3;++p)
                for (unsigned slow=1;slow<=4;slow+=3)
                    failures+=!cadence(rates[r],packets[p],1,slow,jitter);
    }
    gates();
    free(guest);
    CHECK(!failures);
    puts("PASS: terminal mouse routing across original/native packet and update rates");
    return 0;
}
