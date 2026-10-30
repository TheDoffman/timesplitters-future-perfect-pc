/* Original console path planning, independent of host filesystem APIs. */
#ifndef TSFP_GUEST_PATHS_H
#define TSFP_GUEST_PATHS_H
#include <stddef.h>
#include <stdint.h>
enum {TSFP_PATH_LIMIT=4096};
enum {TSFP_PATH_PASSTHROUGH,TSFP_PATH_GAME,TSFP_PATH_SAVE,TSFP_PATH_PROFILE};
enum {TSFP_PATH_PLAIN,TSFP_PATH_TITLE,TSFP_PATH_USER,TSFP_PATH_CACHE,TSFP_PATH_PARTITION};
typedef struct {unsigned root,category;size_t relative_bytes,directory_bytes;} tsfp_path_plan;
typedef struct {
    void *context;
    /* Normal-return copied lookup;1 success,0 missing/unsupported. Preserve
     * output on failure. Success supplies full counted target, never borrowed
     * pointers. Inputs/output/context remain stable through this call. */
    int (*link)(void *,const char *,size_t,char *,size_t,size_t *);
} tsfp_path_ops;
/* Counted input, length0..4095 with no embedded NUL. ASCII-insensitive device
 * components; slash/backslash are separators. One drive-link expansion only,
 * accepted only if its complete result maps to a recognized device/drive.
 * An unsupported/oversized/NUL target leaves the original routing in control.
 * Relative output is NUL terminated with explicit host separator. Full output
 * and plan preserved on failure; disjoint caller storage, no alias to inputs.
 * PROFILE means UserData under an active profile root, otherwise under SAVE.
 * No filesystem calls, traversal removal, Unicode folding or link recursion.
 */
int tsfp_path_prepare(const char *,size_t,uint32_t title_id,char separator,
    const tsfp_path_ops *,char *,size_t,tsfp_path_plan *);
/* Bounded counted host join, used by byte-path adapters. Preserves POSIX/drive/
 * UNC root separators and strips redundant trailing root separators. Relative
 * bytes already use the requested separator. Output/length preserved on failure;
 * inputs/output are disjoint. Empty relative returns the root without a new
 * trailing separator. This does not validate host path permissions or encoding. */
int tsfp_path_join(const char *,size_t,const char *,size_t,char,char *,size_t,size_t *);
#endif
