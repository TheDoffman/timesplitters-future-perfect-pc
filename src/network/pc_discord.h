#ifndef TSFP_PC_DISCORD_H
#define TSFP_PC_DISCORD_H
#include <stddef.h>
#include <stdint.h>

typedef struct PcDiscordConfig { int enabled; char application_id[21]; } PcDiscordConfig;
/* Plain values sampled on the guest thread; the IPC worker never reads Xbox RAM. */
typedef struct PcDiscordSnapshot {
    int32_t scene;
    uint32_t flags, menu_mode;
    int network, paused;
    uint32_t mode;
    /* Built-in game text only, copied before leaving the guest thread. */
    char mission[64], map[48], rules[48], event[64];
    unsigned humans, bots, capacity, difficulty;
    int role, lobby;
} PcDiscordSnapshot;
typedef struct PcDiscordActivity { char details[128], state[128]; } PcDiscordActivity;
int pc_discord_parse_config(const char *text, PcDiscordConfig *out);
void pc_discord_activity(const PcDiscordSnapshot *snapshot, PcDiscordActivity *out);
size_t pc_discord_payload(char *out, size_t size, const PcDiscordActivity *activity,
    uint32_t pid, int64_t started, unsigned nonce);
void pc_discord_start(void);
void pc_discord_submit(const PcDiscordSnapshot *snapshot);
void pc_discord_stop(void);
#endif
