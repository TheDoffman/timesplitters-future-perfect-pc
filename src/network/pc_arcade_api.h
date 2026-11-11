#ifndef TSFP_PC_ARCADE_API_H
#define TSFP_PC_ARCADE_API_H
#include "pc_arcade_control.h"
#ifdef TSFP_ARCADE_MIGRATION
extern int g_tsfp_arcade_migrating;
int pc_network_arcade_progress(int active,PcArcadeControlState *out);
int pc_network_arcade_enabled(void);
void pc_network_arcade_native_peer(unsigned native_peer);
void pc_network_arcade_slots(const uint32_t slots[4]);
int pc_network_arcade_bind_peer(uint32_t ip,uint32_t previous_host);
int pc_network_arcade_snapshot(PcArcadeCheckpoint *out);
int pc_network_arcade_publish(const PcArcadeCheckpoint *snapshot);
void pc_network_arcade_ack(int ready);
int pc_network_arcade_request_leave(void);
void pc_network_arcade_send_leave(void);
int pc_network_arcade_departing(void);
uint32_t pc_network_arcade_local_ip(void);
int pc_network_arcade_adopt_key(uint32_t old_host);
int pc_network_arcade_read(void *context,uint32_t address,void *out,size_t bytes);
int pc_network_arcade_write(void *context,uint32_t address,const void *value,size_t bytes,int apply);
int pc_network_arcade_bot_read(void *context,unsigned slot,PcNetworkBotState *out);
void pc_multiplayer_migration_leave(void);
int pc_arcade_migration_frame(int after);
void pc_arcade_join_allocate(void (*original)(void));
int pc_arcade_join_initial_construct(void (*original)(void));
int pc_arcade_join_context_entry(void);
int pc_arcade_join_context_tail(void);
void pc_arcade_join_frame(void);
#endif
#endif
