/* Host roster/activity prototype. Guest AI replication remains unfinished.
 * Called through a reproducible wrapper, never by editing generated game C.
 * Reject non-network guest states before querying host settings: these hooks
 * also run for every offline actor's animation and collision updates. Keep
 * eligible network calls live so menu changes and bridge readiness still work. */
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
#include <stdlib.h>
#include <stdio.h>
#include "pc_network_bots.h"
#include "pc_network_bot_motion.h"
static unsigned pc_network_bots_option(const char *name,unsigned fallback,unsigned maximum)
{
    const char *text=getenv(name);char *end;unsigned long value;
    if (!text || !*text) return fallback;
    value=strtoul(text,&end,10);
    return *end || value>maximum ? fallback : (unsigned)value;
}
#endif
#include "pc_network_bots_lobby.inc"
#ifdef _WIN32
extern void sub_000658D0_gen(void);
int pc_story_network_prepare(uint32_t address);
void pc_story_network_activate(void);
int pc_story_network_actor_active(uint32_t caller,unsigned index,uint32_t entity);
int pc_story_network_hit_trace(void (*original)(void));
int pc_story_network_damage(void);
int pc_story_network_replica(uint32_t entity);
int pc_story_network_replica_bounds(uint32_t entity);
int pc_story_network_cinematic_pose(uint32_t entity);
int pc_story_network_shot(void (*original)(void));
uint32_t pc_story_network_wire_entity(unsigned slot);
uint32_t pc_story_network_world_key(uint32_t entity);
uint32_t pc_story_network_world_entity(uint32_t key);
#endif
static void pc_network_bots_guest_config(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    /* Recheck after the stock builder clears byte 2, immediately before both
       final serialization and the host's pending-load copy. */
    uint32_t address=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
    if(address==0x0078AD58u) pc_network_bots_lobby_prepare(1);
    const int story=pc_story_network_prepare(address);
#endif
    original();
#ifdef _WIN32
    /* Both original entry points retain their ABI. Only the pending-load
       function may change simulation mode, never the start serializer. */
    if(story && original==sub_000658D0_gen) pc_story_network_activate();
#endif
}

/* The stock activity predicate checks controller records, so it rejects every
 * AI slot. Only actor/HUD callers may accept an allocated bot: controller
 * enumeration and network player serializers must retain their human checks. */
static void pc_network_bots_guest_active(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    const uint32_t caller=*(uint32_t *)((uintptr_t)g_esp+g_xbox_mem_offset);
    const unsigned story_index=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
    if(pc_story_network_actor_active(caller,story_index,g_ebx)) {
        g_eax=1u;g_esp+=4u;return;
    }
    switch (caller) {
    case 0x00064743u: /* team score ordering */
    case 0x00064856u: /* score calculation: human + AI actor records */
    case 0x00064A06u: /* completed-round score update */
    case 0x0006562Du: /* match winner: actor ranks */
    case 0x0013F82Au: /* scoreboard row count */
    case 0x002E72B6u: /* paused Scores: team row count */
    case 0x002E744Bu: /* paused Scores: sorted actor rows */
    case 0x000D8E5Du: /* actor update, including host-owned AI */
    case 0x00133BC9u: /* radar: actor pool, already includes human + AI slots */
    case 0x0013E0BBu: /* HUD character list: humans + AI, actor pool */
    case 0x0013E230u: /* HUD character draw: humans + AI, actor pool */
    case 0x0013EAFEu: /* HUD roster eligibility: humans + AI */
    case 0x0013EBDAu:
        if (*(uint32_t *)((uintptr_t)0x0076B108u+g_xbox_mem_offset) &&
            pc_network_enabled()) {
            const uint8_t *config=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
            const unsigned index=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
            const unsigned humans=config[0],bots=config[2];
            if (humans && bots && bots<=10u && humans+bots<=16u &&
                index>=humans && index<humans+bots &&
                *(uint32_t *)((uintptr_t)0x007356D8u+g_xbox_mem_offset)) {
                g_eax=1u;g_esp+=4u;return;
            }
        }
        break;
    default: break;
    }
#endif
    original();
}

