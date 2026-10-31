#include "guest_map_layout.h"
#include "guest_arena_service.h"
#if defined(_WIN32)
#include <windows.h>
#endif
static int begin(void *context,tsfp_shared_backend *out,void **fixed,size_t *page,size_t *gran) {
    (void)context;
#if defined(_WIN32)
    *fixed=NULL;return tsfp_shared_host_backend(out,page,gran);
#else
    tsfp_guest_host_session *session=context;
    if(tsfp_guest_arena_occupied())return 0;
    session->owns_arena=1;
    *fixed=tsfp_guest_arena_reserve((size_t)UINT64_C(0x100000000));
    return *fixed && tsfp_guest_arena_backend(out,page,gran);
#endif
}
static void *map_private(void *context,void *address,size_t bytes,unsigned role) {
    (void)context;
#if defined(_WIN32)
    (void)role;return VirtualAlloc(address,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
    if(role==TSFP_MAP_NV2A || role==TSFP_MAP_MCPX)return tsfp_guest_mmio_map(address,bytes);
    return tsfp_guest_arena_place(address,bytes,-1,0,TSFP_ARENA_READ|TSFP_ARENA_WRITE);
#endif
}
static int unmap_private(void *context,void *address,size_t bytes,unsigned role) {
    (void)context;(void)bytes;
#if defined(_WIN32)
    (void)role;return VirtualFree(address,0,MEM_RELEASE)!=0;
#else
    /* MMIO entries and both aliases belong to the service until finish. */
    if(role==TSFP_MAP_NV2A || role==TSFP_MAP_MCPX)return 1;
    return tsfp_guest_arena_unmap(address);
#endif
}
static int healthy(void *context) {
    (void)context;
#if defined(_WIN32)
    return 1;
#else
    return ((tsfp_guest_host_session *)context)->owns_arena && tsfp_guest_arena_ready();
#endif
}
static int finish(void *context) {
    (void)context;
#if defined(_WIN32)
    return 1;
#else
    tsfp_guest_host_session *session=context;
    if(!session->owns_arena)return 1;
    if(!tsfp_guest_arena_release())return 0;
    session->owns_arena=0;return 1;
#endif
}
int tsfp_guest_map_host(tsfp_layout_host *out,tsfp_guest_host_session *session) {
    if(!out || !session || session->owns_arena)return 0;
    *out=(tsfp_layout_host){session,begin,map_private,unmap_private,healthy,finish};return 1;
}
