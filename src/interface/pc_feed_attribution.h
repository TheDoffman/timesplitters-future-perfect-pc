#ifndef TSFP_PC_FEED_ATTRIBUTION_H
#define TSFP_PC_FEED_ATTRIBUTION_H
#include <stdint.h>
#include <string.h>
/* Hash collisions lose an icon, never substitute a different projectile. */
typedef struct PcFeedShot { uint32_t pointer,owner,kind;unsigned icon; } PcFeedShot;
typedef struct PcFeedHit { uint32_t pointer,owner,kind,tick;unsigned icon; } PcFeedHit;
typedef struct PcFeedAttribution {
    uint32_t pool,match;
    PcFeedShot shots[1024];PcFeedHit hits[64];
    uint16_t counters[16][16],icons[16][16];
} PcFeedAttribution;
static inline void pc_feed_attribution_scene(PcFeedAttribution *a,uint32_t pool,uint32_t match) {
    if(a->pool!=pool || a->match!=match) {memset(a,0,sizeof *a);a->pool=pool;a->match=match;}
}
static inline void pc_feed_shot(PcFeedAttribution *a,uint32_t p,uint32_t owner,uint32_t kind,unsigned icon) {
    a->shots[(p>>4)%1024]=(PcFeedShot){p,owner,kind,icon};
}
static inline void pc_feed_hit(PcFeedAttribution *a,uint32_t hit,uint32_t shot,uint32_t owner,uint32_t kind,uint32_t tick) {
    PcFeedShot *s=&a->shots[(shot>>4)%1024];
    unsigned icon=s->pointer==shot && s->owner==owner && s->kind==kind?s->icon:0;
    a->hits[(hit>>2)%64]=(PcFeedHit){hit,owner,kind,tick,icon};
}
static inline unsigned pc_feed_hit_icon(const PcFeedAttribution *a,uint32_t hit,uint32_t owner,uint32_t kind,uint32_t tick) {
    const PcFeedHit *h=&a->hits[(hit>>2)%64];
    return h->pointer==hit && h->owner==owner && h->kind==kind && h->tick==tick?h->icon:0;
}
#endif
