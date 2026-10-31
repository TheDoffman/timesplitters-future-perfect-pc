#include <stdio.h>
#include <stdlib.h>
#include "../src/network/pc_network_bot_motion.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int near(float a,float b) {return fabsf(a-b)<.0001f;}
int main(void)
{
    PcNetworkBotMotion motion={0};
    PcNetworkBotState state={0},out;
    unsigned i,changes=0;
    float previous=0;
    CHECK(!pc_network_bot_motion_sample(&motion,0,&out));
    state.slot=2;state.spawn=7;state.body_present=1;state.health=100;
    state.entity_flags=0x71;state.heading=350.f;
    pc_network_bot_motion_push(&motion,&state,0);
    CHECK(pc_network_bot_motion_sample(&motion,0,&out) && near(out.position[0],0));
    /* 20 Hz authoritative motion must produce all 60 Hz presentation steps.
       A no-interpolation mutation changes only 20 of these 60 observations. */
    for(i=1;i<=60;i++) {
        double now=(double)i/60.;
        if(i%3==0) {
            state.tick=i;state.position[0]=(float)now;state.bounds_center[0]=(float)now+2;
            state.clip_channels[0]=(float)now;state.scores[0]=(int16_t)i;
            pc_network_bot_motion_push(&motion,&state,now);
        }
        CHECK(pc_network_bot_motion_sample(&motion,now,&out));
        CHECK(out.position[0]>=previous-.0001f && out.position[0]<=state.position[0]+.0001f);
        if(out.position[0]>previous+.001f) changes++;
        CHECK(out.scores[0]==state.scores[0]);
        previous=out.position[0];
    }
    CHECK(changes>=55);
    CHECK(near(out.position[0],.95f));
    /* Guest body rotations and heading use degrees, not radians. A 90-degree
       turn must interpolate to45, and a north crossing follows350 ->370. */
    CHECK(near(pc_network_bot_motion_angle(0,90,.5f),45));
    CHECK(near(pc_network_bot_motion_angle(350,10,.5f),360));
    CHECK(near(pc_network_bot_motion_angle(10,350,.5f),0));
    /* Packet loss stops at the last host value rather than running guest AI. */
    CHECK(pc_network_bot_motion_sample(&motion,4,&out) && near(out.position[0],1));
    state.position[0]=20;pc_network_bot_motion_push(&motion,&state,4);
    CHECK(pc_network_bot_motion_sample(&motion,4,&out) && near(out.position[0],20));
    /* Uneven arrival: retarget from the in-flight pose, without a position jump. */
    state.position[0]=21;state.heading=10.f;
    pc_network_bot_motion_push(&motion,&state,4.05);
    pc_network_bot_motion_sample(&motion,4.07,&out);previous=out.position[0];
    CHECK(out.heading>350.f && out.heading<370.f);
    state.position[0]=22;state.health=45;state.shot.serial=8;
    pc_network_bot_motion_push(&motion,&state,4.07);
    pc_network_bot_motion_sample(&motion,4.07,&out);
    CHECK(near(out.position[0],previous) && out.health==45 && out.shot.serial==8);
    /* Death, absence, respawn, and clock reset must never use stale motion. */
    state.health=0;state.position[0]=23;pc_network_bot_motion_push(&motion,&state,4.1);
    pc_network_bot_motion_sample(&motion,4.1,&out);CHECK(out.position[0]==23 && out.health==0);
    state.body_present=0;pc_network_bot_motion_push(&motion,&state,4.15);
    pc_network_bot_motion_sample(&motion,4.15,&out);CHECK(!out.body_present);
    state.body_present=1;state.health=100;state.spawn++;state.position[0]=50;
    pc_network_bot_motion_push(&motion,&state,4.2);pc_network_bot_motion_sample(&motion,4.2,&out);
    CHECK(out.position[0]==50 && out.spawn==8);
    state.position[0]=60;pc_network_bot_motion_push(&motion,&state,0);
    pc_network_bot_motion_sample(&motion,0,&out);CHECK(out.position[0]==60);
    /* Crossfade completion is atomic. A delayed nonzero duration combined
       with the new inactive secondary clip can make 5CEF0 sample clip -1. */
    state.clip=20;state.next_clip=21;state.blend_channels[0]=1;
    state.blend_channels[1]=.5f;state.blend_channels[2]=.5f;
    pc_network_bot_motion_push(&motion,&state,1);
    state.clip=21;state.next_clip=0;
    memset(state.blend_channels,0,sizeof state.blend_channels);
    state.clip_channels[11]=2;state.overlay_channels[3]=4;
    pc_network_bot_motion_push(&motion,&state,1.05);
    pc_network_bot_motion_sample(&motion,1.06,&out);
    CHECK(out.clip==21 && out.next_clip==0);
    CHECK(!memcmp(out.blend_channels,state.blend_channels,sizeof state.blend_channels));
    CHECK(out.clip_channels[11]==2 && out.overlay_channels[3]==4);
    memset(&motion,0,sizeof motion);CHECK(!pc_network_bot_motion_sample(&motion,1,&out));
    /* Native refresh sampling uses the same 50 ms wall-clock span, without
       raising the original console clock or multiplying AI simulation. */
    const unsigned rates[]={120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) {
        unsigned rate=rates[r];
        memset(&motion,0,sizeof motion);state.position[0]=0;
        pc_network_bot_motion_push(&motion,&state,0);
        state.position[0]=1;pc_network_bot_motion_push(&motion,&state,.05);
        previous=0;
        for(unsigned frame=1;frame<=rate/20;frame++) {
            pc_network_bot_motion_sample(&motion,.05+(double)frame/rate,&out);
            CHECK(out.position[0]>previous && out.position[0]<=1);
            previous=out.position[0];
        }
        pc_network_bot_motion_sample(&motion,.11,&out);CHECK(near(out.position[0],1));
    }
    /* A crowded scene can vary its update interval. Presentation must follow
       elapsed time, not the number of rendered frames or the selected preset. */
    {
        const unsigned mixed_rates[]={240,110,180,144,60};
        double now=.05;
        memset(&motion,0,sizeof motion);state.position[0]=0;
        pc_network_bot_motion_push(&motion,&state,0);
        state.position[0]=1;pc_network_bot_motion_push(&motion,&state,.05);
        previous=0;
        for(unsigned r=0;r<sizeof mixed_rates/sizeof mixed_rates[0];r++) {
            now+=1.0/mixed_rates[r];
            CHECK(pc_network_bot_motion_sample(&motion,now,&out));
            CHECK(near(out.position[0],(float)((now-.05)/.05)));
            CHECK(out.position[0]>previous && out.position[0]<1);
            previous=out.position[0];
        }
        CHECK(pc_network_bot_motion_sample(&motion,.12,&out) && near(out.position[0],1));
    }
    {
        memset(&motion,0,sizeof motion);memset(&state,0,sizeof state);
        state.body_present=1;state.health=1;state.entity_flags=1;state.camo.flags=0x10;
        state.camo.fade_phase=2;state.camo.fade_values[2]=.5f;
        state.camo.fade_values[0]=.1f;pc_network_bot_motion_push(&motion,&state,2.);
        state.camo.fade_values[0]=.2f;pc_network_bot_motion_push(&motion,&state,2.05);
        CHECK(pc_network_bot_motion_sample(&motion,2.075,&out) && near(out.camo.fade_values[0],.15f));
        state.camo.fade_phase=1;state.camo.fade_values[0]=0;pc_network_bot_motion_push(&motion,&state,2.1);
        CHECK(pc_network_bot_motion_sample(&motion,2.125,&out) && out.camo.fade_phase==1 && out.camo.fade_values[0]==0);
        state.camo.flags=0;pc_network_bot_motion_push(&motion,&state,2.15);
        CHECK(pc_network_bot_motion_sample(&motion,2.175,&out) && !out.camo.flags);
    }
    puts("PASS: 20 Hz snapshots to continuous 60 Hz transforms, jitter, loss, angles, authority and lifecycle");
    return 0;
}
