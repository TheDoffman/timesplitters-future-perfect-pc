#define main checkpoint_fixture_main
#include "arcade_checkpoint.c"
#undef main
#include "../src/network/pc_arcade_join.h"

int main(void)
{
    unsigned humans=99,bots=99;
    REQUIRE(pc_arcade_join_population(2,6,7,8,&humans,&bots));
    REQUIRE(humans==3 && bots==5);
    REQUIRE(pc_arcade_join_population(3,5,5,8,&humans,&bots));
    REQUIRE(humans==3 && bots==5); /* a human hole never moves actors */
    REQUIRE(pc_arcade_join_population(1,0,3,8,&humans,&bots));
    REQUIRE(humans==2 && bots==0);
    REQUIRE(!pc_arcade_join_population(8,0,0x1ff,8,&humans,&bots));
    REQUIRE(!pc_arcade_join_population(16,1,1,16,&humans,&bots));
    REQUIRE(!pc_arcade_join_population(2,6,0x10003,16,&humans,&bots));
    Fixture f;init(&f,0x50000u);
    setword(&f,0x78ad2cu,11);setword(&f,0x78ad4cu,0);
    setword(&f,0x4d239cu+8u,16);
    const uint32_t addresses[]={0x7de400u,0x7de340u,0x774044u,0x78ad58u};
    for(unsigned i=0;i<4u;i++)memcpy(f.memory+addresses[i],f.memory+0x7de400u,0xa0);
    for(unsigned slot=0;slot<4u;slot++) {
        setword(&f,0x777244u+slot*0x124u,0x774180u);
        setword(&f,0x77728cu+slot*0x124u,1);
    }
    uint8_t before[0x9000];memcpy(before,f.memory+0x50000u,sizeof before);
    f.readonly=0x7a2958u;
    REQUIRE(!pc_arcade_join_sync(&f,read_guest,write_guest) && !f.writes);
    REQUIRE(f.memory[0x7de400u]==3 && f.memory[0x7de402u]==6);
    f.readonly=0;
    REQUIRE(pc_arcade_join_sync(&f,read_guest,write_guest));
    REQUIRE(word(&f,0x790950u)==4 && word(&f,0x7a2958u)==5);
    for(unsigned i=0;i<4u;i++) {
        REQUIRE(f.memory[addresses[i]]==4 && f.memory[addresses[i]+2u]==5);
        REQUIRE(f.memory[addresses[i]+1u]==1);
    }
    REQUIRE(!memcmp(before,f.memory+0x50000u,sizeof before));
    REQUIRE(word(&f,0x7de318u)==3600u && word(&f,0x7de460u)==32400u);
    unsigned writes=f.writes;
    REQUIRE(!pc_arcade_join_sync(&f,read_guest,write_guest) && f.writes==writes);
    uint32_t slots[4];
    setword(&f,0x774028u,0x774180u);setword(&f,0x77418cu,0x777240u);
    REQUIRE(pc_arcade_join_slots(&f,read_guest,slots));
    REQUIRE(slots[0]==16 && slots[1]==15 && slots[2]==0x1f0 && slots[3]==0);
    REQUIRE(pc_arcade_join_fresh_score(&f,read_guest,write_guest,3));
    REQUIRE(word(&f,0x50000u+3u*0xea0u+0xa6cu)==0);
    for(unsigned i=0;i<42u;i++)REQUIRE(!f.memory[0x50000u+3u*0xea0u+0xa10u+i*2u] &&
        !f.memory[0x50000u+3u*0xea0u+0xa11u+i*2u]);
    REQUIRE(!memcmp(before,f.memory+0x50000u,3u*0xea0u));
    REQUIRE(!memcmp(before+4u*0xea0u,f.memory+0x50000u+4u*0xea0u,sizeof before-4u*0xea0u));
    REQUIRE(word(&f,0x7de318u)==3600u && word(&f,0x7de460u)==32400u);
    PcArcadeCheckpoint team={0};team.config[0]=4;team.config[0x58]=1;team.config[0x24]=2;
    REQUIRE(pc_arcade_join_team(&f,read_guest,write_guest,&team));
    REQUIRE(word(&f,0x777288u)==2 && word(&f,0x50000u+0x84u)==2 && f.memory[0x7de424u]==2);
    free(f.memory);
    puts("PASS: Arcade checkpoint join population preserves actors, scores and timer");
    return 0;
}
