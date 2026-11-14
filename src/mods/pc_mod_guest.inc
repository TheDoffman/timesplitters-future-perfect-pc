/* Supported US title adapter. Derived score/rank rows only; never write raw
   kills/deaths, replay damage, or choose guest addresses from a worker reply. */
#include "pc_mod_game.h"
#include <math.h>
static TsfpModMatch *pc_mod_bound;
static uint64_t pc_mod_epoch;
static uint32_t pc_mod_pool,pc_mod_scene,pc_mod_tick,pc_mod_running,pc_mod_stop,pc_mod_finished;
static uint64_t pc_mod_tick_sent=UINT64_MAX;
extern void sub_00064810(void),sub_00067450(void);

int pc_mod_game_capture(TsfpModWorld *out) {
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    uint32_t stage=WORD_AT(0x7de30cu),scene=WORD_AT(0x79094cu),pool=WORD_AT(0x7356d8u);
    uint32_t mode=WORD_AT(0x7de454u),flags=WORD_AT(0x7de458u),tick=WORD_AT(0x7de318u);
    uint32_t humans=WORD_AT(0x790950u),bots=WORD_AT(0x7a2958u),rate=WORD_AT(0x4e8cecu);
    /* Initial integration is the offline, non-team Arcade rules-index0 path.
       Other title modes/network/save bootstrap contracts remain unimplemented. */
    if((stage!=7u && stage!=8u) || ((mode>>8)&255u)!=9u || ((mode>>16)&255u)!=0u ||
       (flags&(1u|0x40u|0x08000000u|0x20000000u)) || WORD_AT(0x76b108u) || WORD_AT(0x7de474u) ||
       !humans || humans>4u || bots>16u-humans || !rate || rate>1000 || scene>=100u ||
       !guest_writable(pool,(humans+bots)*0xea0u)) {pc_mod_running=0;return 0;}
    if(!pc_mod_running || pool!=pc_mod_pool || scene!=pc_mod_scene || tick<pc_mod_tick) {
        if(pc_mod_epoch==UINT64_MAX)return 0;
        ++pc_mod_epoch;
    }
    pc_mod_running=1;pc_mod_pool=pool;pc_mod_scene=scene;pc_mod_tick=tick;
    out->epoch=pc_mod_epoch;out->tick=tick;out->player_count=humans+bots;out->tick_rate=rate;
    out->offline=out->authoritative=1;
    for(unsigned i=0;i<out->player_count;++i) {
        uint32_t actor=pool+i*0xea0u,entity=WORD_AT(actor),name=WORD_AT(actor+0x70u);
        tsfp_script_player *p=&out->players[i];
        if(WORD_AT(actor+8u)!=i || !name)return 0;
        /* Stock scoring uses actor+84 even for non-team identity comparisons.
           This adapter admits no team mode: the public API reports no team. */
        p->id=i+1;p->team=0;
        memcpy(&p->score,(void *)((uintptr_t)(actor+0xa6cu)+g_xbox_mem_offset),4);
        float position[3];memcpy(position,(void *)((uintptr_t)(actor+0x30u)+g_xbox_mem_offset),sizeof(position));
        p->x=position[0];p->y=position[1];p->z=position[2];
        float health=0;
        if(entity) {
            if(!guest_writable(entity,0x25cu) || WORD_AT(entity+0x7cu)!=actor)return 0;
            memcpy(&health,(void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),4);
        }
        if(!isfinite(health) || health>1000000.f || p->team>8 || p->score < -1000000 || p->score>1000000 ||
           !isfinite(p->x) || !isfinite(p->y) || !isfinite(p->z) || fabsf(p->x)>10000000.f || fabsf(p->y)>10000000.f || fabsf(p->z)>10000000.f)return 0;
        p->health=health>0?(int32_t)health:0;
        /* Actor/name identity survives body replacement during respawn. */
        out->identities[i]=((uint64_t)actor<<32)|name;
    }
    return 1;
}
int pc_mod_game_bind(TsfpModMatch *match) {
    if(match) {TsfpModWorld world;if(!pc_mod_game_capture(&world) || !tsfp_mod_match_matches(match,&world))return 0;}
    pc_mod_bound=match;pc_mod_stop=pc_mod_finished=0;pc_mod_tick_sent=UINT64_MAX;return 1;
}
static unsigned pc_mod_slot(uint32_t entity,const TsfpModWorld *world) {
    if(!entity || !guest_writable(entity,0x25cu))return 0;
    uint32_t actor=WORD_AT(entity+0x7cu),pool=WORD_AT(0x7356d8u);
    if(actor<pool || (actor-pool)%0xea0u)return 0;
    unsigned slot=(actor-pool)/0xea0u;
    return slot<world->player_count && WORD_AT(actor)==entity?slot+1u:0;
}
void pc_mod_game_death(void (*original)(void)) {
    TsfpModWorld world;uint32_t victim=0,serial=0;unsigned killer=0,target=0;
    if(pc_mod_bound && pc_mod_game_capture(&world) && tsfp_mod_match_matches(pc_mod_bound,&world) && WORD_AT(0x7de30cu)==7u) {
        uint32_t hit=WORD_AT(g_esp+4u);
        if(guest_writable(hit,8u)) {
            victim=WORD_AT(hit);target=pc_mod_slot(victim,&world);killer=pc_mod_slot(WORD_AT(hit+4u),&world);
            if(target)serial=WORD_AT(victim+0x258u);
        }
    }
    original();
    /* Stock BABA0 increments victim+258 only at its accepted-death epilogue.
       Early authorization/Story departure returns do not manufacture events. */
    if(target && pc_mod_game_capture(&world) && tsfp_mod_match_matches(pc_mod_bound,&world) &&
       guest_writable(victim,0x25cu) && pc_mod_slot(victim,&world)==target && (uint32_t)(WORD_AT(victim+0x258u)-serial)==1u)
        (void)tsfp_mod_match_enqueue(pc_mod_bound,&world,TSFP_SCRIPT_DEATH,killer,target);
}
void pc_mod_game_score(void (*original)(void)) {
    TsfpModWorld world;int32_t score;uint32_t slot=WORD_AT(g_esp+4u);
    if(pc_mod_bound && slot<32 && pc_mod_game_capture(&world) && tsfp_mod_match_score(pc_mod_bound,&world,slot+1u,&score)) {
        g_eax=(uint32_t)score;g_esp+=4u;return;
    }
    original();
}
void pc_mod_game_ranking(void (*original)(void)) {
    original();
    if(!pc_mod_bound)return;
    TsfpModWorld world;int32_t ranks[32];uint32_t order[32];
    if(!pc_mod_game_capture(&world) || !tsfp_mod_match_ranking(pc_mod_bound,&world,ranks,order))return;
    /* Capture validates every actor extent first. This uninterrupted write batch
       changes only the derived rows used by stock sorting/HUD/results. */
    uint32_t pool=WORD_AT(0x7356d8u);int32_t scores[32];
    if(!guest_writable(0x7de4c0u,world.player_count*4u))return;
    for(unsigned i=0;i<world.player_count;++i)if(!tsfp_mod_match_score(pc_mod_bound,&world,i+1u,&scores[i]))return;
    for(unsigned i=0;i<world.player_count;++i) {
        WORD_AT(pool+i*0xea0u+0xa6cu)=(uint32_t)scores[i];
        WORD_AT(pool+i*0xea0u+0xa68u)=(uint32_t)ranks[i];
        WORD_AT(0x7de4c0u+i*4u)=order[i]?order[i]-1u:UINT32_MAX;
    }
}
void pc_mod_game_limits(void (*original)(void)) {
    TsfpModWorld world;
    if(pc_mod_bound && pc_mod_game_capture(&world) && tsfp_mod_match_matches(pc_mod_bound,&world)) {g_esp+=4u;return;}
    original();
}
void pc_mod_game_frame(void (*original)(void)) {
    original();
    if(!pc_mod_bound)return;
    GuestRegisters saved;save_registers(&saved);
    TsfpModWorld world;
    if(!pc_mod_game_capture(&world)){tsfp_mod_match_close(pc_mod_bound);restore_registers(&saved);return;}
    if(!pc_mod_stop && (WORD_AT(0x7de30cu)==8u || tsfp_mod_match_ended(pc_mod_bound,&world,NULL))) {
        if(tsfp_mod_match_enqueue(pc_mod_bound,&world,TSFP_SCRIPT_STOP,0,0)>0)pc_mod_stop=1;
    }else if(!pc_mod_stop && world.tick!=pc_mod_tick_sent) {
        if(tsfp_mod_match_enqueue(pc_mod_bound,&world,TSFP_SCRIPT_TICK,0,0)>0)pc_mod_tick_sent=world.tick;
    }
    TsfpModCommit commit;
    int status=tsfp_mod_match_pump(pc_mod_bound,&world,&commit);
    if(status>0) {
        /* Refresh through the real stock score pass; its adapter applies the
           committed model and preserves raw statistics. No worker pointers. */
        guest_call(sub_00064810,1,0u);
        uint32_t winner;
        if(!pc_mod_finished && tsfp_mod_match_ended(pc_mod_bound,&world,&winner)) {
            (void)winner;pc_mod_finished=1;
            if(!pc_mod_stop && tsfp_mod_match_enqueue(pc_mod_bound,&world,TSFP_SCRIPT_STOP,0,0)>0)pc_mod_stop=1;
            if(WORD_AT(0x7de30cu)==7u)guest_call(sub_00067450,1,1u);
        }
        /* Message transactions are available to the future shared HUD adapter;
           do not treat arbitrary text as a format string or console command. */
    }
    restore_registers(&saved);
}
