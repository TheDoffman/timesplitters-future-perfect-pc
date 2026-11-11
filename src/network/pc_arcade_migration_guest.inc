/* Shared game-thread adapter. Original world/actor allocations stay in place;
 * capture and restoration operate only on the bounded pointer-free schema. */
#ifdef TSFP_ARCADE_MIGRATION
#include "pc_arcade_api.h"
#include "pc_match_overlay.h"
#include "pc_arcade_join.h"
extern void pc_native_visual_rebase(uint32_t tick,double fraction);
static PcArcadeControlState arcade_guest_control;
static PcArcadeCheckpoint arcade_guest_checkpoint;
static PcArcadeCheckpoint arcade_final_checkpoint;
static uint8_t arcade_guest_room[16];
static uint32_t arcade_restored_epoch,arcade_authority_epoch,arcade_published_sequence;
static uint32_t arcade_ack_sequence,arcade_ack_epoch;
static uint32_t arcade_join_restored_sequence;
static int arcade_final_sent,arcade_exit_sent;
static double arcade_last_capture,arcade_last_ack;
static double arcade_trace_at,arcade_capture_trace_at;

static int arcade_bot_apply(void *ctx,const PcNetworkBotState *s,int apply)
{
    uint32_t pool,entity,owner;uint8_t *actor,*body;
    if(!pc_network_arcade_read(ctx,0x7356d8u,&pool,4) || !pool || s->slot>=16u ||
       pool>UINT32_MAX-16u*0xea0u)return 0;
    const uint32_t a=pool+s->slot*0xea0u;
    if(!pc_network_arcade_write(ctx,a,NULL,0xea0u,0) ||
       !pc_network_arcade_read(ctx,a,&entity,4))return 0;
    if(s->body_present && !entity)return 0;
    if(entity && (!pc_network_arcade_write(ctx,entity,NULL,0x2e8u,0) ||
       !pc_network_arcade_read(ctx,entity+0x7cu,&owner,4) || owner!=a))return 0;
    if(!apply)return 1;
    actor=(uint8_t *)((uintptr_t)a+g_xbox_mem_offset);
    const uint32_t visibility=WORD_AT(a+0x2cu)&0x10u;
    memcpy(actor+0x10u,s->life,sizeof s->life);
    memcpy(actor+0x24u,s->flags,sizeof s->flags);
    WORD_AT(a+0x2cu)=(WORD_AT(a+0x2cu)&~0x10u)|visibility;
    memcpy(actor+0xa10u,s->scores,sizeof s->scores);
    if(!entity)return 1;
    body=(uint8_t *)((uintptr_t)entity+g_xbox_mem_offset);
    if(!s->body_present) {
        pc_character_ragdoll_release(entity);
        WORD_AT(entity+0x28u)&=~0x71u;FLOAT_AT(entity+0x1ccu)=0.f;
        pc_network_bots_present(entity,s);return 1;
    }
    if(WORD_AT(entity+0x258u)!=s->spawn)pc_character_ragdoll_release(entity);
    WORD_AT(entity+0x28u)=(WORD_AT(entity+0x28u)&~0x71u)|(s->entity_flags&0x71u);
    memcpy(actor+0x94u,&s->animation,4);memcpy(actor+0x30u,s->position,12);
    memcpy(actor+0xbb4u,s->muzzle,12);memcpy(actor+0xb88u,s->aim,12);
    memcpy(actor+0xb08u,s->health_scale,8);
    memcpy(body+0x64u,s->position,12);memcpy(body+0xc0u,s->angles,12);
    memcpy(body+0x70u,&s->heading,4);memcpy(body+0x1ccu,&s->health,4);
    memcpy(body+0x1d0u,&s->armor,4);memcpy(body+0x1d4u,&s->last_damage_tick,4);
    memcpy(body+0x1d8u,&s->max_health,4);memcpy(body+0x258u,&s->spawn,4);
    pc_character_camo_apply(actor,&s->camo);
    pc_network_bots_present(entity,s);
    pc_network_bots_entity_refresh(entity);pc_network_bots_entity_room(entity,s->primary_room);
    memcpy(body+0x1a8u,s->bounds_center,12);memcpy(body+0x1c0u,s->bounds_radius,12);
    return 1;
}
int pc_arcade_migration_frame(int after)
{
    const int active=pc_online_mode()==PC_ONLINE_ARCADE && WORD_AT(0x76b108u) &&
        WORD_AT(0x76b1a4u)==15u && WORD_AT(0x79094cu)<=100u && WORD_AT(0x7de30cu)==7u;
    PcArcadeControlState state={0};
    if(!after) {
        uint32_t native_peer=UINT32_MAX,node=WORD_AT(0x774028u);
        if(node && !pc_network_arcade_read(&g_xbox_mem_offset,node,&native_peer,4))native_peer=UINT32_MAX;
        pc_network_arcade_native_peer(native_peer);
        const int control=pc_network_arcade_progress(active,&state);
        pc_match_overlay_migration(active && control && (state.phase==2u ||
            (state.phase==1u && pc_network_arcade_departing())));
        if(getenv("TSFP_ARCADE_TRACE") && pc_network_bots_time()-arcade_trace_at>=2.) {
            arcade_trace_at=pc_network_bots_time();
            fprintf(stderr,"[PC-ARCADE-MIGRATION] control=%d active=%d mode=%u role=%u network=%u game=%u epoch=%u phase=%u checkpoint=%u\n",
                control,active,(unsigned)pc_online_mode(),WORD_AT(0x76b140u),WORD_AT(0x76b1a4u),
                WORD_AT(0x7de30cu),state.epoch,state.phase,state.sequence);
        }
        if(!control)return 0;
        arcade_guest_control=state;
        if(memcmp(state.room,arcade_guest_room,16)) {
            memcpy(arcade_guest_room,state.room,16);
            arcade_restored_epoch=arcade_authority_epoch=arcade_published_sequence=0;
            arcade_ack_sequence=arcade_ack_epoch=0;
            arcade_join_restored_sequence=0;
            arcade_final_sent=arcade_exit_sent=0;arcade_last_capture=arcade_last_ack=0;
        }
        if(state.phase==3u || state.phase==4u) {
            if(!arcade_exit_sent){arcade_exit_sent=1;pc_multiplayer_migration_leave();}
            return 1;
        }
        if(state.phase==1u || state.phase==5u)pc_arcade_join_frame(&state);
        if(!active)return 0;
        if(state.phase==5u)g_tsfp_arcade_migrating=1;
        const double now=pc_network_bots_time();
        if(pc_network_arcade_snapshot(&arcade_guest_checkpoint)) {
            const PcArcadeCheckpoint *s=&arcade_guest_checkpoint;
            const int team_ok=state.phase!=5u || pc_arcade_join_team(&g_xbox_mem_offset,
                pc_network_arcade_read,pc_network_arcade_write,s);
            const int target_ok=team_ok && s->level==WORD_AT(0x79094cu) &&
                pc_arcade_config_matches(s->config,(const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset));
            int bots_ok=target_ok;
            /* A client may already have removed a corpse which is alive in the
               selected backup. Recreate missing LOCAL bodies with the same
               stock allocator used by ordinary bot replication, then preflight
               the entire restore. No remote pointer crosses this boundary. */
            if(target_ok && (state.phase==2u || state.phase==5u) &&
               (arcade_restored_epoch!=state.epoch || (state.phase==5u && arcade_join_restored_sequence!=s->sequence))) {
                uint32_t pool;
                if(!pc_network_arcade_read(&g_xbox_mem_offset,0x7356d8u,&pool,4) ||
                   !pool || pool>UINT32_MAX-16u*0xea0u)bots_ok=0;
                for(unsigned i=0;bots_ok && i<s->config[2];i++) {
                    uint32_t entity;
                    if(!pc_network_arcade_read(&g_xbox_mem_offset,pool+s->bots[i].slot*0xea0u,&entity,4))bots_ok=0;
                    else if(s->bots[i].body_present && !entity && !pc_network_bots_revive(s->bots[i].slot))bots_ok=0;
                }
            }
            for(unsigned i=0;bots_ok && i<s->config[2];i++)
                bots_ok=arcade_bot_apply(&g_xbox_mem_offset,&s->bots[i],0);
            if(bots_ok && state.phase==5u &&
               (arcade_restored_epoch!=state.epoch || arcade_join_restored_sequence!=s->sequence) &&
               pc_arcade_checkpoint_restore(s,&g_xbox_mem_offset,pc_network_arcade_read,
                   pc_network_arcade_write,arcade_bot_apply,state.room)) {
                pc_native_visual_rebase(s->tick,0.);
                arcade_restored_epoch=arcade_authority_epoch=state.epoch;
                arcade_join_restored_sequence=s->sequence;
            }
            if(bots_ok && state.phase==2u && arcade_restored_epoch!=state.epoch) {
                unsigned role;
                const uint32_t local=pc_network_arcade_local_ip();
                int routes=pc_arcade_prepare_peer_routes(&g_xbox_mem_offset,pc_network_arcade_read,
                    pc_network_arcade_write,local,&state);
                for(unsigned i=0;routes && i<state.native_count;i++)
                    routes=pc_network_arcade_bind_peer(state.native_peers[i].ip,state.previous_ip);
                if(routes && pc_arcade_adopt_authority(&g_xbox_mem_offset,pc_network_arcade_read,pc_network_arcade_write,
                       local,state.candidate_ip,&role,0) &&
                   (role!=1u || pc_network_arcade_adopt_key(state.previous_ip)) &&
                   pc_arcade_checkpoint_restore(s,&g_xbox_mem_offset,pc_network_arcade_read,
                       pc_network_arcade_write,arcade_bot_apply,state.room)) {
                    pc_native_visual_rebase(s->tick,0.);
                    arcade_restored_epoch=state.epoch;
                }
            }
            if(bots_ok && (state.sequence!=arcade_ack_sequence || state.epoch!=arcade_ack_epoch ||
                           now-arcade_last_ack>=.2)) {
                pc_network_arcade_ack(arcade_restored_epoch==state.epoch &&
                    (state.phase==2u || (state.phase==5u && arcade_join_restored_sequence==s->sequence)));
                arcade_last_ack=now;arcade_ack_sequence=state.sequence;arcade_ack_epoch=state.epoch;
            }
            if(state.phase==5u && getenv("TSFP_ARCADE_TRACE") && now-arcade_capture_trace_at>=2.) {
                const uint8_t *local=(const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
                unsigned mismatch=0;
                while(mismatch<PC_ARCADE_CHECKPOINT_CONFIG_BYTES && s->config[mismatch]==local[mismatch])mismatch++;
                fprintf(stderr,"[PC-ARCADE-JOIN] catchup sequence=%u team=%d target=%d bots=%d restored=%u valid=%d mismatch=%u\n",
                    s->sequence,team_ok,target_ok,bots_ok,arcade_join_restored_sequence,
                    pc_arcade_checkpoint_valid(s,state.room),mismatch);
                arcade_capture_trace_at=now;
            }
        }
        if(state.phase==2u || state.phase==5u)return 1;
        if(state.phase==1u && state.previous_ip && arcade_authority_epoch!=state.epoch) {
            unsigned role;
            if(arcade_restored_epoch!=state.epoch ||
               !pc_arcade_adopt_authority(&g_xbox_mem_offset,pc_network_arcade_read,pc_network_arcade_write,
                   pc_network_arcade_local_ip(),state.host_ip,&role,1)) {
                g_tsfp_arcade_migrating=1;return 1;
            }
            arcade_authority_epoch=state.epoch;
            fprintf(stderr,"[PC-ARCADE-MIGRATION] resumed epoch=%u role=%u tick=%u\n",
                state.epoch,role,WORD_AT(0x7de318u));
        }
        if(pc_network_arcade_departing() && arcade_final_sent)return 1;
        return 0;
    }
    state=arcade_guest_control;
    if(!active || state.phase!=1u || WORD_AT(0x76b140u)!=1u || !state.epoch)return 0;
    const int leaving=pc_network_arcade_departing();
    const double now=pc_network_bots_time();
    if(leaving && arcade_final_sent) {
        if(now-arcade_last_capture>=.2) {
            pc_network_arcade_publish(&arcade_final_checkpoint);
            pc_network_arcade_send_leave();arcade_last_capture=now;
        }
        return 0;
    }
    /* Leave time for WAN transfer and an engine ACK before the retained pair
       ages out. A graceful leave still captures its final frame immediately. */
    if(!leaving && now-arcade_last_capture<.5)return 0;
    if(arcade_published_sequence<state.sequence)arcade_published_sequence=state.sequence;
    if(arcade_published_sequence==UINT32_MAX)return 0;
    const int captured=pc_arcade_checkpoint_capture(&arcade_guest_checkpoint,&g_xbox_mem_offset,
       pc_network_arcade_read,pc_network_arcade_bot_read,state.room,state.epoch,arcade_published_sequence+1u);
    if(!captured && getenv("TSFP_ARCADE_TRACE") && now-arcade_capture_trace_at>=2.) {
        arcade_capture_trace_at=now;
        fprintf(stderr,"[PC-ARCADE-MIGRATION] capture refused tick=%u session=%u level=%u\n",
            WORD_AT(0x7de318u),WORD_AT(0x76b108u),WORD_AT(0x79094cu));
        const uint8_t *config=(const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
        for(unsigned i=config[0];i<config[0]+config[2] && i<16u;i++) {
            PcNetworkBotState b;const int available=pc_network_arcade_bot_read(NULL,i,&b);
            fprintf(stderr,"[PC-ARCADE-MIGRATION] bot=%u available=%d tick=%u valid=%d\n",
                i,available,available?b.tick:0u,available?pc_network_bot_state_valid(&b,config[0],config[2]):0);
        }
    }
    if(captured &&
       pc_network_arcade_publish(&arcade_guest_checkpoint)) {
        arcade_published_sequence++;arcade_last_capture=now;
        if(leaving){arcade_final_checkpoint=arcade_guest_checkpoint;arcade_final_sent=1;pc_network_arcade_send_leave();}
    }
    return 0;
}
#endif
