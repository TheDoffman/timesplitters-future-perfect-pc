#ifndef TSFP_PC_LOCAL_JOIN_H
#define TSFP_PC_LOCAL_JOIN_H
/* Physical Start edges, independent of configured logical input ports.
 * Guest profile selection owns player order; this only allocates an unused port.
 * Disabled=-1, keyboard=-2, physical controller=0..3. */
/* Make connected devices visible to the native player-count chooser before
 * anybody presses Start. Preserve active and disconnected assigned ports. */
static inline void pc_local_detect(int devices[4],const int preferred[4],unsigned connected)
{
    unsigned owned=0;
    for(int i=0;i<4;i++)if(devices[i]>=0)owned|=1u<<devices[i];
    for(int pass=0;pass<2;pass++)for(int i=1;i<4;i++)if(devices[i]==-1) {
        int pad=preferred[i];
        if(pass) {for(pad=0;pad<4;pad++)if((connected&~owned)&(1u<<pad))break;}
        if(pad>=0 && pad<4 && ((connected&~owned)&(1u<<pad))) {
            devices[i]=pad;owned|=1u<<pad;
        }
    }
}
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
