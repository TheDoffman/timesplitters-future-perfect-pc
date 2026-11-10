/* Exercise the real native control socket, framing and checkpoint reassembly. */
#define main checkpoint_memory_fixture_main
#include "arcade_checkpoint.c"
#undef main
#include "../src/network/pc_network_platform.h"
#include "../src/network/pc_arcade_api.h"
#include "../src/network/pc_network.h"
#include "../src/network/pc_online.h"
ptrdiff_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
#define FLOAT_AT(a) (*(float *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static unsigned presents,refreshes,departures,rebases;
PcOnlineMode pc_online_mode(void){return PC_ONLINE_ARCADE;}
void pc_character_ragdoll_release(uint32_t entity){(void)entity;}
void pc_network_bots_present(uint32_t entity,const PcNetworkBotState *s)
{(void)entity;(void)s;presents++;}
void pc_network_bots_entity_refresh(uint32_t entity){(void)entity;refreshes++;}
void pc_network_bots_entity_room(uint32_t entity,uint32_t room){(void)entity;(void)room;}
void pc_native_visual_rebase(uint32_t tick,double fraction)
{REQUIRE(tick==3600u && fraction==0.);rebases++;}
void pc_multiplayer_migration_leave(void){departures++;}
#include "../src/network/pc_arcade_migration_guest.inc"

