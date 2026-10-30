#include "guest_arena_service.h"
#include "mmio_views.h"
static tsfp_owned_arena arena;
static tsfp_arena_view placements[512];
static tsfp_mmio_views mmio;
static tsfp_mmio_view registers[2];
void *tsfp_guest_arena_reserve(size_t bytes) {
    tsfp_shared_backend backend;
    size_t page,granularity;
    if(arena.base || mmio.views)return NULL;
    if(!tsfp_shared_host_arena_init(&arena,bytes,placements,512))return NULL;
    if(!tsfp_shared_host_in_arena(&backend,&arena,&page,&granularity) ||
       !tsfp_mmio_views_init(&mmio,&backend,page,granularity,registers,2)) {
        (void)tsfp_owned_arena_stop(&arena);return NULL;
    }
    return arena.base;
}
int tsfp_guest_arena_ready(void) { return arena.base && arena.active && !arena.poisoned; }
int tsfp_guest_arena_occupied(void) { return arena.base || mmio.views; }
int tsfp_guest_arena_contains(const void *p,size_t bytes) {
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)arena.base;
    return b && a>=b && a-b<arena.bytes && bytes<=arena.bytes-(a-b);
}
int tsfp_guest_arena_backend(tsfp_shared_backend *out,size_t *page,size_t *granularity) {
    return tsfp_guest_arena_ready() && tsfp_shared_host_in_arena(out,&arena,page,granularity);
}
void *tsfp_guest_arena_place(void *p,size_t bytes,int fd,size_t offset,unsigned access) {
    if(fd < -1)return NULL;
    return tsfp_owned_arena_map(&arena,p,bytes,fd<0?0:(uintptr_t)fd+1,offset,access);
}
int tsfp_guest_arena_unmap(void *p) { return tsfp_owned_arena_release(&arena,p); }
void *tsfp_guest_mmio_map(void *p,size_t bytes) {
    if(!tsfp_guest_arena_ready() || !tsfp_guest_arena_contains(p,bytes))return NULL;
    return tsfp_mmio_views_map(&mmio,p,bytes);
}
void *tsfp_guest_mmio_alias(const void *p,size_t bytes) { return tsfp_mmio_views_alias(&mmio,p,bytes); }
int tsfp_guest_arena_release(void) {
    arena.active=0; /* quiescent lifecycle: stop admission even if cleanup fails */
    if(!tsfp_mmio_views_reset(&mmio))return 0;
    return tsfp_owned_arena_stop(&arena);
}
