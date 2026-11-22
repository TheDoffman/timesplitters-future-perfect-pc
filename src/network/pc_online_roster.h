#ifndef TSFP_PC_ONLINE_ROSTER_H
#define TSFP_PC_ONLINE_ROSTER_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define PC_ONLINE_ROSTER_MAGIC 0x31524f54u
typedef struct PcOnlinePerson {
    uint32_t slot;
    int32_t character,skin,portrait;
    char name[32],character_name[48];
} PcOnlinePerson;
typedef struct PcOnlineRoster {
    uint32_t magic,version,scene,tick,generation,count,serial;
    PcOnlinePerson people[16];
} PcOnlineRoster;
_Static_assert(sizeof(PcOnlineRoster)==1564,"Local roster display ABI");
typedef int (*PcRosterRead)(void *,uint32_t,void *,size_t);
typedef void (*PcRosterPortrait)(void *,int,int,int32_t *,char *,size_t);
static inline void pc_roster_text(void *ctx,PcRosterRead read,uint32_t address,char *out,size_t size)
{
    size_t n=0;
    if(!address || !size)return;
    for(size_t i=0;i<size-1 && address<=UINT32_MAX-(uint32_t)i;i++) {
        unsigned char c;
        if(!read(ctx,address+(uint32_t)i,&c,1) || !c)break;
        if(c>=32 && c!=127)out[n++]=(char)c;
    }
    out[n]=0;
}
/* Read-only metadata; no addresses, profile files or credentials leave the game. */
static inline int pc_online_roster_capture(PcOnlineRoster *out,void *ctx,PcRosterRead read,PcRosterPortrait portrait)
{
    PcOnlineRoster next={0};uint32_t session,role,phase,pool,play;
    uint8_t config[3],generation;
    if(!out || !read || !portrait ||
       !read(ctx,0x76b108u,&session,4) || !session ||
       !read(ctx,0x76b140u,&role,4) || role!=1 ||
       !read(ctx,0x76b1a4u,&phase,4) || phase!=15 ||
       !read(ctx,0x7de30cu,&play,4) || play!=7 ||
       !read(ctx,0x79094cu,&next.scene,4) || next.scene>100 ||
       !read(ctx,0x7de318u,&next.tick,4) ||
       !read(ctx,0x7de499u,&generation,1) ||
       !read(ctx,0x7de400u,config,sizeof config) ||
       !config[0] || config[0]>16 || !config[1] || config[1]>4 || config[1]>config[0] ||
       config[2]>10 || config[0]+config[2]>16 ||
       !read(ctx,0x7356d8u,&pool,4) || !pool || pool>UINT32_MAX-16u*0xea0u)return 0;
    next.magic=PC_ONLINE_ROSTER_MAGIC;next.version=1;next.generation=generation;
    for(unsigned slot=0;slot<(unsigned)config[0]+config[2];slot++) {
        if(slot<config[0]) {
            uint32_t owner,flags;
            if(!read(ctx,0x777244u+slot*0x124u,&owner,4) ||
               !read(ctx,0x77728cu+slot*0x124u,&flags,4))return 0;
            if(!owner || !(flags&1))continue;
        }
        PcOnlinePerson *p=&next.people[next.count++];uint32_t name;
        const uint32_t actor=pool+slot*0xea0u;int16_t character;int8_t skin;
        if(!read(ctx,actor+0x70u,&name,4) || !read(ctx,actor+0x48u,&character,2) ||
           !read(ctx,actor+0xc0du,&skin,1))return 0;
        p->slot=slot;p->character=character;p->skin=skin;p->portrait=-1;
        if(character< -1 || character>=4096)p->character=-1;
        pc_roster_text(ctx,read,name,p->name,sizeof p->name);
        if(character>=0 && character<4096)
            portrait(ctx,character,skin,&p->portrait,p->character_name,sizeof p->character_name);
        p->name[31]=p->character_name[47]=0;
    }
    if(!next.count)return 0;
    *out=next;return 1;
}
#endif
