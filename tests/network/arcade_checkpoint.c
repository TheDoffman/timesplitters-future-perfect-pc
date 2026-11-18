/* Production checkpoint policy with deliberately different native allocations.
 * This does not emulate the missing engine authority handoff or live bot AI.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/network/pc_arcade_control.h"
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define BYTES 0x800000u
typedef struct Fixture {
    uint8_t *memory;
    PcNetworkBotState bots[10];
    unsigned missing,invalid_destination,writes,bot_writes;
    uint32_t readonly;
} Fixture;
static uint32_t word(Fixture *f,uint32_t address)
{uint32_t value;memcpy(&value,f->memory+address,4);return value;}
static void setword(Fixture *f,uint32_t address,uint32_t value)
{memcpy(f->memory+address,&value,4);}
static int read_guest(void *ctx,uint32_t address,void *out,size_t bytes)
{
    Fixture *f=(Fixture *)ctx;
    if(address>BYTES || bytes>BYTES-address)return 0;
    memcpy(out,f->memory+address,bytes);return 1;
}
static int write_guest(void *ctx,uint32_t address,const void *value,size_t bytes,int apply)
{
    Fixture *f=(Fixture *)ctx;
    if(address>BYTES || bytes>BYTES-address || (f->readonly && address==f->readonly))return 0;
    if(apply){memcpy(f->memory+address,value,bytes);f->writes++;}return 1;
}
static int bot_read(void *ctx,unsigned slot,PcNetworkBotState *out)
{
    Fixture *f=(Fixture *)ctx;
    if(slot<3u || slot>=9u || slot==f->missing)return 0;
    *out=f->bots[slot-3u];return 1;
}
static int bot_apply(void *ctx,const PcNetworkBotState *state,int apply)
{
    Fixture *f=(Fixture *)ctx;
    if(f->invalid_destination || state->slot<3u || state->slot>=9u)return 0;
    if(apply){f->bots[state->slot-3u]=*state;f->bot_writes++;}return 1;
}
static void init(Fixture *f,uint32_t pool)
{
    memset(f,0,sizeof *f);f->memory=(uint8_t *)calloc(1,BYTES);REQUIRE(f->memory);
    setword(f,0x76b108u,1);setword(f,0x76b140u,1);setword(f,0x76b1a4u,15);
    setword(f,0x79094cu,48);setword(f,0x7de30cu,7);setword(f,0x7de318u,3600);
    setword(f,0x7356d8u,pool);
    f->memory[0x7de400u]=3;f->memory[0x7de401u]=1;f->memory[0x7de402u]=6;
    f->memory[0x7de455u]=11;
    setword(f,0x7de460u,32400u);
    for(unsigned actor=0;actor<9u;actor++) {
        setword(f,pool+actor*0xea0u,0x30000u+actor*0x1000u);
        setword(f,pool+actor*0xea0u+0x18u,actor==8u?2u:1u);
        setword(f,0x30000u+actor*0x1000u+0x7cu,pool+actor*0xea0u);
        for(unsigned score=0;score<42u;score++) {
            const int16_t value=(int16_t)((int)actor*100+(int)score-20);
            memcpy(f->memory+pool+actor*0xea0u+0xa10u+score*2u,&value,2);
        }
        setword(f,pool+actor*0xea0u+0xa6cu,1234u+actor);
        setword(f,pool+actor*0xea0u+0xa64u,3000u+actor);
    }
    for(unsigned i=0;i<6u;i++) {
        PcNetworkBotState *s=&f->bots[i];s->slot=3u+i;s->tick=3600u;
        s->body_present=1;s->health=100.f-(float)i;s->armor=(float)i;
        s->animation=1;s->position[0]=(float)(10u+i);s->position[1]=20.f;
        s->spawn=50u+i;s->primary_room=3u;s->entity_flags=0x71;
        memcpy(s->scores,f->memory+pool+s->slot*0xea0u+0xa10u,sizeof s->scores);
        memcpy(s->life,f->memory+pool+s->slot*0xea0u+0x10u,sizeof s->life);
        for(unsigned actor=0;actor<9u;actor++)
            memcpy(&s->killed_by[actor],f->memory+pool+actor*0xea0u+0xa10u+s->slot*2u,2);
    }
    /* Dead/absent bot state remains meaningful, including its score row. */
    f->bots[5].body_present=0;f->bots[5].health=0;
}
int main(int argc,char **argv)
{
    Fixture source,target;init(&source,0x10000u);init(&target,0x50000u);
    const uint8_t room[16]={1,2,3,4,5,6};
    PcArcadeCheckpoint *snapshot=(PcArcadeCheckpoint *)malloc(sizeof *snapshot);
    PcArcadeCheckpoint *broken=(PcArcadeCheckpoint *)malloc(sizeof *broken);
    REQUIRE(snapshot && broken);
    REQUIRE(pc_arcade_crc32("123456789",9)==0xcbf43926u);
    REQUIRE(pc_arcade_checkpoint_capture(snapshot,&source,read_guest,bot_read,room,4,99));
    REQUIRE(pc_arcade_checkpoint_valid(snapshot,room));
    if(argc==2) {
        FILE *file=fopen(argv[1],"wb");REQUIRE(file);
        REQUIRE(fwrite(snapshot,sizeof *snapshot,1,file)==1);REQUIRE(!fclose(file));
    }
    REQUIRE(snapshot->sequence==99 && snapshot->epoch==4 && snapshot->tick==3600u);
    REQUIRE(snapshot->bot_mask==63u && snapshot->bots[5].body_present==0);
    PcArcadeControl *control=(PcArcadeControl *)calloc(1,sizeof *control);REQUIRE(control);
    uint8_t status[PC_ARCADE_STATE_BYTES]={0};memcpy(status,"AMST",4);pc_arcade_store32(status+4,1);
    pc_arcade_store32(status+8,5);pc_arcade_store32(status+12,2);pc_arcade_store32(status+16,99);
    pc_arcade_store32(status+32,sizeof *snapshot);memset(status+36,0x33,32);memcpy(status+68,room,16);
    REQUIRE(pc_arcade_control_receive(control,status,sizeof status));
    uint8_t part[52+PC_ARCADE_FRAGMENT];memcpy(part,"AMFR",4);pc_arcade_store32(part+4,99);
    pc_arcade_store32(part+8,sizeof *snapshot);pc_arcade_store32(part+16,pc_arcade_crc32(snapshot,sizeof *snapshot));
    memset(part+20,0x33,32);
    /* Reverse fragment order, duplicate delivery and corrupt/mismatched parts. */
    const unsigned chunks=((unsigned)sizeof *snapshot+PC_ARCADE_FRAGMENT-1)/PC_ARCADE_FRAGMENT;
    for(unsigned i=chunks;i;i--) {
        const unsigned offset=(i-1u)*PC_ARCADE_FRAGMENT;
        const unsigned count=sizeof *snapshot-offset<PC_ARCADE_FRAGMENT?(unsigned)sizeof *snapshot-offset:PC_ARCADE_FRAGMENT;
        pc_arcade_store32(part+12,offset);memcpy(part+52,(const uint8_t *)snapshot+offset,count);
        REQUIRE(pc_arcade_control_receive(control,part,52u+count));
        REQUIRE(pc_arcade_control_receive(control,part,52u+count));
    }
    REQUIRE(control->complete && !memcmp(snapshot,&control->checkpoint,sizeof *snapshot));
    pc_arcade_store32(status+8,4);REQUIRE(!pc_arcade_control_receive(control,status,sizeof status));
    pc_arcade_store32(status+8,5);pc_arcade_store32(status+12,1);
    REQUIRE(pc_arcade_control_receive(control,status,sizeof status));
    pc_arcade_store32(status+12,2);REQUIRE(!pc_arcade_control_receive(control,status,sizeof status));
    free(control);
    memset(target.memory+0x50000,0x55,9u*0xea0u);
    setword(&target,0x76b140u,2);setword(&target,0x7de318u,7200);
    setword(&target,0x7de460u,31234u);
    target.memory[0x7de401u]=2;target.memory[0x7de424u]=1;
    const uint32_t pointer_before=word(&target,0x50000u);
    REQUIRE(pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    REQUIRE(word(&target,0x7de318u)==3600u && target.bot_writes==6u);
    REQUIRE(word(&target,0x7de460u)==32400u);
    REQUIRE(target.memory[0x7de401u]==2u && target.memory[0x7de424u]==1u);
    REQUIRE(word(&target,0x50000u)==pointer_before); /* Keep local actor pointers. */
    REQUIRE(word(&target,0x76b140u)==2); /* Restore never guesses/promotes authority. */
    for(unsigned actor=0;actor<9u;actor++) {
        const uint32_t a=0x50000u+actor*0xea0u,b=0x10000u+actor*0xea0u;
        REQUIRE(!memcmp(target.memory+a+0xa10u,source.memory+b+0xa10u,84));
        REQUIRE(word(&target,a+0xa6cu)==word(&source,b+0xa6cu));
        REQUIRE(word(&target,a+0xa64u)==word(&source,b+0xa64u));
    }
    REQUIRE(!memcmp(target.bots,source.bots,6u*sizeof source.bots[0]));
    const unsigned writes=target.writes,bot_writes=target.bot_writes;
    target.readonly=0x50000u+8u*0xea0u+0xa6cu; /* Last row fails before first write. */
    REQUIRE(!pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    REQUIRE(target.writes==writes && target.bot_writes==bot_writes);
    target.readonly=0x7de460u;
    REQUIRE(!pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    REQUIRE(target.writes==writes && target.bot_writes==bot_writes);
    target.readonly=0;target.invalid_destination=1;
    REQUIRE(!pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    REQUIRE(target.writes==writes && target.bot_writes==bot_writes);
    target.invalid_destination=0;setword(&target,0x79094cu,49);
    REQUIRE(!pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    REQUIRE(target.writes==writes);
    setword(&target,0x79094cu,48);
    target.memory[0x7de402u]=5;
    REQUIRE(!pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    target.memory[0x7de402u]=6;
    target.memory[0x7de455u]^=1u;
    REQUIRE(!pc_arcade_checkpoint_restore(snapshot,&target,read_guest,write_guest,bot_apply,room));
    target.memory[0x7de455u]^=1u;
    uint8_t team_rules[PC_ARCADE_CHECKPOINT_CONFIG_BYTES];memcpy(team_rules,snapshot->config,sizeof team_rules);
    team_rules[0x58u]|=1u;target.memory[0x7de458u]|=1u;
    REQUIRE(!pc_arcade_config_matches(team_rules,target.memory+0x7de400u));
    target.memory[0x7de458u]&=~1u;
    target.memory[0x7de45du]^=0x40u;
    REQUIRE(pc_arcade_config_matches(snapshot->config,target.memory+0x7de400u));
    target.memory[0x7de45du]^=0x80u;
    REQUIRE(!pc_arcade_config_matches(snapshot->config,target.memory+0x7de400u));
    target.memory[0x7de45du]^=0xc0u;
    for(unsigned mutation=0;mutation<8u;mutation++) {
        *broken=*snapshot;
        switch(mutation) {
        case 0:broken->schema++;break;
        case 1:broken->bytes--;break;
        case 2:broken->room[15]^=1;break;
        case 3:broken->config[2]=11;break;
        case 4:broken->bots[0].position[0]=NAN;break;
        case 5:broken->bots[0].scores[0]++;break;
        case 6:broken->bots[0].killed_by[0]++;break;
        default:broken->bots[0].tick++;break;
        }
        REQUIRE(!pc_arcade_checkpoint_valid(broken,room));
        REQUIRE(!pc_arcade_checkpoint_restore(broken,&target,read_guest,write_guest,bot_apply,room));
        REQUIRE(target.writes==writes && target.bot_writes==bot_writes);
    }
    source.missing=8;memset(broken,0x42,sizeof *broken);
    REQUIRE(!pc_arcade_checkpoint_capture(broken,&source,read_guest,bot_read,room,4,100));
    REQUIRE(((uint8_t *)broken)[0]==0x42); /* Incomplete capture does not publish. */
    source.missing=0;setword(&source,0x76b140u,2);
    REQUIRE(!pc_arcade_checkpoint_capture(broken,&source,read_guest,bot_read,room,4,100));
    /* Companion peer2 resolves to native peer9, not player/controller slot2. */
    setword(&target,0x774028u,0x70000u);setword(&target,0x70000u,5u);
    const uint32_t local_ip=0x0300fe0au,host_ip=0x0200fe0au;
    PcArcadeControlState routes={0};routes.candidate_ip=host_ip;routes.native_count=2;
    routes.native_peers[0]=(PcArcadeNativePeer){5u,local_ip};
    routes.native_peers[1]=(PcArcadeNativePeer){9u,host_ip};
    setword(&target,0x774040u,0);
    target.memory[0x666488u]=2;target.memory[0x66648bu]=1;
    setword(&target,0x774180u+9u*0x30cu,9u);
    setword(&target,0x7741b4u+9u*0x30cu,1u);
    setword(&target,0x666040u+9u*4u,1u);
    const unsigned route_writes=target.writes;
    target.readonly=0x666488u+9u*16u;
    REQUIRE(!pc_arcade_prepare_peer_routes(&target,read_guest,write_guest,local_ip,&routes));
    REQUIRE(target.writes==route_writes);
    target.readonly=0;
    REQUIRE(pc_arcade_prepare_peer_routes(&target,read_guest,write_guest,local_ip,&routes));
    REQUIRE(word(&target,0x66648cu+9u*16u)==host_ip);
    REQUIRE(!memcmp(target.memory+0x666488u,target.memory+0x666488u+9u*16u,4));
    routes.native_peers[1].native_id=8u;
    REQUIRE(!pc_arcade_prepare_peer_routes(&target,read_guest,write_guest,local_ip,&routes));
    routes.native_peers[1].native_id=9u;
    setword(&target,0x66648cu+9u*16u,0x9900fe0au);
    REQUIRE(!pc_arcade_prepare_peer_routes(&target,read_guest,write_guest,local_ip,&routes));
    setword(&target,0x666040u+9u*4u,1u);setword(&target,0x66648cu+9u*16u,host_ip);
    unsigned adopted_role=0;
    REQUIRE(pc_arcade_adopt_authority(&target,read_guest,write_guest,local_ip,host_ip,&adopted_role,0));
    REQUIRE(adopted_role==2u && word(&target,0x76b140u)==2u);
    REQUIRE(pc_arcade_adopt_authority(&target,read_guest,write_guest,local_ip,host_ip,&adopted_role,1));
    REQUIRE(word(&target,0x774040u)==9u);
    REQUIRE(pc_arcade_adopt_authority(&target,read_guest,write_guest,local_ip,local_ip,&adopted_role,1));
    REQUIRE(adopted_role==1u && word(&target,0x774040u)==5u && word(&target,0x76b140u)==1u);
    REQUIRE(word(&target,0x7de318u)==3600u && word(&target,0x50000u)==pointer_before);
    printf("PASS: Arcade checkpoint %zu bytes; scores, timer, six bots and guarded restoration\n",sizeof *snapshot);
    free(snapshot);free(broken);free(source.memory);free(target.memory);return 0;
}