#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
/* AI entities are not controller records. Give the original combat/event
 * serializers a separate, slot-based reference rather than a guest pointer
 * or an invalid entry in the human controller table. */
static uint32_t pc_network_bots_guest_actor(unsigned index)
{
    const uint8_t *config=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
    const unsigned humans=config[0],bots=config[2];
    uint32_t pool;
    if (!*(uint32_t *)((uintptr_t)0x0076B108u+g_xbox_mem_offset) ||
        !humans || !bots || bots>10u || humans+bots>16u ||
        index<humans || index>=humans+bots || !pc_network_enabled()) return 0;
    pool=*(uint32_t *)((uintptr_t)0x007356D8u+g_xbox_mem_offset);
    if (!pool) return 0;
    return pool+index*0xea0u;
}
static uint32_t pc_network_bots_guest_entity(unsigned index)
{
    uint32_t actor=pc_network_bots_guest_actor(index);
    return actor?*(uint32_t *)((uintptr_t)actor+g_xbox_mem_offset):0;
}
static unsigned pc_network_bots_guest_slot(uint32_t entity)
{
    unsigned slot;
    if(entity) for(slot=0;slot<16u;slot++)
        if(pc_network_bots_guest_entity(slot)==entity) return slot;
    return 16u;
}
#endif
/* Evaluate replicas in the engine's animation pass, after its per-frame bone
   allocation. The host already advanced these channels and moved the body.
   sub_0005FDC0's second argument selects advance versus frozen evaluation. */
