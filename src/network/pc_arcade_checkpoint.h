#ifndef TSFP_PC_ARCADE_CHECKPOINT_H
#define TSFP_PC_ARCADE_CHECKPOINT_H
/* Portable Arcade continuation state. Never serialize actor/controller pointers.
 * Existing virtual player IDs and each machine's local actor allocations survive.
 * The current targets are little-endian IEEE binary32; schema1 records that ABI.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "pc_network_bots_state.h"

#define PC_ARCADE_CHECKPOINT_MAGIC 0x314d4341u
#define PC_ARCADE_CHECKPOINT_SCHEMA 1u
#define PC_ARCADE_CHECKPOINT_CONFIG_BYTES 0xa0u
#define PC_ARCADE_CHECKPOINT_ACTORS 16u
#define PC_ARCADE_CHECKPOINT_BOTS 10u

typedef struct PcArcadeCheckpoint {
    uint32_t magic,schema,bytes,sequence;
    uint8_t room[16];
    uint32_t epoch,level,tick,phase;
    uint8_t config[PC_ARCADE_CHECKPOINT_CONFIG_BYTES];
    int16_t scores[PC_ARCADE_CHECKPOINT_ACTORS][42];
    int32_t totals[PC_ARCADE_CHECKPOINT_ACTORS];
    uint32_t score_ticks[PC_ARCADE_CHECKPOINT_ACTORS];
    uint32_t bot_mask;
    PcNetworkBotState bots[PC_ARCADE_CHECKPOINT_BOTS];
} PcArcadeCheckpoint;
typedef char PcArcadeCheckpointBound[(sizeof(PcArcadeCheckpoint)<=65536u)?1:-1];

typedef int (*PcArcadeRead)(void *context,uint32_t address,void *out,size_t bytes);
typedef int (*PcArcadeWrite)(void *context,uint32_t address,const void *value,size_t bytes,int apply);
typedef int (*PcArcadeBotRead)(void *context,unsigned slot,PcNetworkBotState *out);
typedef int (*PcArcadeBotApply)(void *context,const PcNetworkBotState *state,int apply);

/* The US config block mixes shared rules with local player count, unused
 * Deathmatch human-team placeholders, the mode-state reset latch and a LIVE countdown at +60. Those may
 * differ on healthy peers. Validate shared rules; preserve local fields. */
static inline int pc_arcade_config_matches(const uint8_t *a,const uint8_t *b)
{
    if(!a || !b || !a[0] || a[0]>16u || a[0]!=b[0] ||
       !b[1] || b[1]>4u || b[1]>b[0])return 0;
    for(unsigned i=0;i<PC_ARCADE_CHECKPOINT_CONFIG_BYTES;i++) {
        if(i==1u || (i>=0x60u && i<0x64u))continue;
        /* Original22CEB0 sets flags+5C bit14 while resetting mode runtime
           state. A late loader has not necessarily run that transition yet.
           Preserve its local latch; every other flag bit must still match. */
        if(i==0x5du) {if((a[i]^b[i])&~0x40u)return 0;continue;}
        if(!(a[0x58u]&1u) && i>=0x24u && i<0x24u+a[0])continue;
        if(a[i]!=b[i])return 0;
    }
    return 1;
}

