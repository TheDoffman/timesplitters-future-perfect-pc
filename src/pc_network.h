#ifndef TSFP_PC_NETWORK_H
#define TSFP_PC_NETWORK_H
#include <stdint.h>
#include <stddef.h>
/* US executable's public XNet entry points. Guest pointers stay 32-bit. */
int pc_network_enabled(void);
unsigned pc_network_arguments(uint32_t address);
uint32_t pc_network_call(uint32_t address, const uint32_t *args, ptrdiff_t memory_offset);
void pc_network_shutdown(void);
int pc_network_set_online(const char *configuration);
#endif
