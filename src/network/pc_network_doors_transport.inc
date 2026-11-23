/* Called under network_lock by the existing compatible LAN/Online decoder. */
typedef struct PcDoorContext {uint32_t role,host_ip,match;} PcDoorContext;
static PcDoorContext door_context;
static PcNetworkDoorState door_pending[PC_NETWORK_DOORS_MAX];
static uint32_t door_ticks[PC_NETWORK_DOORS_MAX],door_targets[16];
static unsigned char door_seen[PC_NETWORK_DOORS_MAX],door_fresh[PC_NETWORK_DOORS_MAX];
static unsigned door_target_count;
static void door_reset(void)
{
    memset(&door_context,0,sizeof door_context);door_target_count=0;
    memset(door_seen,0,sizeof door_seen);memset(door_fresh,0,sizeof door_fresh);
}
void pc_network_doors_context(unsigned role,uint32_t host_ip,uint32_t match)
{
    const PcDoorContext next={role,host_ip,match};
    net_lock(&network_lock);
    if(memcmp(&next,&door_context,sizeof next)) {
        door_reset();door_context=next;
    }
    net_unlock(&network_lock);
}
void pc_network_doors_targets(const uint32_t *addresses,unsigned count)
{
    net_lock(&network_lock);door_target_count=0;
    if(door_context.role==1u && count<=16u) {
        memcpy(door_targets,addresses,count*sizeof *addresses);door_target_count=count;
    }
    net_unlock(&network_lock);
}
static int door_control(const struct sockaddr_in *address,const unsigned char *data,int length)
{
    uint32_t match,tick,count;unsigned i,j;
    if(length<8 || memcmp(data,"TSFDR01",8))return 0;
    if(length<28 || door_context.role!=2u || address->sin_addr.s_addr!=door_context.host_ip)return 1;
    memcpy(&match,data+16,4);memcpy(&tick,data+20,4);memcpy(&count,data+24,4);
    if(match!=door_context.match || !count || count>PC_NETWORK_DOORS_BATCH ||
       (unsigned)length!=28u+count*sizeof(PcNetworkDoorState))return 1;
    for(i=0;i<peer_count;i++)if(peers[i].ip==door_context.host_ip && !memcmp(peers[i].key,data+8,8))break;
    if(i==peer_count)return 1;
    PcNetworkDoorState states[PC_NETWORK_DOORS_BATCH];
    memcpy(states,data+28,count*sizeof *states);
    /* Reject the whole batch before publishing any malformed/duplicate row. */
    for(i=0;i<count;i++) {
        if(!pc_network_door_valid(&states[i]))return 1;
        for(j=0;j<i;j++)if(states[j].slot==states[i].slot)return 1;
    }
    for(i=0;i<count;i++) {
        const unsigned slot=states[i].slot;
        if(door_seen[slot] && (int32_t)(tick-door_ticks[slot])<=0)continue;
        door_pending[slot]=states[i];door_ticks[slot]=tick;
        door_seen[slot]=door_fresh[slot]=1;
    }
    return 1;
}
void pc_network_doors_send(uint32_t tick,const PcNetworkDoorState *states,unsigned count)
{
    unsigned char data[28+PC_NETWORK_DOORS_BATCH*sizeof(PcNetworkDoorState)];
    if(!count || count>PC_NETWORK_DOORS_MAX)return;
    for(unsigned i=0;i<count;i++)if(!pc_network_door_valid(&states[i]))return;
    net_lock(&network_lock);
    if(!initialized || !local_session_registered || door_context.role!=1u)goto done;
    memcpy(data,"TSFDR01",8);memcpy(data+8,local_session_id,8);
    memcpy(data+16,&door_context.match,4);memcpy(data+20,&tick,4);
    for(unsigned first=0;first<count;first+=PC_NETWORK_DOORS_BATCH) {
        const uint32_t n=count-first<PC_NETWORK_DOORS_BATCH?count-first:PC_NETWORK_DOORS_BATCH;
        memcpy(data+24,&n,4);memcpy(data+28,states+first,n*sizeof *states);
        for(unsigned i=0;i<door_target_count;i++) {
            struct sockaddr_in address={0};
            if(!door_targets[i] || door_targets[i]==local_ip)continue;
            address.sin_family=AF_INET;address.sin_addr.s_addr=door_targets[i];
            for(unsigned j=0;j<MAX_SOCKETS;j++)if(sockets[j].socket!=INVALID_SOCKET && sockets[j].port) {
                address.sin_port=htons((uint16_t)sockets[j].port);
                send_packet(&sockets[j],(const char *)data,(int)(28u+n*sizeof *states),0,&address);
            }
        }
    }
done:
    net_unlock(&network_lock);
}
unsigned pc_network_doors_receive(PcNetworkDoorState *states,unsigned capacity)
{
    unsigned count=0;net_lock(&network_lock);
    if(door_context.role==2u)for(unsigned i=0;i<PC_NETWORK_DOORS_MAX && count<capacity;i++)
        if(door_fresh[i]) {states[count++]=door_pending[i];door_fresh[i]=0;}
    net_unlock(&network_lock);return count;
}
