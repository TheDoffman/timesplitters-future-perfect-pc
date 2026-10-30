/* Original counted-name index for guest object ownership. */
#ifndef TSFP_GUEST_OBJECT_NAMES_H
#define TSFP_GUEST_OBJECT_NAMES_H
#include "guest_objects.h"

enum { TSFP_NAME_OK, TSFP_NAME_INVALID, TSFP_NAME_CAPACITY, TSFP_NAME_ALLOCATION,
       TSFP_NAME_COLLISION, TSFP_NAME_CLOSED, TSFP_NAME_OBJECT, TSFP_NAME_MISSING };
enum { TSFP_NAME_IGNORE_ASCII_CASE=1 };
typedef struct tsfp_guest_name_node tsfp_guest_name_node;
typedef struct tsfp_guest_names {
    tsfp_guest_objects *objects;
    tsfp_guest_name_node *head;
    size_t count,limit;
    void *context;
    void *(*allocate)(void *,size_t);
    void (*dispose)(void *,void *);
    unsigned closed;
#if defined(_MSC_VER)
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_guest_names;

/* Init requires fresh storage (or a stopped, empty index) and quiescence.
 * Storage and callbacks stay live until callers have
 * joined, even after stop. Allocator returns suitably aligned host storage.
 * Names are opaque counted bytes, length1..65535; no NUL or path normalization.
 * Case folding is explicitly ASCII, not a complete Xbox character-table claim.
 * Namespace/type parsing, guest memory spans and guest status ABI are separate.
 * Allocation/disposal and object destruction callbacks run outside this lock. */
int tsfp_guest_names_init(tsfp_guest_names *,tsfp_guest_objects *,size_t limit,
    void *(*allocate)(void *,size_t),void (*dispose)(void *,void *),void *context);
/* The index copies the name and owns one object reference per binding. The
 * supplied key must identify a currently retained/handled/leased object. */
int tsfp_guest_names_bind(tsfp_guest_names *,const void *,size_t,unsigned,uint64_t);
/* Success gives the caller an independent owned reference. Output is untouched
 * on failure. Case-insensitive ambiguity selects the newest matching binding. */
int tsfp_guest_names_lookup(tsfp_guest_names *,const void *,size_t,unsigned,uint64_t *);
/* expected_key==0 removes by name; otherwise the current binding must match.
 * This does not distinguish unbind/rebind of the same name to the same object. */
int tsfp_guest_names_remove(tsfp_guest_names *,const void *,size_t,unsigned,uint64_t expected_key);
/* Close to future operations and detach all names. Idempotent. Outstanding
 * lookup references remain owned by their callers. Does not join callers. */
size_t tsfp_guest_names_stop(tsfp_guest_names *);
#endif
