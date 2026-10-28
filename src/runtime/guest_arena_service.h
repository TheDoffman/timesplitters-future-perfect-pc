/* Single project arena used by native guest maps and compatibility callers. */
#ifndef TSFP_GUEST_ARENA_SERVICE_H
#define TSFP_GUEST_ARENA_SERVICE_H
#include "shared_views.h"
/* Lifecycle and MMIO construction/reset require quiescent users. No implicit
 * worker join occurs here. The service owns at most one guest arena. */
void *tsfp_guest_arena_reserve(size_t bytes);
int tsfp_guest_arena_release(void);
int tsfp_guest_arena_contains(const void *,size_t bytes);
int tsfp_guest_arena_ready(void);
int tsfp_guest_arena_occupied(void);
int tsfp_guest_arena_backend(tsfp_shared_backend *,size_t *page,size_t *granularity);
/* Descriptor -1 creates private pages, otherwise descriptor >=0 refers to an
 * already-owned native file. Access is a TSFP_ARENA_* mask, not NT page flags.
 * The caller keeps backing alive until placement; mapped views retain it. */
void *tsfp_guest_arena_place(void *,size_t bytes,int descriptor,size_t offset,unsigned access);
int tsfp_guest_arena_unmap(void *);
void *tsfp_guest_mmio_map(void *,size_t bytes);
void *tsfp_guest_mmio_alias(const void *,size_t bytes);
#endif
