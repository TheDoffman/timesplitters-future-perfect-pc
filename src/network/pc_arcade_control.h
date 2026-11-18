#ifndef TSFP_PC_ARCADE_CONTROL_H
#define TSFP_PC_ARCADE_CONTROL_H
#include "pc_arcade_checkpoint.h"
#define PC_ARCADE_FRAGMENT 1200u
#define PC_ARCADE_STATE_BYTES 216u

typedef struct PcArcadeNativePeer {uint32_t native_id,ip;} PcArcadeNativePeer;
typedef struct PcArcadeControlState {
    uint32_t epoch,phase,sequence,host_ip,candidate_ip,previous_ip,bytes;
    uint8_t digest[32],room[16];
    uint32_t native_count;
    PcArcadeNativePeer native_peers[16];
} PcArcadeControlState;
typedef struct PcArcadeControl {
    PcArcadeControlState state;
    uint8_t assembly[sizeof(PcArcadeCheckpoint)],digest[32];
    uint64_t received;
    uint32_t sequence,total,crc;
    int enabled,complete;
    PcArcadeCheckpoint checkpoint;
} PcArcadeControl;
static inline uint32_t pc_arcade_load32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline void pc_arcade_store32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
/* Only feed data already authenticated by the local bridge token/address.
 * State epochs and ordered phase transitions fence delayed local datagrams.
 */
static inline int pc_arcade_control_receive(PcArcadeControl *c,const uint8_t *data,size_t bytes)
{
    if(!c || !data)return 0;
    if(bytes==PC_ARCADE_STATE_BYTES && !memcmp(data,"AMST",4)) {
        PcArcadeControlState s;
        if(pc_arcade_load32(data+4)!=1u)return 0;
        s.epoch=pc_arcade_load32(data+8);s.phase=pc_arcade_load32(data+12);
        s.sequence=pc_arcade_load32(data+16);memcpy(&s.host_ip,data+20,4);
        memcpy(&s.candidate_ip,data+24,4);memcpy(&s.previous_ip,data+28,4);
        s.bytes=pc_arcade_load32(data+32);memcpy(s.digest,data+36,32);memcpy(s.room,data+68,16);
        s.native_count=pc_arcade_load32(data+84);
        if(s.native_count>16u)return 0;
        memset(s.native_peers,0,sizeof s.native_peers);
        for(unsigned i=0;i<s.native_count;i++) {
            const uint8_t *p=data+88u+i*8u;
            s.native_peers[i].native_id=pc_arcade_load32(p);memcpy(&s.native_peers[i].ip,p+4,4);
            if(s.native_peers[i].native_id>=16u || p[4]!=10u || p[5]!=254u || p[6] || !p[7] || p[7]==255u)return 0;
            for(unsigned j=0;j<i;j++)if(s.native_peers[j].native_id==s.native_peers[i].native_id ||
                s.native_peers[j].ip==s.native_peers[i].ip)return 0;
        }
        if(!s.epoch || s.phase>5u || (s.bytes && s.bytes!=sizeof(PcArcadeCheckpoint)) ||
           (!!s.sequence != !!s.bytes) || (c->enabled && s.epoch<c->state.epoch))return 0;
        if(c->enabled && s.epoch==c->state.epoch) {
            if(memcmp(s.room,c->state.room,16) ||
               (c->state.phase==1u && s.phase==2u) ||
               ((c->state.phase==3u || c->state.phase==4u) && s.phase!=c->state.phase) ||
               (c->state.phase==1u && s.phase==5u) ||
               (c->state.phase==5u && s.phase==5u && s.sequence<c->state.sequence) ||
               (c->state.phase==1u && s.phase==1u && s.sequence<c->state.sequence))return 0;
        }
        if(c->enabled && memcmp(s.room,c->state.room,16)) {
            c->complete=0;c->received=0;c->sequence=c->total=c->crc=0;
        }
        c->state=s;c->enabled=1;return 1;
    }
    if(bytes>52u && bytes<=52u+PC_ARCADE_FRAGMENT && !memcmp(data,"AMFR",4) && c->enabled) {
        const uint32_t seq=pc_arcade_load32(data+4),total=pc_arcade_load32(data+8);
        const uint32_t offset=pc_arcade_load32(data+12),crc=pc_arcade_load32(data+16);
        if(seq!=c->state.sequence || total!=sizeof(PcArcadeCheckpoint) || total!=c->state.bytes ||
           offset>=total || offset%PC_ARCADE_FRAGMENT || memcmp(data+20,c->state.digest,32))return 0;
        const size_t count=total-offset<PC_ARCADE_FRAGMENT?total-offset:PC_ARCADE_FRAGMENT;
        if(bytes!=52u+count)return 0;
        if(c->sequence!=seq || c->total!=total || c->crc!=crc || memcmp(c->digest,data+20,32)) {
            c->received=0;c->sequence=seq;c->total=total;c->crc=crc;memcpy(c->digest,data+20,32);
        }
        const unsigned part=offset/PC_ARCADE_FRAGMENT;
        if(part>=64u)return 0;
        const uint64_t bit=(uint64_t)1u<<part;
        if((c->received&bit) && memcmp(c->assembly+offset,data+52,count)) {
            c->received=0;return 0;
        }
        memcpy(c->assembly+offset,data+52,count);c->received|=bit;
        const unsigned parts=(total+PC_ARCADE_FRAGMENT-1u)/PC_ARCADE_FRAGMENT;
        if(c->received==(((uint64_t)1u<<parts)-1u)) {
            if(pc_arcade_crc32(c->assembly,total)!=crc)return 0;
            PcArcadeCheckpoint checkpoint;
            memcpy(&checkpoint,c->assembly,sizeof checkpoint);
            if(checkpoint.sequence!=seq || checkpoint.epoch>c->state.epoch ||
               !pc_arcade_checkpoint_valid(&checkpoint,c->state.room))return 0;
            c->checkpoint=checkpoint;c->complete=1;
        }
        return 1;
    }
    return 0;
}
/* Stock clients allocate records for other guests but leave their destinations
 * zero: traffic previously went through the host. Populate only authenticated
 * native-ID routes, preserving the channel queues and every local controller. */