static void pc_network_bots_guest_pose(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    uint32_t stack=g_esp;
    uint32_t entity=*(uint32_t *)((uintptr_t)(stack+4u)+g_xbox_mem_offset);
    if(pc_story_network_cinematic_pose(entity)) {
        uint32_t *advance=(uint32_t *)((uintptr_t)(stack+8u)+g_xbox_mem_offset),saved=*advance;
        *advance=0;original();*advance=saved;return;
    }
    if(*(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==2u &&
       *(uint32_t *)((uintptr_t)0x0076b1a4u+g_xbox_mem_offset)==15u &&
       pc_network_enabled() &&
       (pc_network_bots_guest_slot(entity)<16u || pc_story_network_replica(entity))) {
        uint32_t *advance=(uint32_t *)((uintptr_t)(stack+8u)+g_xbox_mem_offset);
        uint32_t saved=*advance;
        *advance=0;
        original();
        *advance=saved;
        pc_network_bots_pose_weapons(entity);
        pc_character_camo_present(entity);
        return;
    }
#endif
    original();
}
/* The stock death sender authorizes the SHOOTER. That refuses a guest's
   lethal hit on the host's NPC, leaving zero HP without a completed death.
   NPC death is owned by the host; its snapshots carry the resulting body and
   victim score column. Do not emit a second stock death event for these NPCs. */
static void pc_network_bots_guest_death_gate(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    uint32_t context=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
    if(context) {
        const uint32_t entity=*(uint32_t *)((uintptr_t)context+g_xbox_mem_offset);
        for(unsigned slot=0;entity && slot<32u;slot++) if(pc_story_network_wire_entity(slot)==entity) {
            g_eax=*(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==1u;
            g_esp+=4u;return;
        }
    }
    if(context && pc_network_bots_guest_slot(*(uint32_t *)((uintptr_t)context+g_xbox_mem_offset))<16u) {
        g_eax=*(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==1u;
        g_esp+=4u;return;
    }
#endif
    original();
}
/* A successful host firing call is an event, separate from locomotion flags.
   Keep only numeric weapon/aim data; the receiver resolves its own actors. */
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
static PcNetworkBotShot pc_network_bot_shots[16];
static struct PcNetworkBotBounds {
    uint32_t entity,spawn;
    float center[3],radius[3];
} pc_network_bot_bounds[16];
#endif
/* The original pose/collision path recalculates bounds between network
   snapshots. Restore authoritative geometry at that boundary too. ESI is
   the original function's entity argument, not a stack parameter. */
static void pc_network_bots_guest_bounds(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    const uint32_t entity=g_esi;
    original();
    if(pc_story_network_replica_bounds(entity)) return;
    if(entity &&
       *(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==2u &&
       *(uint32_t *)((uintptr_t)0x0076b1a4u+g_xbox_mem_offset)==15u &&
       pc_network_enabled()) {
        unsigned slot;
        for(slot=0;slot<16u;slot++) {
            const struct PcNetworkBotBounds *bounds=&pc_network_bot_bounds[slot];
            if(bounds->entity==entity && pc_network_bots_guest_entity(slot)==entity &&
               bounds->spawn==*(uint32_t *)((uintptr_t)(entity+0x258u)+g_xbox_mem_offset)) {
                memcpy((void *)((uintptr_t)(entity+0x1a8u)+g_xbox_mem_offset),bounds->center,12);
                memcpy((void *)((uintptr_t)(entity+0x1c0u)+g_xbox_mem_offset),bounds->radius,12);
                break;
            }
        }
    }
#else
    original();
#endif
}
static void pc_network_bots_guest_shot(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(pc_story_network_shot(original)) return;
    PcNetworkBotShot shot;unsigned slot=16u;
    if(*(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==1u &&
       *(uint32_t *)((uintptr_t)0x0076b1a4u+g_xbox_mem_offset)==15u &&
       pc_network_enabled()) {
        const uint32_t *args=(const uint32_t *)((uintptr_t)g_esp+g_xbox_mem_offset);
        const uint32_t entity=args[1];unsigned i;
        for(i=0;i<16u;i++) if(entity && pc_network_bots_guest_entity(i)==entity) {slot=i;break;}
        if(slot<16u && args[2]<=1u && args[3]<=1u && args[4]<=1u) {
            uint32_t actor=*(uint32_t *)((uintptr_t)(entity+0x7cu)+g_xbox_mem_offset);
            const uint8_t *a=(const uint8_t *)((uintptr_t)actor+g_xbox_mem_offset);
            uint32_t target;memcpy(&target,a+4,4);
            memset(&shot,0,sizeof shot);shot.hand=args[2];shot.mode=args[3];shot.flags=args[4];
            memcpy(&shot.weapon,a+0xd4u+shot.hand*0x130u,4);
            memcpy(shot.transform,a+0xa4u+shot.hand*0x130u,48);
            memcpy(shot.aim,a+0xb88u,12);memcpy(shot.muzzle,a+0xbb4u,12);
            shot.target_slot=UINT32_MAX;
            for(i=0;i<16u;i++) {
                uint32_t pool=*(uint32_t *)((uintptr_t)0x007356d8u+g_xbox_mem_offset);
                if(pool && target && *(uint32_t *)((uintptr_t)(pool+i*0xea0u)+g_xbox_mem_offset)==target) {shot.target_slot=i;break;}
            }
            shot.tick=*(uint32_t *)((uintptr_t)0x007de318u+g_xbox_mem_offset);
            shot.spawn=*(uint32_t *)((uintptr_t)(entity+0x258u)+g_xbox_mem_offset);
        } else slot=16u;
    }
    original();
    if(slot<16u && g_eax && shot.weapon>=0 && shot.weapon<66) {
        const uint8_t *config=(const uint8_t *)((uintptr_t)0x007de400u+g_xbox_mem_offset);
        shot.serial=pc_network_bot_shots[slot].serial+1u;
        if(!shot.serial) shot.serial=1u;
        if(pc_network_bot_shot_valid(&shot,config[0]+config[2])) pc_network_bot_shots[slot]=shot;
    }
#else
    original();
#endif
}
/* The stock score builder also checks peer/controller eligibility after
 * activity. Only its AI iterations use the offline actor branch. Human peer
 * membership and all other network/AI-authority decisions remain original. */
static void pc_network_bots_guest_score_network(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    uint32_t caller=*(uint32_t *)((uintptr_t)g_esp+g_xbox_mem_offset);
    unsigned slot=UINT32_MAX;
    if(caller==0x0006486eu) slot=g_ebx;
    else if(caller==0x00064b18u || caller==0x00064ba5u) slot=g_edi;
    /* Completed-match records feed Results portraits and the Scores table.
       This iteration has a slot in ESI; later calls use ESI as an actor
       pointer and must retain the network path (including score offsets).
       The stock reliable results packet already carries all 16 slots. */
    else if(caller==0x00063f05u) slot=g_esi;
    /* Round score-limit scan uses EDI as its actor slot. Its network branch
       filters controller flags, which are absent for AI even after scoring.
       Retain the original limit/team/authority decisions after eligibility. */
    else if(caller==0x00067868u) slot=g_edi;
    /* A dead bot keeps its score row even while its body is missing. */
    if(pc_network_bots_guest_actor(slot)) {g_eax=0;g_esp+=4u;return;}
#endif
    original();
}
/* Combat first checks whether a victim can be encoded. Accept the same AI
 * slots as the codec, without inventing controller records or object IDs. */
static void pc_network_bots_guest_serializable(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    const uint32_t entity=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
    unsigned index;
    if(pc_story_network_world_key(entity)) {g_eax=1u;g_esp+=4u;return;}
    if(entity) for(index=0;index<32u;index++) if(pc_story_network_wire_entity(index)==entity) {
        g_eax=1u;g_esp+=4u;return;
    }
    if(entity) for(index=0;index<16u;index++) {
        if(pc_network_bots_guest_entity(index)==entity) {
            g_eax=1u;g_esp+=4u;return;
        }
    }
#endif
    original();
}
static void pc_network_bots_guest_encode(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    unsigned index;
    const uint32_t world_key=pc_story_network_world_key(g_ecx);
    if(world_key) {
        uint32_t *cursor=(uint32_t *)((uintptr_t)g_eax+g_xbox_mem_offset);
        uint8_t *bytes=(uint8_t *)((uintptr_t)*cursor+g_xbox_mem_offset);
        bytes[0]=5u;memcpy(bytes+1u,&world_key,4u);*cursor+=5u;g_esp+=4u;return;
    }
    if(g_ecx) for(index=0;index<32u;index++) if(pc_story_network_wire_entity(index)==g_ecx) {
        uint32_t *cursor=(uint32_t *)((uintptr_t)g_eax+g_xbox_mem_offset);
        uint8_t *bytes=(uint8_t *)((uintptr_t)*cursor+g_xbox_mem_offset);
        bytes[0]=4u;bytes[1]=(uint8_t)index;*cursor+=2u;g_esp+=4u;return;
    }
    if (g_ecx) for (index=0;index<16u;index++) {
        if (pc_network_bots_guest_entity(index)==g_ecx) {
            uint32_t *cursor=(uint32_t *)((uintptr_t)g_eax+g_xbox_mem_offset);
            uint8_t *bytes=(uint8_t *)((uintptr_t)*cursor+g_xbox_mem_offset);
            bytes[0]=4u;bytes[1]=(uint8_t)index;*cursor+=2u;
            g_esp+=4u;return;
        }
    }
#endif
    original();
}
static void pc_network_bots_guest_decode(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    uint32_t *cursor=(uint32_t *)((uintptr_t)g_ecx+g_xbox_mem_offset);
    const uint8_t *bytes=(const uint8_t *)((uintptr_t)*cursor+g_xbox_mem_offset);
    if(bytes[0]==5u) {
        uint32_t key;memcpy(&key,bytes+1u,4u);g_eax=pc_story_network_world_entity(key);
        *cursor+=5u;g_esp+=4u;return;
    }
    if (bytes[0]==4u) {
        g_eax=pc_story_network_wire_entity(bytes[1]);
        if(!g_eax) g_eax=pc_network_bots_guest_entity(bytes[1]);
        *cursor+=2u;
        g_esp+=4u;return;
    }
#endif
    original();
}

static void pc_network_bots_guest_frame(void (*original)(void))
{
    original();
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    static uint32_t last_tick;static int sent;
    static uint32_t seen_shots[16];
    static int16_t seen_kills[16][16];
    static PcNetworkBotMotion motion[16];
    static unsigned char corpse_copied[16];
    static uint32_t motion_match,motion_pool,motion_host;
    const uint8_t *config=(const uint8_t *)((uintptr_t)0x007DE400u+g_xbox_mem_offset);
    unsigned role=*(uint32_t *)((uintptr_t)0x0076B140u+g_xbox_mem_offset);
    const unsigned humans=config[0],bots=config[2];
    unsigned index;uint32_t pool,host_ip=0,host_peer,tick,match;
    if(*(uint32_t *)((uintptr_t)0x0076B1A4u+g_xbox_mem_offset)!=15u ||
       !humans || !bots || bots>10u || humans+bots>16u || (role!=1u && role!=2u) ||
       !pc_network_enabled()) {
        pc_network_bots_context(0,0,0,0,0);sent=0;
        memset(pc_network_bot_shots,0,sizeof pc_network_bot_shots);memset(seen_shots,0,sizeof seen_shots);
        memset(pc_network_bot_bounds,0,sizeof pc_network_bot_bounds);
        memset(motion,0,sizeof motion);memset(corpse_copied,0,sizeof corpse_copied);
        memset(seen_kills,0,sizeof seen_kills);return;
    }
    pool=*(uint32_t *)((uintptr_t)0x007356D8u+g_xbox_mem_offset);
    if(!pool) return;
    host_peer=*(uint32_t *)((uintptr_t)0x00774040u+g_xbox_mem_offset);
    if(role==2u) {
        if(host_peer>=16u) return;
        /* Original XNet sockaddr table, also used by sub_0002E530. The
           higher-level peer record contains a connection handle, not an IP. */
        host_ip=*(uint32_t *)((uintptr_t)(0x0066648Cu+host_peer*16u)+g_xbox_mem_offset);
    }
    match=config[0x99]|((uint32_t)config[0x54]<<8)|((uint32_t)config[0x55]<<16)|((uint32_t)config[0x56]<<24);
    if(role!=2u || motion_match!=match || motion_pool!=pool || motion_host!=host_ip) {
        memset(motion,0,sizeof motion);memset(corpse_copied,0,sizeof corpse_copied);
        motion_match=match;motion_pool=pool;motion_host=host_ip;
    }
    tick=*(uint32_t *)((uintptr_t)0x007DE318u+g_xbox_mem_offset);
    pc_network_bots_context(role,host_ip,match,humans,bots);
    if(role==1u) {
        uint32_t addresses[16];unsigned count=0,peer;
        for(peer=0;peer<16u;peer++) {
            if(*(uint32_t *)((uintptr_t)(0x00666040u+peer*4u)+g_xbox_mem_offset))
                addresses[count++]=*(uint32_t *)((uintptr_t)(0x0066648Cu+peer*16u)+g_xbox_mem_offset);
        }
        pc_network_bots_targets(addresses,count);
    }
    for(index=humans;index<humans+bots;index++) {
        uint32_t entity=pc_network_bots_guest_entity(index);
        uint8_t *actor=(uint8_t *)((uintptr_t)(pool+index*0xea0u)+g_xbox_mem_offset);
        uint8_t *body=(uint8_t *)((uintptr_t)entity+g_xbox_mem_offset);
        PcNetworkBotState state;
        if(role==2u) {
            PcNetworkBotState received;
            double now=pc_network_bots_time();
            if(pc_network_bots_receive(index,&received) &&
               pc_network_bot_state_valid(&received,humans,bots)) {
                const PcNetworkBotState *previous=&motion[index].target;
                /* Copy the last posed dead body before absence or a new spawn
                   overwrites it. Host packet pointers never cross this boundary. */
                if(motion[index].ready && previous->body_present && previous->health<=0.f &&
                   (!received.body_present || received.spawn!=previous->spawn) &&
                   !corpse_copied[index] && entity)
                    corpse_copied[index]=(unsigned char)pc_network_bots_preserve_corpse(entity,previous->spawn);
                if(received.body_present && (!motion[index].ready || !previous->body_present ||
                   received.spawn!=previous->spawn)) corpse_copied[index]=0;
                pc_network_bot_motion_push(&motion[index],&received,now);
            }
            if(!pc_network_bot_motion_sample(&motion[index],now,&state)) continue;
            unsigned killer;
            /* Scores outlive bodies. Apply the confirmed NPC victim column
               even when the host has already removed its corpse. */
            memcpy(actor+0xa10,state.scores,sizeof state.scores);
            for(killer=0;killer<humans+bots;killer++) {
                /* The roster survives corpse removal. Retry an undelivered
                   local HUD message when its view is temporarily unavailable. */
                if(state.killed_by[killer]<seen_kills[index][killer])
                    seen_kills[index][killer]=state.killed_by[killer];
                while(state.killed_by[killer]>seen_kills[index][killer]) {
                    if(!pc_network_bots_kill_notice(index,killer)) break;
                    seen_kills[index][killer]++;
                }
                memcpy((void *)((uintptr_t)(pool+killer*0xea0u+0xa10u+index*2u)+g_xbox_mem_offset),
                    &state.killed_by[killer],2);
            }
            if(!state.body_present) {
                if(entity) {
                    uint32_t flags;float zero=0.f;
                    memcpy(&flags,body+0x28,4);flags&=~0x71u;memcpy(body+0x28,&flags,4);
                    memcpy(body+0x1cc,&zero,4);
                    pc_network_bots_present(entity,&state);
                }
                memset(&pc_network_bot_bounds[index],0,sizeof pc_network_bot_bounds[index]);
                continue;
            }
            if(!entity && state.health>0.f && (state.entity_flags&1u))
                entity=pc_network_bots_revive(index);
            if(!entity) continue;
            body=(uint8_t *)((uintptr_t)entity+g_xbox_mem_offset);
        } else if(role==2u) continue;
        if(role==1u) {
            uint32_t flags,life,dead;float health;unsigned killer;
            if(sent && tick-last_tick<3u) continue;
            memset(&state,0,sizeof state);state.slot=index;state.tick=tick;
            memcpy(state.scores,actor+0xa10,sizeof state.scores);
            for(killer=0;killer<humans+bots;killer++)
                memcpy(&state.killed_by[killer],(void *)((uintptr_t)(pool+killer*0xea0u+0xa10u+index*2u)+g_xbox_mem_offset),2);
            /* Explicit absence prevents a lost death/respawn transition from
               leaving a live, firing replica frozen at its last snapshot. */
            if(!entity) {pc_network_bots_send(&state);continue;}
            state.body_present=1u;
            memcpy(&flags,body+0x28,4);memcpy(&life,actor+0x18,4);
            memcpy(&dead,actor+0x2c,4);memcpy(&health,body+0x1cc,4);
            /* The network startup path leaves allocated AI bodies disabled.
               Activate living bots after their spawn state, never dead or
               inactive actors. Bit 0 enables drawing; bits 0x60 enable the original hit
               collision passes (including sub_0016CCD0). */
            if(life && life!=2u && !(dead&4u) && health>0.f && (flags&0x61u)!=0x61u) {
                flags|=0x61u;memcpy(body+0x28,&flags,4);
                pc_network_bots_entity_refresh(entity);
            }
            /* 0x10 is the original damage-acceptance bit, separate from hit
               collision. It must also follow death/respawn on the host. */
            pc_character_camo_capture(&state.camo,actor);
            state.entity_flags=flags&0x71u;
            memcpy(&state.primary_room,body+0x34,4);
            memcpy(state.flags,actor+0x24,12);memcpy(state.life,actor+0x10,16);
            memcpy(&state.animation,actor+0x94,4);memcpy(state.position,body+0x64,12);
            state.health=health;memcpy(&state.armor,body+0x1d0,4);
            memcpy(&state.last_damage_tick,body+0x1d4,4);memcpy(&state.max_health,body+0x1d8,4);
            memcpy(state.health_scale,actor+0xb08,8);
            memcpy(state.angles,body+0xc0,12);
            memcpy(&state.heading,body+0x70,4);
            memcpy(state.aim,actor+0xb88,12);memcpy(state.muzzle,actor+0xbb4,12);
            memcpy(&state.spawn,body+0x258,4);
            memcpy(state.bounds_center,body+0x1a8,12);
            memcpy(state.bounds_radius,body+0x1c0,12);
            {
                uint32_t model;memcpy(&model,body+4,4);
                if(model) {
                    PcCharacterPose pose;
                    pc_character_pose_read(&pose,(const void *)((uintptr_t)model+g_xbox_mem_offset));
                    memcpy(&state.clip,&pose,sizeof pose);
                }
            }
            memcpy(state.scores,actor+0xa10,sizeof state.scores);
            state.shot=pc_network_bot_shots[index];
            pc_network_bots_send(&state);
        } else {
            uint32_t flags;
            memcpy(&flags,body+0x28,4);flags=(flags&~0x71u)|(state.entity_flags&0x71u);
            memcpy(body+0x28,&flags,4);
            /* Actor +2c bit 0x10 is set by this machine's visibility pass
               (sub_000A0130). Never replace it with the host camera's bit. */
            memcpy(&flags,actor+0x2c,4);
            memcpy(actor+0x24,state.flags,12);
            flags=(state.flags[2]&~0x10u)|(flags&0x10u);memcpy(actor+0x2c,&flags,4);
            memcpy(actor+0x10,state.life,16);memcpy(actor+0x94,&state.animation,4);
            memcpy(body+0x64,state.position,12);memcpy(actor+0x30,state.position,12);
            memcpy(actor+0xbb4,state.muzzle,12);memcpy(actor+0xb88,state.aim,12);memcpy(body+0x1cc,&state.health,4);
            memcpy(body+0x1d0,&state.armor,4);memcpy(body+0x1d4,&state.last_damage_tick,4);
            memcpy(body+0x1d8,&state.max_health,4);memcpy(actor+0xb08,state.health_scale,8);
            memcpy(body+0xc0,state.angles,12);
            memcpy(body+0x70,&state.heading,4);
            memcpy(body+0x258,&state.spawn,4);
            pc_network_bot_bounds[index].entity=entity;pc_network_bot_bounds[index].spawn=state.spawn;
            memcpy(pc_network_bot_bounds[index].center,state.bounds_center,12);
            memcpy(pc_network_bot_bounds[index].radius,state.bounds_radius,12);
            memcpy(body+0x1a8,state.bounds_center,12);
            memcpy(body+0x1c0,state.bounds_radius,12);
            pc_character_camo_apply(actor,&state.camo);
            pc_network_bots_present(entity,&state);
            pc_network_bots_entity_refresh(entity);
            pc_network_bots_entity_room(entity,state.primary_room);
            /* The host can retire drawing before releasing its actor link.
               Preserve this final death pose once, before the hidden replica
               is reused. Cached snapshots and reliable replays cannot duplicate it. */
            if(state.health<=0.f && !(state.entity_flags&1u) && !corpse_copied[index])
                corpse_copied[index]=(unsigned char)pc_network_bots_preserve_corpse(entity,state.spawn);
            if(state.shot.serial!=seen_shots[index]) {
                seen_shots[index]=state.shot.serial;
                if(state.shot.serial && state.shot.spawn==state.spawn && state.tick-state.shot.tick<=30u &&
                   (state.shot.target_slot==UINT32_MAX || state.shot.target_slot<humans+bots) && state.health>0.f)
                    pc_network_bots_present_shot(entity,&state.shot);
            }
            /* Pose, weapon and spatial refreshes can overwrite local bounds.
               Restore the host hit geometry after all presentation work. */
            memcpy(body+0x1a8,state.bounds_center,12);
            memcpy(body+0x1c0,state.bounds_radius,12);
        }
    }
    if(role==1u && (!sent || tick-last_tick>=3u)) {last_tick=tick;sent=1;}
#endif
}

/* Opt-in bounded diagnostic: record actual hit delivery, not health settings.
 * Tests route this file into their disposable run directory. */
static void pc_network_bots_guest_damage(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(pc_story_network_damage()) {g_eax=0;g_esp+=4u;return;}
    if(pc_story_network_hit_trace(original)) return;
    static unsigned traces;
    /* Guest shot replay (including delayed projectile effects) is cosmetic.
       Only the original incoming host event may apply bot-sourced damage. */
    if(*(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==2u &&
       !*(uint32_t *)((uintptr_t)0x0076b170u+g_xbox_mem_offset) && pc_network_enabled()) {
        uint32_t ctx=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
        uint32_t source=*(uint32_t *)((uintptr_t)(ctx+4u)+g_xbox_mem_offset);
        const uint8_t *config=(const uint8_t *)((uintptr_t)0x007de400u+g_xbox_mem_offset);
        unsigned slot,humans=config[0],bots=config[2];
        uint32_t pool=*(uint32_t *)((uintptr_t)0x007356d8u+g_xbox_mem_offset);
        if(source && pool && humans && bots && bots<=10u && humans+bots<=16u) {
            uint32_t owner=*(uint32_t *)((uintptr_t)(source+0x7cu)+g_xbox_mem_offset);
            uint32_t type=*(uint32_t *)((uintptr_t)(source+0x20u)+g_xbox_mem_offset);
            for(slot=humans;slot<humans+bots;slot++) if(pc_network_bots_guest_entity(slot)==source ||
                (type==8u && owner==pool+slot*0xea0u)) {
                g_eax=0;g_esp+=4u;return;
            }
        }
    }
    /* Never run a second health/death simulation for a replicated NPC. Local
       human hits use the existing reliable request; incoming damage is already
       represented by the host snapshot and must not be subtracted twice. */
    if(*(uint32_t *)((uintptr_t)0x0076b140u+g_xbox_mem_offset)==2u && pc_network_enabled()) {
        uint32_t ctx=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
        if(ctx && pc_network_bots_guest_slot(*(uint32_t *)((uintptr_t)ctx+g_xbox_mem_offset))<16u) {
            if(!*(uint32_t *)((uintptr_t)0x0076b170u+g_xbox_mem_offset))
                pc_network_bots_request_hit(ctx);
            g_eax=0;g_esp+=4u;return;
        }
    }
    const char *path=getenv("TSFP_BOT_TRACE");
    if(path && *path && traces<256u && pc_network_enabled()) {
        uint32_t ctx=*(uint32_t *)((uintptr_t)(g_esp+4u)+g_xbox_mem_offset);
        uint32_t entity=*(uint32_t *)((uintptr_t)ctx+g_xbox_mem_offset);
        unsigned slot;
        for(slot=0;slot<16u;slot++) if(entity && pc_network_bots_guest_entity(slot)==entity) {
            float before,after,armor_before,armor_after,amount;FILE *log;uint32_t source;
            memcpy(&before,(void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),4);
            memcpy(&amount,(void *)((uintptr_t)(ctx+0x20u)+g_xbox_mem_offset),4);
            memcpy(&armor_before,(void *)((uintptr_t)(entity+0x1d0u)+g_xbox_mem_offset),4);
            source=*(uint32_t *)((uintptr_t)(ctx+4u)+g_xbox_mem_offset);
            original();
            memcpy(&after,(void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),4);
            memcpy(&armor_after,(void *)((uintptr_t)(entity+0x1d0u)+g_xbox_mem_offset),4);
            log=fopen(path,traces++?"a":"w");
            if(log) {fprintf(log,"bot=%u source=%08x damage=%g hp=%g->%g armor=%g->%g result=%u\n",slot,source,amount,before,after,armor_before,armor_after,g_eax);fclose(log);}
            return;
        }
    }
#endif
    original();
}
