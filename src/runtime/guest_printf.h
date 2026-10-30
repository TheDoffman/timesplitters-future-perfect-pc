/* Original narrow guest printf with x86 argument decoding and checked spans. */
#ifndef TSFP_GUEST_PRINTF_H
#define TSFP_GUEST_PRINTF_H
#include "guest_virtual_memory.h"
#include <stddef.h>
typedef struct tsfp_printf_allocator {
    void *context;
    /* realloc contract: failure preserves the old allocation. No guest calls or
     * mapping changes. NULL allocator uses the standard host C allocator. */
    void *(*resize)(void *,void *,size_t);
    void (*release)(void *,void *);
} tsfp_printf_allocator;
/* Compatibility contract: clipped bytes actually stored, always NUL terminated
 * for nonzero capacity, rather than C99 would-have-written/old MSVC -1 semantics.
 * Capacity0 is an unchanged0. Failure returns0 and preserves all guest outputs.
 * Arguments are packed at x86 four-byte slot alignment: 32-bit scalars/pointers,
 * 64-bit integers and binary64 doubles; no host va_list or pointer struct casts.
 * d/i/u/o/x/X,c,s,p,n,f/F/e/E/g/G/a/A and integer h/hh/l/ll/I32/I64/q/z/t/j
 * are handled. L floats use the guest's binary64 representation. s/c remain
 * narrow even with l, retaining this runtime's compatibility policy; S/C/Z and
 * other unknown conversions stay literal "%<specifier>" without a value read.
 * p retains eight-digit lowercase guest hex, optional # prefix, independent of
 * native pointer size. A full output buffer stops further parsing/argument reads.
 * %n supports1/2/4/8-byte counts; null targets are ignored. Writes are staged and
 * visible to later guest reads, then published only on success. Overlap of a
 * count target with the destination output (including aliases) is rejected.
 * Input/output overlap is buffered. Caller owns stable mappings/access/data for
 * the full call; this resolver is not an ownership or mapping-lifetime pin. */
uint32_t tsfp_guest_printf(const tsfp_guest_vm *,uint32_t destination,uint32_t capacity,
    uint32_t format,uint32_t arguments,const tsfp_printf_allocator *);
#endif
