#ifndef TSFP_PC_NETWORK_BOTS_H
#define TSFP_PC_NETWORK_BOTS_H
#include <stdint.h>
#include <string.h>

/* Experimental US match configuration adapter. Transport/player ownership
 * remains original; this is deliberately opt-in until two-peer AI replication
 * has been validated. Counts describe humans followed by AI, not sixteen
 * humans plus AI beyond the game's sixteen character slots. */
#define PC_NETWORK_BOT_LIMIT 10u
#define PC_NETWORK_CHARACTER_LIMIT 16u
#define PC_NETWORK_MATCH_BYTES 0xa0u
typedef struct PcNetworkBot {
    int32_t character,team,skill,enabled,override_enabled,preview,variant;
} PcNetworkBot;

typedef struct PcNetworkBotPopulation {
    unsigned players, human_slots, selected, bots, waiting, limit;
} PcNetworkBotPopulation;

static int pc_network_bot_selected(const PcNetworkBot *bot)
{
    int selected=bot->override_enabled==-1?bot->enabled:bot->override_enabled;
    return selected==1 && bot->character>=0 && bot->character<=255 &&
        bot->skill>=0 && bot->skill<=4 && bot->team>=0 && bot->team<=3 &&
        bot->variant>=-1 && bot->variant<=255;
}

static PcNetworkBotPopulation pc_network_bots_population(
    const uint32_t active[16],const PcNetworkBot roster[10],unsigned capacity)
{
    PcNetworkBotPopulation p={0};unsigned i,room;
    p.limit=capacity<PC_NETWORK_CHARACTER_LIMIT?capacity:PC_NETWORK_CHARACTER_LIMIT;
    for(i=0;i<PC_NETWORK_CHARACTER_LIMIT;i++) if(active[i]&1u) {
        p.players++;p.human_slots=i+1u;
    }
    for(i=0;i<PC_NETWORK_BOT_LIMIT;i++) p.selected+=pc_network_bot_selected(&roster[i]);
    /* Original global player IDs cannot be compacted while peers are joined. */
    room=p.human_slots<p.limit?p.limit-p.human_slots:0u;
    p.bots=p.selected<room?p.selected:room;
    p.waiting=p.selected-p.bots;
    return p;
}

static unsigned pc_network_bots_configure(uint8_t *config, unsigned bytes,
    const uint32_t active[16], const PcNetworkBot roster[10],
    int enabled, int network, int host, unsigned mode,unsigned capacity)
{
    unsigned humans, count=0, i;
    PcNetworkBotPopulation population;
    uint8_t staged[PC_NETWORK_MATCH_BYTES];
    if (!config || !active || !roster || bytes!=PC_NETWORK_MATCH_BYTES || !enabled ||
        !network || !host || mode<11u || mode>13u) return 0;
    /* Preserve holes in original global player IDs. Never reuse a human slot. */
    population=pc_network_bots_population(active,roster,capacity);
    humans=population.human_slots;
    if (!humans) return 0;
    if (!config[1] || config[1]>4u || config[1]>humans) return 0;
    memcpy(staged,config,sizeof staged);
    for (i=0;i<PC_NETWORK_BOT_LIMIT && count<population.bots;i++) {
        const PcNetworkBot *bot=&roster[i];
        unsigned slot=humans+count,teams=config[0x56]==4u?4u:2u;
        if (!pc_network_bot_selected(bot)) continue;
        staged[0x14+slot]=(uint8_t)bot->character;
        staged[0x24+slot]=(config[0x58]&1u)?(uint8_t)(1u+(unsigned)bot->team%teams):0;
        staged[0x34+slot]=(uint8_t)bot->skill;
        staged[0x44+slot]=(uint8_t)bot->variant;
        count++;
    }
    staged[0]=(uint8_t)humans;
    staged[2]=(uint8_t)count;
    memcpy(config,staged,sizeof staged);
    return count;
}
#endif