static int receive_control(NetHostSocket socket,const char *magic,uint8_t *out,
                           struct sockaddr_in *source)
{
    const uint64_t deadline=net_milliseconds()+2000;
    while(net_milliseconds()<deadline) {
        NetHostSocklen bytes=sizeof *source;
        int n=recvfrom(socket,(char *)out,4096,0,(struct sockaddr *)source,&bytes);
        if(n>=72 && !memcmp(out+68,magic,4))return n;
    }
    return -1;
}
static void bridge_reply(NetHostSocket socket,const struct sockaddr_in *destination,
                         const void *payload,size_t bytes,int valid)
{
    uint8_t frame[68+1600]={0};
    memcpy(frame,"TSFB\1\0\0\0",8);frame[8]=1;memset(frame+40,valid?0x11:0x22,16);
    const uint8_t ip[4]={10,254,0,3};memcpy(frame+56,ip,4);memcpy(frame+60,ip,4);
    memcpy(frame+68,payload,bytes);
    REQUIRE(sendto(socket,(const char *)frame,(int)(68+bytes),0,
            (const struct sockaddr *)destination,sizeof *destination)==(int)(68+bytes));
}
int main(void)
{
    REQUIRE(net_start());
    NetHostSocket bridge=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);REQUIRE(bridge!=INVALID_SOCKET);
    struct sockaddr_in address={0},native={0};NetHostSocklen address_bytes=sizeof address;
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    REQUIRE(!bind(bridge,(struct sockaddr *)&address,sizeof address));
    REQUIRE(!getsockname(bridge,(struct sockaddr *)&address,&address_bytes));
    REQUIRE(!net_nonblocking(bridge));
    {int buffer=262144;setsockopt(bridge,SOL_SOCKET,SO_RCVBUF,(const char *)&buffer,sizeof buffer);}
    char configuration[128];snprintf(configuration,sizeof configuration,
        "%u 11111111111111111111111111111111 10.254.0.3",(unsigned)ntohs(address.sin_port));
    REQUIRE(pc_network_set_online(configuration));
    PcArcadeControlState observed;REQUIRE(!pc_network_arcade_progress(0,&observed));
    uint8_t frame[4096];REQUIRE(receive_control(bridge,"AMPG",frame,&native)==80);
    REQUIRE(pc_arcade_load32(frame+76)==0u);
    uint8_t status[84]={0};memcpy(status,"AMST",4);pc_arcade_store32(status+4,1);
    pc_arcade_store32(status+8,1);const uint8_t local_ip[4]={10,254,0,3};
    memcpy(status+20,local_ip,4);
    bridge_reply(bridge,&native,status,sizeof status,0);
    REQUIRE(!pc_network_arcade_progress(1,&observed));
    bridge_reply(bridge,&native,status,sizeof status,1);
    REQUIRE(pc_network_arcade_progress(1,&observed));REQUIRE(observed.epoch==1 && observed.phase==0);
    Fixture source,target;init(&source,0x10000u);init(&target,0x50000u);
    const uint8_t room[16]={1,2,3,4,5,6};
    g_xbox_mem_offset=(ptrdiff_t)target.memory;
    setword(&target,0x76b140u,2u);setword(&target,0x774028u,0x70000u);
    setword(&target,0x70000u,5u);setword(&target,0x774040u,0u);
    const uint32_t previous_ip=0x0100fe0au;
    memcpy(target.memory+0x8000u,&previous_ip,4);memset(target.memory+0x8100u,0x55,8);
    uint32_t convert[3]={0x8000u,0x8100u,0x8200u};
    REQUIRE(!pc_network_call(0x43172cu,convert,g_xbox_mem_offset));
    PcArcadeCheckpoint *snapshot=(PcArcadeCheckpoint *)malloc(sizeof *snapshot);
    PcArcadeCheckpoint *download=(PcArcadeCheckpoint *)malloc(sizeof *download);REQUIRE(snapshot && download);
    REQUIRE(pc_arcade_checkpoint_capture(snapshot,&source,read_guest,bot_read,room,4,99));
    pc_arcade_store32(status+8,5);pc_arcade_store32(status+12,2);pc_arcade_store32(status+16,99);
    pc_arcade_store32(status+32,sizeof *snapshot);memset(status+36,0x33,32);memcpy(status+68,room,16);
    memcpy(status+24,local_ip,4);memcpy(status+28,&previous_ip,4);
    bridge_reply(bridge,&native,status,sizeof status,1);
    REQUIRE(pc_network_arcade_progress(1,&observed) && g_tsfp_arcade_migrating);
    uint8_t part[52+PC_ARCADE_FRAGMENT];memcpy(part,"AMFR",4);pc_arcade_store32(part+4,99);
    pc_arcade_store32(part+8,sizeof *snapshot);pc_arcade_store32(part+16,pc_arcade_crc32(snapshot,sizeof *snapshot));
    memset(part+20,0x33,32);
    for(uint32_t offset=0;offset<sizeof *snapshot;offset+=PC_ARCADE_FRAGMENT) {
        const size_t count=sizeof *snapshot-offset<PC_ARCADE_FRAGMENT?sizeof *snapshot-offset:PC_ARCADE_FRAGMENT;
        pc_arcade_store32(part+12,offset);memcpy(part+52,(const uint8_t *)snapshot+offset,count);
        bridge_reply(bridge,&native,part,52u+count,1);
    }
    REQUIRE(pc_network_arcade_progress(1,&observed));
    REQUIRE(pc_network_arcade_snapshot(download) && !memcmp(snapshot,download,sizeof *snapshot));
    REQUIRE(pc_arcade_migration_frame(0)==1); /* Apply checkpoint while frozen. */
    REQUIRE(WORD_AT(0x76b140u)==2u && WORD_AT(0x7de318u)==3600u && rebases==1u);
    REQUIRE((int32_t)WORD_AT(0x50000u+0xa6cu)==snapshot->totals[0]);
    REQUIRE(presents==6u && refreshes==5u); /* Five live bots, one absent/dead. */
    for(unsigned i=0;i<5u;i++) {
        const uint32_t entity=word(&target,0x50000u+(3u+i)*0xea0u);
        REQUIRE(FLOAT_AT(entity+0x1ccu)==snapshot->bots[i].health);
        REQUIRE(!memcmp(target.memory+entity+0x64u,snapshot->bots[i].position,12));
    }
    REQUIRE(receive_control(bridge,"AMAK",frame,&native)==112);
    REQUIRE(pc_arcade_load32(frame+72)==99 && pc_arcade_load32(frame+108)==1);
    REQUIRE(!pc_network_arcade_request_leave()); /* Cannot depart as host during preparation. */
    pc_arcade_store32(status+12,1);bridge_reply(bridge,&native,status,sizeof status,1);
    REQUIRE(pc_arcade_migration_frame(0)==0);
    REQUIRE(!g_tsfp_arcade_migrating && WORD_AT(0x76b140u)==1u && WORD_AT(0x774040u)==5u);
    REQUIRE(WORD_AT(0x774028u)==0x70000u); /* Original local controller node survives. */
    REQUIRE(pc_network_arcade_request_leave() && g_tsfp_arcade_migrating);
    REQUIRE(pc_network_arcade_departing());pc_network_arcade_send_leave();
    REQUIRE(receive_control(bridge,"AMLV",frame,&native)==72);
    REQUIRE(pc_network_arcade_publish(snapshot));
    REQUIRE(receive_control(bridge,"AMCP",frame,&native)>88);
    REQUIRE(pc_arcade_load32(frame+72)==99 && pc_arcade_load32(frame+76)==sizeof *snapshot);
    ptrdiff_t offset=(ptrdiff_t)source.memory;uint32_t value=0;
    REQUIRE(pc_network_arcade_read(&offset,0x76b140u,&value,4) && value==1u);
    value=2;REQUIRE(pc_network_arcade_write(&offset,0x76b140u,&value,4,0));
    REQUIRE(word(&source,0x76b140u)==1u);
    REQUIRE(pc_network_arcade_write(&offset,0x76b140u,&value,4,1));
    REQUIRE(word(&source,0x76b140u)==2u);
    REQUIRE(!pc_network_arcade_adopt_key(0x9900fe0au));
    pc_network_set_online(NULL);REQUIRE(!g_tsfp_arcade_migrating);
    REQUIRE(!pc_network_arcade_progress(1,&observed));
    net_close(bridge);net_stop();free(snapshot);free(download);free(source.memory);free(target.memory);
    puts("PASS: Arcade checkpoint control socket; authenticated progress, fragments, ACK and departure");return 0;
}
