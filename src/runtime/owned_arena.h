/* Owned placement policy for a reserved guest address range. */
#ifndef TSFP_OWNED_ARENA_H
#define TSFP_OWNED_ARENA_H
#include <stddef.h>
#include <stdint.h>
enum { TSFP_ARENA_READ=1,TSFP_ARENA_WRITE=2,TSFP_ARENA_EXECUTE=4 };
typedef struct tsfp_arena_backend {
    void *context;
    void *(*reserve)(void *,size_t);
    int (*release)(void *,void *,size_t);
    /* The owner proves containment/nonoverlap before replacement. Token zero
     * means fresh private pages; nonzero identifies shared backing. Access is
     * a READ/WRITE/EXECUTE mask (zero is inaccessible). 1 means
     * placed, 0 means failed without changing the reservation, -1 uncertain. */
    int (*place)(void *,void *,size_t,uintptr_t token,size_t offset,unsigned access);
    /* Restore inaccessible anonymous reservation without freeing its address.
     * Same result convention. Uncertain placement/restoration poisons the
     * whole arena: retain it, stop new placements, and allow exact cleanup. */
    int (*restore)(void *,void *,size_t);
} tsfp_arena_backend;
typedef struct tsfp_arena_view { void *base;size_t bytes;unsigned state; } tsfp_arena_view;
typedef struct tsfp_owned_arena {
    tsfp_arena_backend backend;
    void *base;
    size_t bytes,page_size,capacity,live;
    tsfp_arena_view *views;
    unsigned active,poisoned;
#ifdef _MSC_VER
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_owned_arena;
/* Init/stop require quiescent callers and memory users. The arena exclusively
 * owns its complete reserved address range; no external unmap/protect/remap.
 * Metadata callbacks serialize; no callback reentry or signal-handler use. */
int tsfp_owned_arena_init(tsfp_owned_arena *,const tsfp_arena_backend *,size_t bytes,
    size_t page,tsfp_arena_view *,size_t capacity);
/* This read-only address routing helper requires stable init/stop lifetime.
 * Partial overlap counts as an intersection and will fail actual placement. */
int tsfp_owned_arena_intersects(const tsfp_owned_arena *,const void *,size_t);
void *tsfp_owned_arena_map(tsfp_owned_arena *,void *,size_t,uintptr_t,size_t,unsigned);
int tsfp_owned_arena_release(tsfp_owned_arena *,void *);
/* Refuse to destroy backing while published views remain. First release their
 * shared/private owners. Abandoned uncertain placements are released with the
 * complete arena only after all published views are gone. Retry on failure. */
int tsfp_owned_arena_stop(tsfp_owned_arena *);
#endif
