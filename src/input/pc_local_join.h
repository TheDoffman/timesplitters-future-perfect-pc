#ifndef TSFP_PC_LOCAL_JOIN_H
#define TSFP_PC_LOCAL_JOIN_H
/* Physical Start edges, independent of configured logical input ports.
 * Guest profile selection owns player order; this only allocates an unused port.
 * Disabled=-1, keyboard=-2, physical controller=0..3. */
typedef struct PcLocalJoin { unsigned held, pending; int initialized; } PcLocalJoin;
static inline int pc_local_join(PcLocalJoin *s,int devices[4],unsigned starts,unsigned used)
{
    if(!s->initialized) { s->initialized=1; s->held=starts; }
    s->pending|=starts&~s->held; s->held=starts;
    for(int pad=0;pad<4;pad++)if(s->pending&(1u<<pad)) {
        int owner=-1,port=-1;
        s->pending&=~(1u<<pad);
        for(int i=0;i<4;i++)if(devices[i]==pad)owner=i;
        if(owner>=0) {
            if(used&(1u<<owner))continue;
            return owner;
        }
        for(int i=0;i<4;i++)if(!(used&(1u<<i)) && devices[i]==-1){port=i;break;}
        if(port<0)for(int i=0;i<4;i++)if(!(used&(1u<<i)) && devices[i]!=-2){port=i;break;}
        if(port>=0) {devices[port]=pad;return port;}
    }
    return -1;
}
#endif
