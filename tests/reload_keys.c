#include <assert.h>
#include <stdio.h>
#include "pc_input.c"

static unsigned char roles[256];
static void context(PcInput *s,int reloading,unsigned now)
{ pc_reload_context(&s->reload,1,1,reloading,now); }
static void sample(PcInput *s,unsigned now,int active,int *r,int *f,int *a)
{ pc_reload_sample(&s->reload,roles,active,now,r,f,a); }

static void taps(void)
{
    const unsigned rates[]={60,120,144,180,240},widths[]={1,5,10,15,20,30,50,100};
    unsigned cases=0;
    for(unsigned rate=0;rate<5;rate++)for(unsigned w=0;w<8;w++)for(unsigned phase=0;phase<34;phase++) {
        PcInput s={0};pc_input_focus(&s,1);pc_input_capture(&s,1);context(&s,0,0);
        unsigned down_at=100+phase,up_at=down_at+widths[w],frame=1,edges=0,held=0;
        int reloading=0;
        /* 1 ms host events, guest samples at the render rate, original digital
         * decoder only publishes on 60 Hz ticks. Down/up can share a pump. */
        for(unsigned t=0;t<400;t++) {
            if(t==down_at)pc_input_key_at(&s,'R',1,t);
            if(t==up_at)pc_input_key_at(&s,'R',0,t);
            if(t* rates[rate] < frame*1000)continue;
            context(&s,reloading,t);
            int r=s.keys['R']?255:0,f=0,a=0;sample(&s,t,1,&r,&f,&a);
            if(frame*60/rates[rate]!=(frame-1)*60/rates[rate]) {
                if(r && !held){edges++;reloading=1;}
                held=!!r;
            }
            frame++;
        }
        assert(edges==1);cases++;
    }
    printf("PASS: %u short-tap phase/rate/width schedules\n",cases);
}

static void priority(void)
{
    PcInput s={0};int r,f,a;
    pc_input_focus(&s,1);pc_input_capture(&s,1);context(&s,0,100);
    pc_input_key_at(&s,1,1,100); /* Fire already held */
    pc_input_key_at(&s,'R',1,105);pc_input_key_at(&s,'R',0,106);
    r=0;f=255;a=0;sample(&s,110,1,&r,&f,&a);assert(r==255 && f==0);
    context(&s,1,120);r=0;f=255;a=0;sample(&s,120,1,&r,&f,&a);assert(f==0);
    context(&s,1,700);r=0;f=255;a=0;sample(&s,700,1,&r,&f,&a);assert(!r && !f);
    context(&s,0,710);r=0;f=255;a=0;sample(&s,710,1,&r,&f,&a);assert(!r && f==255);
    /* A fresh shot interrupts, even when its release and press share a poll. */
    pc_input_key_at(&s,'R',1,800);context(&s,0,800);
    r=255;f=255;a=0;sample(&s,800,1,&r,&f,&a);assert(!f);
    context(&s,1,820);pc_input_key_at(&s,1,0,820);pc_input_key_at(&s,1,1,821);
    r=255;f=255;a=0;sample(&s,825,1,&r,&f,&a);assert(f==255);
    /* No-ammo/full/busy weapon: rejected request expires, not stuck Fire. */
    pc_reload_reset(&s.reload);context(&s,0,1000);
    pc_input_key_at(&s,'R',0,1000);pc_input_key_at(&s,'R',1,1001);
    r=255;f=255;a=255;sample(&s,1002,1,&r,&f,&a);assert(!f && !a);
    context(&s,0,1153);r=0;f=255;a=255;sample(&s,1153,1,&r,&f,&a);assert(!r && f==255 && a==255);
    /* OS autorepeat cannot continually suppress Fire/restart a request. */
    pc_input_key_at(&s,'R',1,1160);context(&s,0,1160);
    r=255;f=255;a=0;sample(&s,1160,1,&r,&f,&a);assert(f==255);
}

static void event_order(void)
{
    /* Both trigger actions, alternate binding, same-millisecond events, and
     * a held trigger released/repressed before the first sample. */
    roles[3]=2;
    for(unsigned key=1;key<=3;key++)for(int held=0;held<2;held++)
    for(int fire_first=0;fire_first<2;fire_first++)for(int same_time=0;same_time<2;same_time++) {
        PcInput s={0};int r=0,f=key==2?0:255,a=key==2?255:0;
        pc_input_focus(&s,1);pc_input_capture(&s,1);context(&s,0,100);
        if(held)pc_input_key_at(&s,key,1,100);
        if(fire_first) {
            if(held)pc_input_key_at(&s,key,0,101);
            pc_input_key_at(&s,key,1,101);
        }
        pc_input_key_at(&s,'R',1,101);pc_input_key_at(&s,'R',0,102);
        if(!fire_first) {
            if(held)pc_input_key_at(&s,key,0,same_time?101:105);
            pc_input_key_at(&s,key,1,same_time?101:105);
        }
        sample(&s,110,1,&r,&f,&a);
        assert(r==255);
        assert((key==2?a:f)==(fire_first?0:255));
        context(&s,1,120);r=0;f=key==2?0:255;a=key==2?255:0;
        sample(&s,120,1,&r,&f,&a);
        assert((key==2?a:f)==(fire_first?0:255));
    }
    roles[3]=0;
}

static void lifecycle(void)
{
    for(int guard=0;guard<7;guard++) {
        PcInput s={0};int r=0,f=255,a=0;
        pc_input_focus(&s,1);pc_input_capture(&s,1);context(&s,0,100);
        pc_input_key_at(&s,'R',1,100);pc_input_key_at(&s,'R',0,101);
        if(guard==0)pc_input_focus(&s,0);
        if(guard==1)pc_input_capture(&s,0);
        if(guard==2)pc_reload_context(&s.reload,1,0,0,102); /* dead/mounted/cutscene */
        if(guard==3)pc_reload_context(&s.reload,2,1,0,102); /* player change */
        sample(&s,guard==4?250:102,guard!=5 && guard!=6,&r,&f,&a); /* stale/menu/controller */
        assert(!r && f==255);
        context(&s,0,260);sample(&s,260,1,&r,&f,&a);assert(!r && f==255);
    }
    /* Bindings are action roles, not hardcoded R: alternate keyboard/mouse. */
    PcInput s={0};pc_input_focus(&s,1);pc_input_capture(&s,1);context(&s,0,100);
    roles['R']=0;roles['T']=1;roles[5]=1;
    pc_input_key_at(&s,'R',1,100);int r=0,f=255,a=0;sample(&s,101,1,&r,&f,&a);assert(!r && f==255);
    pc_input_key_at(&s,5,1,102);pc_input_key_at(&s,5,0,103);
    sample(&s,104,1,&r,&f,&a);assert(r==255 && !f);
}
int main(void)
{
    roles['R']=1;roles[1]=2;roles[2]=4;
    taps();priority();event_order();lifecycle();puts("PASS: reload priority, fresh Fire, expiry, remapping and lifecycle");
    return 0;
}
