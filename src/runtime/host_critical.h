/* Native recursive mutex adapter for the original shared critical registry. */
#ifndef TSFP_HOST_CRITICAL_H
#define TSFP_HOST_CRITICAL_H
#include "guest_critical.h"
typedef struct tsfp_host_critical tsfp_host_critical;
tsfp_host_critical *tsfp_host_critical_create(int single);
tsfp_critical_registry *tsfp_host_critical_shared(tsfp_host_critical *);
/* Join all users first. Refuses active/owned/quarantined state and retains the
 * holder on failure. Native mutexes are never copied or relocated. */
int tsfp_host_critical_destroy(tsfp_host_critical *);
#endif
