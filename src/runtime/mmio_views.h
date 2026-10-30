/* Protected device mappings and independent writable aliases. */
#ifndef TSFP_MMIO_VIEWS_H
#define TSFP_MMIO_VIEWS_H
#include "shared_views.h"
typedef struct tsfp_mmio_view {
    tsfp_shared_views store;
    tsfp_shared_view views[2];
    void *guest,*alias;
    size_t bytes;
    unsigned visible;
} tsfp_mmio_view;
typedef struct tsfp_mmio_views {
    tsfp_shared_backend backend;
    size_t page_size,granularity,capacity;
    tsfp_mmio_view *views;
    unsigned active;
} tsfp_mmio_views;
/* All construction and reset must precede/follow quiescent device/signal users.
 * Published fields are immutable for their whole lifetime. Lookup only reads
 * those fields and performs bounded integer arithmetic: no lock/allocation/OS
 * operation. This is not concurrent teardown or a lifetime pin. */
int tsfp_mmio_views_init(tsfp_mmio_views *,const tsfp_shared_backend *,size_t page,
    size_t granularity,tsfp_mmio_view *,size_t capacity);
void *tsfp_mmio_views_map(tsfp_mmio_views *,void *hint,size_t bytes);
void *tsfp_mmio_views_alias(const tsfp_mmio_views *,const void *address,size_t bytes);
/* Withdraw all lookup entries before retryable cleanup. Failed unpublished
 * initialization resources remain owned here too, preventing slot reuse. */
int tsfp_mmio_views_reset(tsfp_mmio_views *);
#endif