static inline int pc_arcade_prepare_peer_routes(void *ctx,PcArcadeRead read,PcArcadeWrite write,
    uint32_t local_ip,const PcArcadeControlState *state)
{
    uint32_t node,local_id,old_host;uint8_t base[8],plan[16][8];
    int self_seen=0,host_seen=0;
    if(!state || !state->native_count || state->native_count>16u ||
       !read(ctx,0x774028u,&node,4) || !node || !read(ctx,node,&local_id,4) || local_id>=16u ||
       !read(ctx,0x774040u,&old_host,4) || old_host>=16u ||
       !read(ctx,0x666488u+old_host*16u,base,sizeof base) || base[0]!=2u || base[1] || (!base[2] && !base[3]))return 0;
    for(unsigned i=0;i<state->native_count;i++) {
        const PcArcadeNativePeer *peer=&state->native_peers[i];uint32_t id,flags,connected,ip;
        if(peer->native_id>=16u)return 0;
        if(peer->ip==state->candidate_ip)host_seen=1;
        if(peer->ip==local_ip) {if(peer->native_id!=local_id)return 0;self_seen=1;continue;}
        if(peer->native_id==local_id ||
           !read(ctx,0x774180u+peer->native_id*0x30cu,&id,4) || id!=peer->native_id ||
           !read(ctx,0x7741b4u+peer->native_id*0x30cu,&flags,4) || !(flags&1u) ||
           !read(ctx,0x666040u+peer->native_id*4u,&connected,4) || !connected ||
           !read(ctx,0x66648cu+peer->native_id*16u,&ip,4) || (ip && ip!=peer->ip))return 0;
        memcpy(plan[i],base,4);memcpy(plan[i]+4,&peer->ip,4);
        if(!write(ctx,0x666488u+peer->native_id*16u,plan[i],8,0))return 0;
    }
    if(!self_seen || !host_seen)return 0;
    for(unsigned i=0;i<state->native_count;i++)if(state->native_peers[i].ip!=local_ip &&
       !write(ctx,0x666488u+state->native_peers[i].native_id*16u,plan[i],8,1))return 0;
    return 1;
}
/* Capture/restore preflight uses this helper for the original native peer table.
 * Companion virtual-IP IDs never become original player/controller indices.
 */
static inline int pc_arcade_adopt_authority(void *ctx,PcArcadeRead read,PcArcadeWrite write,
    uint32_t virtual_local_ip,uint32_t new_host_ip,unsigned *role_out,int apply)
{
    uint32_t local_node,local_id,phase,session,host_id=16u,role;
    if(!read || !write || !new_host_ip || !virtual_local_ip ||
       !read(ctx,0x76b108u,&session,4) || !session ||
       !read(ctx,0x76b1a4u,&phase,4) || phase!=15u ||
       !read(ctx,0x774028u,&local_node,4) || !local_node ||
       !read(ctx,local_node,&local_id,4) || local_id>=16u)return 0;
    if(new_host_ip==virtual_local_ip)host_id=local_id;
    else for(unsigned i=0;i<16u;i++) {
        uint32_t connected,ip;
        if(!read(ctx,0x666040u+i*4u,&connected,4) ||
           !read(ctx,0x66648cu+i*16u,&ip,4))return 0;
        if(connected && ip==new_host_ip){host_id=i;break;}
    }
    if(host_id>=16u)return 0;
    role=host_id==local_id?1u:2u;
    if(!write(ctx,0x76b140u,&role,4,0) || !write(ctx,0x774040u,&host_id,4,0))return 0;
    if(apply && (!write(ctx,0x774040u,&host_id,4,1) || !write(ctx,0x76b140u,&role,4,1)))return 0;
    if(role_out)*role_out=role;
    return 1;
}
#endif
