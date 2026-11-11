#ifndef TSFP_PC_ARCADE_JOIN_H
#define TSFP_PC_ARCADE_JOIN_H
#include "pc_arcade_checkpoint.h"

/* Human IDs are a prefix followed by bots. Consume the first bot slots without
 * moving any remaining actor, scoreboard row or kill-matrix column. */
static inline int pc_arcade_join_population(unsigned humans,unsigned bots,
    uint32_t controllers,unsigned limit,unsigned *new_humans,unsigned *new_bots)
{
    if(!new_humans || !new_bots || !humans || humans>16u || bots>10u ||
       humans+bots>16u || !limit || limit>16u || (controllers&0xffff0000u))return 0;
    unsigned high=humans;
    for(unsigned i=0;i<16u;i++)if(controllers&(1u<<i))high=i+1u>high?i+1u:high;
    if(high>limit)return 0;
    unsigned used=high-humans;
    *new_humans=high;*new_bots=bots>(used)?bots-used:0u;
    return high+*new_bots<=limit;
}

static inline int pc_arcade_join_sync(void *ctx,PcArcadeRead read,PcArcadeWrite write)
{
    uint32_t session,phase,game,mode,map,limit,mask=0;
    uint8_t config[PC_ARCADE_CHECKPOINT_CONFIG_BYTES];
    if(!read || !write || !read(ctx,0x76b108u,&session,4) || !session ||
       !read(ctx,0x76b1a4u,&phase,4) || phase!=15u ||
       !read(ctx,0x7de30cu,&game,4) || game!=7u ||
       !read(ctx,0x78ad2cu,&mode,4) || mode<11u || mode>13u ||
       !read(ctx,0x78ad4cu,&map,4) || map>=17u ||
       !read(ctx,0x4d239cu+(map*15u+(mode==11u?2u:mode==13u?3u:0u))*4u,&limit,4) ||
       !read(ctx,0x7de400u,config,sizeof config))return 0;
    if(limit>16u)limit=16u;
    for(unsigned slot=0;slot<16u;slot++) {
        uint32_t owner,flags;
        if(!read(ctx,0x777244u+slot*0x124u,&owner,4) ||
           !read(ctx,0x77728cu+slot*0x124u,&flags,4))return 0;
        if(owner && (flags&1u))mask|=1u<<slot;
    }
    unsigned humans,bots;
    if(!pc_arcade_join_population(config[0],config[2],mask,limit,&humans,&bots) ||
       (humans==config[0] && bots==config[2]))return 0;
    const uint32_t addresses[]={0x7de400u,0x7de340u,0x774044u,0x78ad58u};
    uint8_t headers[4][3];
    for(unsigned i=0;i<4u;i++) {
        uint8_t generation;
        if(!read(ctx,addresses[i]+0x99u,&generation,1) || generation!=config[0x99] ||
           !read(ctx,addresses[i],headers[i],3))return 0;
        headers[i][0]=(uint8_t)humans;headers[i][2]=(uint8_t)bots;
        if(!write(ctx,addresses[i],headers[i],3,0))return 0;
    }
    if(!write(ctx,0x790950u,&humans,4,0) || !write(ctx,0x7a2958u,&bots,4,0))return 0;
    for(unsigned i=0;i<4u;i++)if(!write(ctx,addresses[i],headers[i],3,1))return 0;
    return write(ctx,0x790950u,&humans,4,1) && write(ctx,0x7a2958u,&bots,4,1);
}

static inline int pc_arcade_join_slots(void *ctx,PcArcadeRead read,uint32_t out[4])
{
    uint32_t mode,map,limit,node,record,owner,humans=0;
    uint8_t config[3];
    if(!read || !out || !read(ctx,0x78ad2cu,&mode,4) || mode<11u || mode>13u ||
       !read(ctx,0x78ad4cu,&map,4) || map>=17u ||
       !read(ctx,0x4d239cu+(map*15u+(mode==11u?2u:mode==13u?3u:0u))*4u,&limit,4) ||
       !read(ctx,0x7de400u,config,sizeof config) || !config[0] || config[0]>16u ||
       config[2]>10u || config[0]+config[2]>16u ||
       !read(ctx,0x774028u,&node,4) || !node || node>UINT32_MAX-0x10u ||
       !read(ctx,node+0xcu,&record,4) || record<0x777240u || record>=0x778480u ||
       (record-0x777240u)%0x124u || !read(ctx,record+4u,&owner,4) || owner!=node)return 0;
    if(limit>16u)limit=16u;
    if(!limit || config[0]+config[2]>limit)return 0;
    for(unsigned slot=0;slot<16u;slot++) {
        uint32_t flags;
        if(!read(ctx,0x777244u+slot*0x124u,&owner,4) ||
           !read(ctx,0x77728cu+slot*0x124u,&flags,4))return 0;
        if(owner && (flags&1u))humans|=1u<<slot;
    }
    const uint32_t bots=((1u<<config[2])-1u)<<config[0],slot=(record-0x777240u)/0x124u;
    if(slot>=limit || !(humans&(1u<<slot)) || (humans&bots) || ((humans|bots)>>limit))return 0;
    out[0]=limit;out[1]=humans;out[2]=bots;out[3]=slot;return 1;
}

static inline int pc_arcade_join_fresh_score(void *ctx,PcArcadeRead read,PcArcadeWrite write,unsigned slot)
{
    uint32_t pool,tick;const int16_t zero[42]={0};const uint32_t total=0;
    if(!read || !write || slot>=16u || !read(ctx,0x7356d8u,&pool,4) || !pool ||
       pool>UINT32_MAX-16u*0xea0u || !read(ctx,0x7de318u,&tick,4))return 0;
    const uint32_t actor=pool+slot*0xea0u;
    if(!write(ctx,actor+0xa10u,zero,sizeof zero,0) ||
       !write(ctx,actor+0xa6cu,&total,4,0) || !write(ctx,actor+0xa64u,&tick,4,0))return 0;
    return write(ctx,actor+0xa10u,zero,sizeof zero,1) &&
        write(ctx,actor+0xa6cu,&total,4,1) && write(ctx,actor+0xa64u,&tick,4,1);
}

/* Only the catching-up player's LOCAL team is adopted from the validated
 * authoritative checkpoint. Never rewrite another player's team selection. */
static inline int pc_arcade_join_team(void *ctx,PcArcadeRead read,PcArcadeWrite write,const PcArcadeCheckpoint *s)
{
    uint32_t slots[4],pool;
    if(!s || !read || !write)return 0;
    if(!(s->config[0x58u]&1u))return 1;
    if(!pc_arcade_join_slots(ctx,read,slots) || slots[3]>=s->config[0] ||
       !read(ctx,0x7356d8u,&pool,4) || !pool || pool>UINT32_MAX-16u*0xea0u)return 0;
    const unsigned slot=slots[3];const uint32_t team=s->config[0x24u+slot];
    const uint32_t controller=0x777288u+slot*0x124u,actor=pool+slot*0xea0u+0x84u;
    const uint8_t value=(uint8_t)team;
    if(team>4u || !write(ctx,controller,&team,4,0) || !write(ctx,actor,&team,4,0) ||
       !write(ctx,0x7de424u+slot,&value,1,0))return 0;
    return write(ctx,controller,&team,4,1) && write(ctx,actor,&team,4,1) &&
        write(ctx,0x7de424u+slot,&value,1,1);
}
#endif
