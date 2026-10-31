/* Original copied, counted symbolic-link namespace. */
#ifndef TSFP_GUEST_SYMLINKS_H
#define TSFP_GUEST_SYMLINKS_H
#include <stddef.h>
#include <stdint.h>
enum {TSFP_SYMLINK_OK,TSFP_SYMLINK_INVALID,TSFP_SYMLINK_RESOURCE,
      TSFP_SYMLINK_COLLISION,TSFP_SYMLINK_MISSING,TSFP_SYMLINK_CLOSED};
typedef struct tsfp_symlink_node tsfp_symlink_node;
typedef struct {
    tsfp_symlink_node *head;size_t count,limit;void *context;
    void *(*allocate)(void *,size_t);void (*dispose)(void *,void *);unsigned closed;
#if defined(_MSC_VER)
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_guest_symlinks;
/* Fresh quiescent metadata; nonzero bounded capacity. Allocator supplies C
 * alignment; callbacks return normally, execute outside locks and remain live
 * until all callers join. Name/target are opaque exact-case counted bytes,
 * length1..65535. Caller keeps input bytes stable until return. No truncation,
 * Unicode/case/path normalization, guest-pointer interpretation or recursion.
 * Namespace owns independent byte copies, not guest objects or native handles.
 */
int tsfp_symlinks_init(tsfp_guest_symlinks *,size_t,
    void *(*)(void *,size_t),void (*)(void *,void *),void *);
int tsfp_symlinks_create(tsfp_guest_symlinks *,const void *,size_t,const void *,size_t);
int tsfp_symlinks_remove(tsfp_guest_symlinks *,const void *,size_t);
/* Copies a complete snapshot under the lock. Output/length are disjoint from
 * namespace/input metadata and preserved on every failure, including capacity.
 * Embedded NULs are copied as bytes; C-string/path adapters must reject them.
 * Copied bytes remain valid after concurrent remove/rebind/stop. */
int tsfp_symlinks_copy(tsfp_guest_symlinks *,const void *,size_t,void *,size_t,size_t *);
/* Close admission/detach under lock; dispose outside it. Idempotent. This is
 * not a join and does not permit resetting live metadata/storage. */
size_t tsfp_symlinks_stop(tsfp_guest_symlinks *);
#endif
