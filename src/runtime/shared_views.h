/* Project-owned shared backing and view ownership. No toolkit ABI types. */
#ifndef TSFP_SHARED_VIEWS_H
#define TSFP_SHARED_VIEWS_H
#include <stddef.h>
#include <stdint.h>
#include "owned_arena.h"

enum { TSFP_VIEW_READ = 1, TSFP_VIEW_WRITE = 2 };
typedef struct tsfp_shared_backend {
    void *context;
    /* Open publishes a nonzero opaque token only on success. Close consumes
     * that token once, after every view is gone; never retry a consumed fd. */
    int (*open)(void *,size_t,uintptr_t *);
    void (*close)(void *,uintptr_t);
    /* A nonnull address is a nondestructive hint, never MAP_FIXED. Successful
     * returned mappings are valid page-aligned spans, even if the hint differs.
     * The owner handles exact-address refusal and cleanup. */
    void *(*map)(void *,uintptr_t,size_t offset,size_t bytes,void *hint,unsigned access);
    /* Failure leaves ownership with the caller. No foreign mapping operations. */
    int (*unmap)(void *,void *,size_t);
} tsfp_shared_backend;

typedef struct tsfp_shared_view {
    void *base;
    size_t bytes,offset;
    unsigned published;
} tsfp_shared_view;

typedef struct tsfp_shared_views {
    tsfp_shared_backend backend;
    uintptr_t backing;
    size_t bytes,page_size,granularity,capacity,live;
    tsfp_shared_view *views;
    unsigned active;
#ifdef _MSC_VER
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_shared_views;

/* Zero-initialized owner and disjoint view storage. Init/stop need quiescent
 * callers and direct users of views. Other calls serialize metadata, not loads
 * through returned pointers. Backend callbacks must not reenter. Metadata and
 * backend context must outlive every view. All sizes are explicit host pages;
 * offsets and requested addresses use the host's mapping granularity.
 * Addresses are not generation tokens: do not release stale reused addresses. */
int tsfp_shared_views_init(tsfp_shared_views *,const tsfp_shared_backend *,
    size_t bytes,size_t page_size,size_t granularity,tsfp_shared_view *,size_t capacity);
void *tsfp_shared_views_map(tsfp_shared_views *,void *hint,size_t offset,size_t bytes,unsigned access);
int tsfp_shared_views_release(tsfp_shared_views *,void *base);
/* Retry cleanup of refused mappings that could not be unmapped immediately.
 * These mappings are retained but never published to the requesting caller. */
int tsfp_shared_views_reap(tsfp_shared_views *);
/* Stops admission, releases all views, then consumes backing once. Failure is
 * retryable: ownership stays recorded. No shutdown implies worker quiescence. */
int tsfp_shared_views_stop(tsfp_shared_views *);

/* OS-only adapter. Windows page-file sections; POSIX private temporary backing
 * streams. No filenames or OS descriptor numbers escape this boundary. */
int tsfp_shared_host_backend(tsfp_shared_backend *,size_t *page,size_t *granularity);
/* Darwin's main guest arena and backing adapter. Init with caller-owned view
 * records. Ordinary Windows mappings use the above backend without an arena. */
int tsfp_shared_host_arena_init(tsfp_owned_arena *,size_t bytes,tsfp_arena_view *,size_t capacity);
int tsfp_shared_host_in_arena(tsfp_shared_backend *,tsfp_owned_arena *,size_t *page,size_t *granularity);
#endif
