/* Original shared host-storage selection, snapshots and synthetic disk format. */
#ifndef TSFP_GUEST_STORAGE_H
#define TSFP_GUEST_STORAGE_H
#include "guest_paths.h"
typedef struct {char game[TSFP_PATH_LIMIT],save[TSFP_PATH_LIMIT],profile[TSFP_PATH_LIMIT];} tsfp_storage_snapshot;
typedef struct {
    tsfp_storage_snapshot paths;unsigned ready,guarded;
    char required_save[TSFP_PATH_LIMIT];
#if defined(_MSC_VER)
    volatile long lock;
#else
    unsigned char lock;
#endif
} tsfp_storage_state;
enum {TSFP_STORAGE_CALLER,TSFP_STORAGE_ENVIRONMENT,TSFP_STORAGE_DEFAULT};
/* All strings are stable counted-by-NUL host bytes, bounded4095. Metadata and
 * output are disjoint. Explicit save > nonempty environment > platform default;
 * explicit empty paths reject. Selection performs no filesystem operations.
 * Output/source preserved on failure. Host adapter validates encoding/roots. */
int tsfp_storage_select(const char *,const char *,const char *,const char *,const char *,tsfp_storage_snapshot *,unsigned *);
/* Zero/static-initialized state, never reset while live. Filesystem/encoding
 * checks happen before these calls. Commit roots preserves profile selection.
 * Empty profile clears override; may set before first roots commit. Snapshots
 * are copied under one lock, with no callbacks/OS operations under that lock.
 * Failed/unready snapshot preserves output. */
int tsfp_storage_commit(tsfp_storage_state *,const tsfp_storage_snapshot *);
int tsfp_storage_profile(tsfp_storage_state *,const char *);
int tsfp_storage_read(tsfp_storage_state *,tsfp_storage_snapshot *);
/* One-way startup fence. Register before storage initialization. A valid root
 * becomes immutable; NULL/empty/oversized or late registration latches a closed
 * fence. No disable/reset API. Existing profile override is cleared; guarded
 * sessions reject nonempty overrides and use their isolated UserData directory.
 * Caller validates/owns the filesystem namespace for the whole storage lifetime.
 * Inputs/outputs must be disjoint from state. All calls perform metadata only. */
int tsfp_storage_require(tsfp_storage_state *,const char *);
/* 0 unfenced;1 copies required root;-1 closed/insufficient output capacity.
 * Output is preserved unless1. A host must consult this BEFORE mkdir/defaults. */
int tsfp_storage_required(tsfp_storage_state *,char *,size_t);
/* Compatibility image geometry retained from this runtime, not a full physical
 * Xbox disk. New Partition0 is2560 bytes with a512-byte synthetic PARTINFO block
 * at0x800; Partition1/2/3-5 sizes retain their existing sector counts. */
int tsfp_storage_image_bytes(unsigned,uint64_t *);
int tsfp_storage_table(void *,size_t);
enum {TSFP_STORAGE_NEW=1,TSFP_STORAGE_EXISTS=2};
typedef struct {
    void *context;
    /* Create exclusively. NEW transfers one owned nonnull token. EXISTS must
     * not open/mutate existing storage and transfers nothing;0 fails. */
    int (*create)(void *,unsigned,void **);
    int (*resize)(void *,void *,uint64_t);
    int (*write)(void *,void *,uint64_t,const void *,size_t);
    /* Once for NEW, outside state locks. commit0 discards only newly-owned
     * storage.1 consumed cleanly;0 uncertain: backend retains accounting and
     * never retries resource disposal automatically. Normal-return/no-guest. */
    int (*close)(void *,void *,int commit);
} tsfp_storage_ops;
typedef struct {unsigned created,existing,failed,uncertain;} tsfp_storage_report;
/* Prepare six images. Existing files are not resized, repaired or rewritten.
 * Report remains disjoint/live; callbacks own any uncertain native resources.
 * Caller serializes initialization/namespace mutation and joins before teardown.
 * Return1 iff all six exist or were initialized/closed successfully. */
int tsfp_storage_prepare(const tsfp_storage_ops *,tsfp_storage_report *);
#endif
