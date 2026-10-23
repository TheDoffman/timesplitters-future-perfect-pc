#ifndef TSFP_PC_NETWORK_H
#define TSFP_PC_NETWORK_H
#include <stdint.h>
/* Monotonic seconds for guest replica presentation, independent of match time. */
double pc_network_bots_time(void);
#include <stddef.h>
/* US executable's public XNet entry points. Guest pointers stay 32-bit. */
int pc_network_enabled(void);
unsigned pc_network_arguments(uint32_t address);
uint32_t pc_network_call(uint32_t address, const uint32_t *args, ptrdiff_t memory_offset);
void pc_network_shutdown(void);
int pc_network_set_online(const char *configuration);
/* 0 offline, 1 LAN, 2 Online; RTT is negative when no fresh peer replied. */
int pc_network_latency(double *milliseconds, unsigned *peers);
#include "pc_network_bots_state.h"
/* Extension packets share the existing compatible LAN/Online transport. */
void pc_network_bots_context(unsigned role,uint32_t host_ip,uint32_t match,unsigned humans,unsigned bots);
void pc_network_bots_send(const PcNetworkBotState *state);
void pc_network_bots_targets(const uint32_t *addresses,unsigned count);
int pc_network_bots_receive(unsigned slot,PcNetworkBotState *state);
#endif
