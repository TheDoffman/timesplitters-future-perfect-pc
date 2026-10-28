/* Supported title's nine-word memory statistics, independent of host ABI. */
#ifndef TSFP_MEMORY_STATISTICS_H
#define TSFP_MEMORY_STATISTICS_H
#include <stdint.h>
enum { TSFP_MEMORY_STATISTICS_WORDS=9, TSFP_MEMORY_STATISTICS_BYTES=36 };
typedef int (*tsfp_memory_available)(void *context,uint64_t *bytes);
/* A complete snapshot or failure without publishing any output. Host counters
 * remain 64-bit until the guest policy bounds them; no modulo-32-bit wrap.
 * Compatibility: total is 16384 guest pages, and host availability above that
 * reports 8192. The six unimplemented usage categories remain zero. */
uint32_t tsfp_memory_statistics(tsfp_memory_available,void *context,
    uint32_t words[TSFP_MEMORY_STATISTICS_WORDS]);
#endif
