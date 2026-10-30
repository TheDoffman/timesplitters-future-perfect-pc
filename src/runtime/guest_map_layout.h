/* Original guest address-space construction and ownership. */
#ifndef TSFP_GUEST_MAP_LAYOUT_H
#define TSFP_GUEST_MAP_LAYOUT_H
#include "shared_views.h"
enum { TSFP_MAP_PHYSICAL=0,TSFP_MAP_NV2A=1,TSFP_MAP_MCPX=2,TSFP_MAP_FLASH=3,TSFP_MAP_KERNEL=4 };
typedef struct tsfp_layout_host {
    void *context;
    /* begin may leave resources needing finish even on failure. fixed_base is
     * optional: Darwin supplies its reserved arena; Windows tries low hints. */
    int (*begin)(void *,tsfp_shared_backend *,void **fixed_base,size_t *page,size_t *granularity);
    void *(*private_map)(void *,void *,size_t,unsigned role);
    int (*private_unmap)(void *,void *,size_t,unsigned role);
    int (*healthy)(void *);
    int (*finish)(void *);
} tsfp_layout_host;
typedef struct tsfp_guest_map_config {
    size_t ram_bytes,physical_bytes;
    unsigned high_alias;
} tsfp_guest_map_config;
typedef struct tsfp_guest_map_layout {
    tsfp_layout_host host;
    tsfp_guest_map_config config;
    tsfp_shared_views ram_store,physical_store;
    tsfp_shared_view ram_records[29],physical_records[3];
    void *ram,*physical,*nv2a,*mcpx,*flash,*kernel,*tiled,*high,*mirrors[28];
    void *private_views[5];size_t private_bytes[5];
    size_t ram_bytes,tiled_bytes;
    unsigned started,published,ready,physical_shared;
} tsfp_guest_map_layout;
/* Zero-initialized owner. Setup/rollback require quiescent users. Publication
 * happens before the first device/guest worker can access any mapping. */
int tsfp_guest_map_init(tsfp_guest_map_layout *,const tsfp_layout_host *,const tsfp_guest_map_config *);
int tsfp_guest_map_publish(tsfp_guest_map_layout *);
/* 1 fully released;0 cleanup failed and remains owned;2 published maps retained
 * until process exit because this layer cannot prove worker quiescence. Never
 * clear publication merely to force cleanup. No reinitialization of retained
 * layouts. Worker teardown needs its own future explicit quiescence boundary. */
int tsfp_guest_map_stop(tsfp_guest_map_layout *);
/* One zero-initialized host session per layout. Do not share/rebind a session
 * while its layout is started. Session storage outlives the layout. */
typedef struct tsfp_guest_host_session { unsigned owns_arena; } tsfp_guest_host_session;
int tsfp_guest_map_host(tsfp_layout_host *,tsfp_guest_host_session *);
#endif
