#ifndef TSFP_PC_KILL_FEED_H
#define TSFP_PC_KILL_FEED_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define PC_FEED_PLAYERS 16
#define PC_FEED_ROWS 5
#define PC_FEED_NAME 32
/* Read-only score snapshots. Online retransmissions never create new events. */
typedef struct PcFeedSnapshot {
    uint32_t match, pool;
    unsigned count;
    uint16_t kills[16][16];
    char names[16][PC_FEED_NAME];
    unsigned teams[16];
    uint16_t weapons[16][16]; /* sprite IDs; zero means unavailable */
} PcFeedSnapshot;
typedef struct PcFeedRow {
    uint64_t at; char text[96];
    char killer[PC_FEED_NAME],victim[PC_FEED_NAME];
    unsigned killer_id,victim_id,killer_team,victim_team,weapon;
} PcFeedRow;
/* Relative to this viewport, never a hard-coded team number. */
static inline uint32_t pc_feed_color(unsigned id,unsigned team,unsigned viewer,unsigned viewer_team,int teams) {
    if(viewer>=PC_FEED_PLAYERS)return 0x707f7f00u;
    return id==viewer || (teams && team && team==viewer_team)?0x305b7f00u:0x7f303000u;
}
typedef struct PcKillFeed {
    PcFeedSnapshot previous;
    PcFeedRow rows[PC_FEED_ROWS];
    unsigned count;
    int ready;
} PcKillFeed;
static inline void pc_feed_clear(PcKillFeed *f) { memset(f,0,sizeof(*f)); }
static inline void pc_feed_name(char out[PC_FEED_NAME],const char *in) {
    unsigned n=0;
    /* Strip guest font escapes and control characters from remote names. */
    while(in && *in && n<PC_FEED_NAME-1) {
        unsigned char c=(unsigned char)*in++;
        if(c<32 || c==127)continue;
        out[n++]=(char)c;
    }
    out[n]=0;
    if(!n)memcpy(out,"Player",7);
}
static inline void pc_feed_add(PcKillFeed *f,uint64_t now,const char *killer,const char *victim,int self) {
    if(f->count==PC_FEED_ROWS) {
        memmove(f->rows,f->rows+1,sizeof(f->rows[0])*(PC_FEED_ROWS-1));--f->count;
    }
    PcFeedRow *r=&f->rows[f->count++];memset(r,0,sizeof *r);r->at=now;
    pc_feed_name(r->killer,killer);pc_feed_name(r->victim,victim);
    if(self)snprintf(r->text,sizeof(r->text),"%s [self]",victim);
    else snprintf(r->text,sizeof(r->text),"%s > %s",killer,victim);
}
static inline void pc_feed_expire(PcKillFeed *f,uint64_t now) {
    unsigned n=0;
    for(unsigned i=0;i<f->count;i++)
        if(now>=f->rows[i].at && now-f->rows[i].at<5000)f->rows[n++]=f->rows[i];
    f->count=n;
}
static inline unsigned pc_feed_alpha(const PcFeedRow *r,uint64_t now) {
    if(now<r->at || now-r->at>=5000)return 0;
    uint64_t age=now-r->at;
    return age<=4000?127u:(unsigned)((5000-age)*127/1000);
}
static inline void pc_feed_sample(PcKillFeed *f,const PcFeedSnapshot *s,uint64_t now,int enabled) {
    if(!enabled || !s || !s->count || s->count>16) {pc_feed_clear(f);return;}
    pc_feed_expire(f,now);
    if(!f->ready || f->previous.pool!=s->pool || f->previous.match!=s->match ||
       f->previous.count!=s->count || memcmp(f->previous.names,s->names,sizeof s->names)) {
        pc_feed_clear(f);f->previous=*s;f->ready=1;return;
    }
    for(unsigned k=0;k<s->count;k++)for(unsigned v=0;v<s->count;v++) {
        unsigned delta=(uint16_t)(s->kills[k][v]-f->previous.kills[k][v]);
        /* Ignore old/reordered snapshots. Match boundaries rebaseline above.
           Bound catch-up work: only the newest five rows can be displayed. */
        if(delta && delta<32768u) {
            unsigned visible=delta<PC_FEED_ROWS?delta:PC_FEED_ROWS;
            for(unsigned i=0;i<visible;i++) {
                pc_feed_add(f,now,s->names[k],s->names[v],k==v);
                PcFeedRow *r=&f->rows[f->count-1];
                r->killer_id=k;r->victim_id=v;r->killer_team=s->teams[k];r->victim_team=s->teams[v];
                /* Only the latest counter has a corresponding weapon record. */
                r->weapon=i+1==visible?s->weapons[k][v]:0;
            }
            f->previous.kills[k][v]=s->kills[k][v];
        }
    }
}
#endif
