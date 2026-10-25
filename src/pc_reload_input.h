#ifndef TSFP_PC_RELOAD_INPUT_H
#define TSFP_PC_RELOAD_INPUT_H
#include <stdint.h>
#include <string.h>

/* Keyboard-only action arbitration, independent of rendering rate. Host
 * adapters serialize events/context/sampling with their existing input lock. */
typedef struct PcReloadInput {
    uint64_t pressed_at[256], context_at, pulse_until, request_until;
    unsigned char events[256]; /* press=1, release=2 */
    uint32_t player;
    int eligible, reloading, waiting, seen_reload, blocked_fire;
} PcReloadInput;

static inline void pc_reload_reset(PcReloadInput *r) { memset(r,0,sizeof *r); }

static inline void pc_reload_event(PcReloadInput *r,unsigned key,int was_down,int down,uint64_t now)
{
    if(key==0 || key>=256 || !!was_down==!!down)return;
    r->events[key]|=down?1:2;
    if(down)r->pressed_at[key]=now;
}

static inline void pc_reload_context(PcReloadInput *r,uint32_t player,int eligible,int reloading,uint64_t now)
{
    if(player!=r->player || !eligible)pc_reload_reset(r);
    r->player=player;r->eligible=eligible;r->reloading=reloading;r->context_at=now;
}

/* roles[key]: Reload=1, primary Fire=2, secondary Fire=4. Magnitudes are
 * KEYBOARD contributions only; physical controllers are merged afterwards. */
static inline void pc_reload_sample(PcReloadInput *r,const unsigned char roles[256],int active,
                                    uint64_t now,int *reload,int *fire,int *secondary)
{
    int pressed=0,released_fire=0;
    if(!active || !r->eligible || now-r->context_at>100) {
        pc_reload_reset(r);return;
    }
    for(unsigned k=1;k<256;k++) {
        if((roles[k]&1) && (r->events[k]&1) && now-r->pressed_at[k]<=100)pressed=1;
        if(r->events[k]&2)released_fire|=roles[k]&6;
        r->events[k]=0;
    }
    /* A release/repress between two polls is still a fresh shot. */
    r->blocked_fire&=~released_fire;
    if(!*fire)r->blocked_fire&=~2;
    if(!*secondary)r->blocked_fire&=~4;
    if(pressed) {
        r->pulse_until=now+34; /* at least two original 60 Hz input intervals */
        r->request_until=now+150;r->waiting=1;r->seen_reload=0;
        r->blocked_fire=(*fire?2:0)|(*secondary?4:0);
    }
    if(r->waiting) {
        if(r->reloading)r->seen_reload=1;
        if((r->seen_reload && !r->reloading) || (!r->seen_reload && now>=r->request_until)) {
            r->waiting=0;r->blocked_fire=0;
        }
    }
    /* Keep a short tap available through the polling gap. If the weapon
     * cannot reload (full/empty/busy), expire rather than queue a future action. */
    if(now<r->pulse_until || (r->waiting && !r->seen_reload))*reload=255;
    if(r->blocked_fire&2)*fire=0;
    if(r->blocked_fire&4)*secondary=0;
}
#endif
