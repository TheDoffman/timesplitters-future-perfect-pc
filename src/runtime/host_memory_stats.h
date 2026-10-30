/* Native available-memory snapshot. No guest layout or toolkit structures. */
#ifndef TSFP_HOST_MEMORY_STATS_H
#define TSFP_HOST_MEMORY_STATS_H
#include <stdint.h>
/* Windows reports its available physical bytes. Darwin estimates available
 * bytes from free plus inactive pages; this is not exact guest free storage or
 * a promise that inactive pages can be reclaimed without work. Failure leaves
 * output untouched. A zero snapshot is valid. */
int tsfp_host_available_memory(uint64_t *bytes);
#endif
