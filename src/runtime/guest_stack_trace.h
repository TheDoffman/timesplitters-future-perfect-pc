/* Original guest EBP diagnostics over checked, owner-supplied stack extents. */
#ifndef TSFP_GUEST_STACK_TRACE_H
#define TSFP_GUEST_STACK_TRACE_H
#include "guest_virtual_memory.h"
typedef struct tsfp_guest_stack_view {uint32_t esp,ebp,caller,low,high,code_low,code_high;} tsfp_guest_stack_view;
/* Position0 is the kernel call site, then return PCs from ascending EBP pairs.
 * Skipped PCs need not be code addresses; captured PCs must be inside the supplied
 * executable range. Frames stay aligned, at/above ESP, inside [low,high), and
 * advance by at least8 bytes. Corrupt/unmapped tails return the valid prefix.
 * Collection precedes guest publication, supporting frame/output aliases. Only
 * actual output bytes are resolved; hash cannot overlap them. Hash is wrapping
 * sum of captured PCs, including empty0. Bad outputs/state/allocation return0
 * unchanged. Caller owns stable mappings, stack lifetime and host view storage. */
uint32_t tsfp_stack_trace(const tsfp_guest_vm *,const tsfp_guest_stack_view *,uint32_t skip,uint32_t count,uint32_t output,uint32_t hash);
/* Two parent call sites, skipping the kernel's immediate call site. Optional
 * four-byte outputs receive zero for missing frames. Exact aliases publish the
 * second result last; partial aliases reject without writes. */
int tsfp_stack_callers(const tsfp_guest_vm *,const tsfp_guest_stack_view *,uint32_t first,uint32_t second);
#endif
