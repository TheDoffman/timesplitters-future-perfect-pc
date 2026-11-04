#ifndef TSFP_PC_LOCAL_JOIN_H
#define TSFP_PC_LOCAL_JOIN_H
/* Session-only seats. Disabled=-1, keyboard=-2, physical pad=0..3. */
typedef struct PcLocalJoin { unsigned held; int initialized, primary_claimed; } PcLocalJoin;
static inline int pc_local_join(PcLocalJoin *s,int devices[4],unsigned starts,int keyboard)
{
    int changed=0;
    if(!s->initialized) {s->initialized=1;s->held=starts;s->primary_claimed=devices[0]>=0;}
    if(keyboard)s->primary_claimed=1;
    unsigned fresh=starts&~s->held;s->held=starts;
    for(int pad=0;pad<4;pad++)if(fresh&(1u<<pad)) {
        int owner=-1,seat=-1;
        for(int i=0;i<4;i++)if(devices[i]==pad)owner=i;
        if(owner>=0)continue;
        if(!s->primary_claimed && devices[0]==-2)seat=0;
        else for(int i=1;i<4;i++)if(devices[i]==-1){seat=i;break;}
        if(seat<0)continue;
        devices[seat]=pad;changed=1;if(!seat)s->primary_claimed=1;
    }
    return changed;
}
#endif
