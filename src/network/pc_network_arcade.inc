/* Included after the shared transport implementation; own loopback control
 * socket never drains or steals original game datagrams. */
#ifdef TSFP_ARCADE_MIGRATION
#include "pc_arcade_api.h"
int g_tsfp_arcade_migrating;
static NetHostSocket arcade_socket=INVALID_SOCKET;
static PcArcadeControl arcade_control;
static uint32_t arcade_counter;
static uint32_t arcade_uploaded_sequence;
static uint32_t arcade_native_id=UINT32_MAX;
static uint32_t arcade_slots[4];
static uint64_t arcade_last_send,arcade_last_state;
static int arcade_leave_pending;
static void arcade_reset(void)
{
    if(arcade_socket!=INVALID_SOCKET)net_close(arcade_socket);
    arcade_socket=INVALID_SOCKET;
    memset(&arcade_control,0,sizeof arcade_control);
    /* Stock transport cleanup can occur between rounds while the companion
       membership survives. Its engine-progress counter must never rewind. */
    arcade_uploaded_sequence=0;arcade_last_send=arcade_last_state=0;
    arcade_native_id=UINT32_MAX;
    memset(arcade_slots,0,sizeof arcade_slots);
    arcade_leave_pending=0;g_tsfp_arcade_migrating=0;
}
static int arcade_send(const void *data,int bytes)
{
    if(!online || arcade_socket==INVALID_SOCKET)return 0;
    NetSocket s={arcade_socket,0,0};struct sockaddr_in to={0};
    to.sin_family=AF_INET;to.sin_addr.s_addr=online_ip;
    return bridge_send(&s,(const char *)data,bytes,0,&to)==bytes;
}
int pc_network_arcade_progress(int active,PcArcadeControlState *out)
{
    int result=0;net_lock(&network_lock);
    if(!online || !initialize())goto done;
    if(arcade_socket==INVALID_SOCKET) {
        struct sockaddr_in address={0};
        address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        arcade_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(arcade_socket==INVALID_SOCKET)goto done;
        net_exclusive(arcade_socket);
        {int receive_bytes=262144;setsockopt(arcade_socket,SOL_SOCKET,SO_RCVBUF,(const char *)&receive_bytes,sizeof receive_bytes);}
        if(bind(arcade_socket,(struct sockaddr *)&address,sizeof address) || net_nonblocking(arcade_socket)) {
            net_close(arcade_socket);arcade_socket=INVALID_SOCKET;goto done;
        }
    }
    const uint64_t now=net_milliseconds();
    ++arcade_counter;
    if(!arcade_last_send || now-arcade_last_send>=200u) {
        uint8_t pulse[32];memcpy(pulse,"AMPG",4);
        pc_arcade_store32(pulse+4,arcade_counter);pc_arcade_store32(pulse+8,!!active);
        pc_arcade_store32(pulse+12,arcade_native_id);
        for(unsigned i=0;i<4u;i++)pc_arcade_store32(pulse+16u+i*4u,arcade_slots[i]);
        arcade_send(pulse,sizeof pulse);arcade_last_send=now;
    }
    for(unsigned attempt=0;attempt<64u;attempt++) {
        uint8_t packet[BRIDGE_HEADER+1600];struct sockaddr_in address;NetHostSocklen size=sizeof address;
        int n=recvfrom(arcade_socket,(char *)packet,sizeof packet,0,(struct sockaddr *)&address,&size);
        if(n==SOCKET_ERROR)break;
        if(n<BRIDGE_HEADER || address.sin_addr.s_addr!=bridge_address.sin_addr.s_addr ||
           address.sin_port!=bridge_address.sin_port || memcmp(packet,"TSFB\1\0\0\0",8) ||
           memcmp(packet+8,compatibility,32) || memcmp(packet+40,online_token,16) ||
           memcmp(packet+56,&online_ip,4) || memcmp(packet+60,&online_ip,4) ||
           packet[64] || packet[65] || packet[66] || packet[67])continue;
        if(pc_arcade_control_receive(&arcade_control,packet+BRIDGE_HEADER,(size_t)n-BRIDGE_HEADER) &&
           n==BRIDGE_HEADER+PC_ARCADE_STATE_BYTES)arcade_last_state=now;
    }
    if(arcade_control.enabled) {
        if(now-arcade_last_state>5000u)arcade_control.state.phase=3u;
        g_tsfp_arcade_migrating=arcade_leave_pending || arcade_control.state.phase==2u;
        if(out)*out=arcade_control.state;result=1;
    }
done:net_unlock(&network_lock);return result;
}
void pc_network_arcade_native_peer(unsigned native_peer)
{net_lock(&network_lock);arcade_native_id=native_peer<16u?native_peer:UINT32_MAX;net_unlock(&network_lock);}
void pc_network_arcade_slots(const uint32_t slots[4])
{net_lock(&network_lock);if(slots)memcpy(arcade_slots,slots,sizeof arcade_slots);else memset(arcade_slots,0,sizeof arcade_slots);net_unlock(&network_lock);}
int pc_network_arcade_enabled(void)
{int enabled;net_lock(&network_lock);enabled=online && arcade_control.enabled && (arcade_control.state.phase<3u || arcade_control.state.phase==5u);net_unlock(&network_lock);return enabled;}
int pc_network_arcade_bind_peer(uint32_t ip,uint32_t previous_host)
{
    int result=0;unsigned source,target;net_lock(&network_lock);
    if(!online || (ntohl(ip)&0xffffff00u)!=0x0afe0000u || !(ntohl(ip)&255u) || (ntohl(ip)&255u)==255u)goto done;
    if(ip==online_ip || ip==previous_host){result=1;goto done;}
    for(source=0;source<peer_count;source++)if(peers[source].ip==previous_host)break;
    if(source==peer_count)goto done;
    for(target=0;target<peer_count;target++)if(peers[target].ip==ip)break;
    if(target==peer_count) {
        if(peer_count==MAX_PEERS)goto done;
        memset(&peers[target],0,sizeof peers[target]);peers[target].ip=ip;
        memcpy(peers[target].xnaddr,&ip,4);peer_count++;
    }
    memcpy(peers[target].key,peers[source].key,8);result=1;
done:net_unlock(&network_lock);return result;
}
int pc_network_arcade_snapshot(PcArcadeCheckpoint *out)
{
    int result=0;net_lock(&network_lock);
    if(arcade_control.complete && arcade_control.checkpoint.sequence==arcade_control.state.sequence &&
       !memcmp(arcade_control.digest,arcade_control.state.digest,32)) {
        if(out)*out=arcade_control.checkpoint;result=1;
    }
    net_unlock(&network_lock);return result;
}
int pc_network_arcade_publish(const PcArcadeCheckpoint *snapshot)
{
    uint8_t packet[20+PC_ARCADE_FRAGMENT];int result=1;
    net_lock(&network_lock);
    if(!snapshot || !arcade_control.enabled ||
       !pc_arcade_checkpoint_valid(snapshot,arcade_control.state.room)) {result=0;goto done;}
    const uint8_t *bytes=(const uint8_t *)snapshot;
    const uint32_t total=sizeof *snapshot,crc=pc_arcade_crc32(snapshot,sizeof *snapshot);
    memcpy(packet,"AMCP",4);pc_arcade_store32(packet+4,snapshot->sequence);
    pc_arcade_store32(packet+8,total);pc_arcade_store32(packet+16,crc);
    for(uint32_t offset=0;offset<total;offset+=PC_ARCADE_FRAGMENT) {
        const uint32_t n=total-offset<PC_ARCADE_FRAGMENT?total-offset:PC_ARCADE_FRAGMENT;
        pc_arcade_store32(packet+12,offset);memcpy(packet+20,bytes+offset,n);
        if(!arcade_send(packet,(int)(20u+n)))result=0;
    }
    if(result)arcade_uploaded_sequence=snapshot->sequence;
done:net_unlock(&network_lock);return result;
}
void pc_network_arcade_ack(int ready)
{
    net_lock(&network_lock);
    if(arcade_control.complete && arcade_control.checkpoint.sequence==arcade_control.state.sequence &&
       !memcmp(arcade_control.digest,arcade_control.state.digest,32)) {
        uint8_t ack[44];memcpy(ack,"AMAK",4);
        pc_arcade_store32(ack+4,arcade_control.state.sequence);
        memcpy(ack+8,arcade_control.state.digest,32);pc_arcade_store32(ack+40,!!ready);
        arcade_send(ack,sizeof ack);
    }
    net_unlock(&network_lock);
}
int pc_network_arcade_request_leave(void)
{
    int result=0;net_lock(&network_lock);
    if(online && arcade_control.enabled && arcade_control.state.phase==1u &&
       arcade_control.state.host_ip==online_ip && arcade_control.state.sequence) {
        arcade_leave_pending=1;g_tsfp_arcade_migrating=1;result=1;
    }
    net_unlock(&network_lock);return result;
}
void pc_network_arcade_send_leave(void)
{
    net_lock(&network_lock);
    if(arcade_leave_pending && arcade_uploaded_sequence) {
        uint8_t leave[8];memcpy(leave,"AMLV",4);
        pc_arcade_store32(leave+4,arcade_uploaded_sequence);arcade_send(leave,sizeof leave);
    }
    net_unlock(&network_lock);
}
int pc_network_arcade_departing(void)
{int result;net_lock(&network_lock);result=arcade_leave_pending;net_unlock(&network_lock);return result;}
uint32_t pc_network_arcade_local_ip(void)
{uint32_t result;net_lock(&network_lock);result=online?online_ip:0;net_unlock(&network_lock);return result;}
int pc_network_arcade_adopt_key(uint32_t old_host)
{
    int result=0;net_lock(&network_lock);
    if(online)for(unsigned i=0;i<peer_count;i++)if(peers[i].ip==old_host) {
        memcpy(local_session_id,peers[i].key,8);
        local_session_created=local_session_registered=1;result=1;break;
    }
    net_unlock(&network_lock);return result;
}
int pc_network_arcade_read(void *ctx,uint32_t address,void *out,size_t bytes)
{
    const ptrdiff_t offset=*(const ptrdiff_t *)ctx;
    if(bytes>UINT32_MAX)return 0;
    void *source=pointer(address,(uint32_t)bytes,offset,0);
    if(!source)return 0;memcpy(out,source,bytes);return 1;
}
int pc_network_arcade_write(void *ctx,uint32_t address,const void *value,size_t bytes,int apply)
{
    const ptrdiff_t offset=*(const ptrdiff_t *)ctx;
    if(bytes>UINT32_MAX)return 0;
    void *target=pointer(address,(uint32_t)bytes,offset,1);
    if(!target)return 0;if(apply)memcpy(target,value,bytes);return 1;
}
int pc_network_arcade_bot_read(void *ctx,unsigned slot,PcNetworkBotState *out)
{
    int result=0;(void)ctx;net_lock(&network_lock);
    if(slot<16u && bot_seen[slot]){*out=bot_pending[slot];result=1;}
    net_unlock(&network_lock);return result;
}
#endif
