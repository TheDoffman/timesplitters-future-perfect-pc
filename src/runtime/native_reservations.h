/* Original reservation state for hosts without native NT virtual memory. */
#ifndef TSFP_NATIVE_RESERVATIONS_H
#define TSFP_NATIVE_RESERVATIONS_H
#include <stddef.h>
#include <stdint.h>
#include "host_pages.h"

typedef struct tsfp_native_vm_region {
    uintptr_t base, allocation_base;
    size_t bytes;
    uint32_t allocation_protection, state, protection, type;
    uint16_t partition_id;
} tsfp_native_vm_region;
typedef struct tsfp_reservation_backend {
    void *context;
    /* reserve returns an anonymous, initially zero, inaccessible mapping.
     * It must never replace an existing mapping, even for an exact hint. */
    void *(*reserve)(void *,void *hint,size_t bytes);
    int (*release)(void *,void *base,size_t bytes);
    /* commit:1 success;0 failure with this span still reserved/unchanged;
     * -1 means access may have changed and cannot be restored. The complete
     * reservation is then quarantined until exact release. zero requests
     * clearing old decommitted contents before the requested access is exposed.
     * Existing committed pages never get zero=1. */
    int (*commit)(void *,void *base,size_t bytes,uint32_t protection,int zero);
    /* Revoke access without removing address-space ownership. Reclamation may
     * be advisory; future commit must zero pages regardless of reclamation. */
    /* Same 1/0/-1 outcome convention as commit. */
    int (*decommit)(void *,void *base,size_t bytes);
    int (*query)(void *,const void *,tsfp_host_page_region *);
} tsfp_reservation_backend;
typedef struct tsfp_native_reservation {
    void *base;
    size_t bytes;
    unsigned char *pages;
    uint32_t allocation_protection;
    int quarantined;
} tsfp_native_reservation;
typedef struct tsfp_native_reservations {
    tsfp_reservation_backend backend;
    tsfp_native_reservation *records;
    size_t capacity,page_size,live;
    unsigned active;
#ifdef _MSC_VER
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_native_reservations;

/* Zero-initialized owner; init/stop require quiescent callers. Metadata and
 * callbacks outlive the owner. Per-page state is allocated before host mapping.
 * Functions serialize metadata; callers keep direct accesses and external
 * protection changes quiescent while changing mappings. No callback reentry. */
int tsfp_native_reservations_init(tsfp_native_reservations *,
    const tsfp_reservation_backend *,size_t page_size,tsfp_native_reservation *,size_t capacity);
void *tsfp_native_reserve(tsfp_native_reservations *,void *hint,size_t bytes,
    uint32_t type,uint32_t protection);
int tsfp_native_reserve_free(tsfp_native_reservations *,void *,size_t bytes,uint32_t type);
int tsfp_native_reserve_query(tsfp_native_reservations *,const void *,tsfp_native_vm_region *);
/* Owned spans must be wholly committed. Foreign mapped spans retain ordinary
 * host protection behavior; partial overlaps with owned reservations fail. */
int tsfp_native_reserve_protect(tsfp_native_reservations *,void *,size_t,uint32_t);
/* Stop is retryable after host release failure; failed mappings remain owned.
 * The production adapter keeps its owner for process lifetime instead. */
int tsfp_native_reservations_stop(tsfp_native_reservations *);
#endif