static inline uint32_t pc_arcade_crc32(const void *payload,size_t bytes)
{
    const uint8_t *p=(const uint8_t *)payload;uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<bytes;i++) {
        crc^=p[i];
        for(unsigned bit=0;bit<8u;bit++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static inline int pc_arcade_checkpoint_valid(const PcArcadeCheckpoint *s,const uint8_t room[16])
{
    if(!s || !room || s->magic!=PC_ARCADE_CHECKPOINT_MAGIC ||
       s->schema!=PC_ARCADE_CHECKPOINT_SCHEMA || s->bytes!=sizeof *s ||
       !s->sequence || !s->epoch || memcmp(s->room,room,16) || s->level>100u || s->phase!=7u)
        return 0;
    const unsigned humans=s->config[0],bots=s->config[2];
    if(!humans || humans>16u || bots>10u || humans+bots>16u ||
       !s->config[1] || s->config[1]>4u || s->config[1]>humans ||
       s->bot_mask!=((1u<<bots)-1u))return 0;
    for(unsigned i=0;i<bots;i++) {
        if(s->bots[i].slot!=humans+i || s->bots[i].tick!=s->tick ||
           !pc_network_bot_state_valid(&s->bots[i],humans,bots) ||
           memcmp(s->bots[i].scores,s->scores[humans+i],sizeof s->scores[0]))return 0;
        for(unsigned actor=0;actor<humans+bots;actor++)
            if(s->bots[i].killed_by[actor]!=s->scores[actor][humans+i])return 0;
    }
    return 1;
}
static inline int pc_arcade_checkpoint_capture(PcArcadeCheckpoint *out,void *ctx,
    PcArcadeRead read,PcArcadeBotRead bot_read,const uint8_t room[16],uint32_t epoch,uint32_t sequence)
{
    PcArcadeCheckpoint staged;uint32_t pool,session,role,network_phase;
    if(!out || !read || !bot_read || !room || !epoch || !sequence)return 0;
    memset(&staged,0,sizeof staged);
    if(!read(ctx,0x76b108u,&session,4) || !session ||
       !read(ctx,0x76b140u,&role,4) || role!=1u ||
       !read(ctx,0x76b1a4u,&network_phase,4) || network_phase!=15u ||
       !read(ctx,0x79094cu,&staged.level,4) || staged.level>100u ||
       !read(ctx,0x7de30cu,&staged.phase,4) || staged.phase!=7u ||
       !read(ctx,0x7de318u,&staged.tick,4) ||
       !read(ctx,0x7de400u,staged.config,sizeof staged.config) ||
       !read(ctx,0x7356d8u,&pool,4) || !pool || pool>UINT32_MAX-16u*0xea0u)return 0;
    staged.magic=PC_ARCADE_CHECKPOINT_MAGIC;staged.schema=PC_ARCADE_CHECKPOINT_SCHEMA;
    staged.bytes=sizeof staged;staged.sequence=sequence;staged.epoch=epoch;
    memcpy(staged.room,room,16);
    const unsigned humans=staged.config[0],bots=staged.config[2];
    if(!humans || humans>16u || bots>10u || humans+bots>16u)return 0;
    for(unsigned i=0;i<humans+bots;i++) {
        const uint32_t actor=pool+i*0xea0u;
        if(!read(ctx,actor+0xa10u,staged.scores[i],sizeof staged.scores[i]) ||
           !read(ctx,actor+0xa6cu,&staged.totals[i],4) ||
           !read(ctx,actor+0xa64u,&staged.score_ticks[i],4))return 0;
    }
    for(unsigned i=0;i<bots;i++) {
        if(!bot_read(ctx,humans+i,&staged.bots[i]) || staged.bots[i].tick!=staged.tick)return 0;
        const uint32_t actor=pool+(humans+i)*0xea0u;
        if(!read(ctx,actor+0x10u,staged.bots[i].life,sizeof staged.bots[i].life) ||
           !read(ctx,actor+0x24u,staged.bots[i].flags,sizeof staged.bots[i].flags))return 0;
        staged.bot_mask|=1u<<i;
    }
    if(!pc_arcade_checkpoint_valid(&staged,room))return 0;
    *out=staged;return 1;
}
/* Preflight the entire destination before changing any state. The guest-thread
 * adapter must guarantee that memory/actor ownership cannot change until return.
 * bot_apply(...,0) validates capacity/identity and performs no allocations/writes.
 */
static inline int pc_arcade_checkpoint_restore(const PcArcadeCheckpoint *s,void *ctx,
    PcArcadeRead read,PcArcadeWrite write,PcArcadeBotApply bot_apply,const uint8_t room[16])
{
    uint32_t pool,level,phase,session;uint8_t config[PC_ARCADE_CHECKPOINT_CONFIG_BYTES];
    if(!read || !write || !bot_apply || !pc_arcade_checkpoint_valid(s,room))return 0;
    if(!read(ctx,0x76b108u,&session,4) || !session ||
       !read(ctx,0x79094cu,&level,4) || level!=s->level ||
       !read(ctx,0x76b1a4u,&phase,4) || phase!=15u ||
       !read(ctx,0x7de400u,config,sizeof config) || !pc_arcade_config_matches(s->config,config) ||
       !read(ctx,0x7356d8u,&pool,4) || !pool || pool>UINT32_MAX-16u*0xea0u)return 0;
    const unsigned actors=s->config[0]+s->config[2],bots=s->config[2];
    for(unsigned i=0;i<actors;i++) {
        uint8_t scratch[84];const uint32_t actor=pool+i*0xea0u;
        if(!read(ctx,actor+0xa10u,scratch,sizeof scratch) ||
           !read(ctx,actor+0xa64u,scratch,4) || !read(ctx,actor+0xa6cu,scratch,4) ||
           !write(ctx,actor+0xa10u,s->scores[i],sizeof s->scores[i],0) ||
           !write(ctx,actor+0xa6cu,&s->totals[i],4,0) ||
           !write(ctx,actor+0xa64u,&s->score_ticks[i],4,0))return 0;
    }
    if(!write(ctx,0x7de318u,&s->tick,4,0) || !write(ctx,0x7de460u,s->config+0x60u,4,0))return 0;
    for(unsigned i=0;i<bots;i++)if(!bot_apply(ctx,&s->bots[i],0))return 0;
    for(unsigned i=0;i<actors;i++) {
        const uint32_t actor=pool+i*0xea0u;
        if(!write(ctx,actor+0xa10u,s->scores[i],sizeof s->scores[i],1) ||
           !write(ctx,actor+0xa6cu,&s->totals[i],4,1) ||
           !write(ctx,actor+0xa64u,&s->score_ticks[i],4,1))return 0;
    }
    for(unsigned i=0;i<bots;i++)if(!bot_apply(ctx,&s->bots[i],1))return 0;
    /* Restore both elapsed level time and the separate remaining match timer.
     * Do not copy ISR, rendering, wall-clock or network retransmission counters.
     */
    return write(ctx,0x7de460u,s->config+0x60u,4,1) && write(ctx,0x7de318u,&s->tick,4,1);
}
#endif
