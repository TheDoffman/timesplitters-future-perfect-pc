/* Original counted sharing policy for the Xbox byte layout. */
#ifndef TSFP_GUEST_IO_SHARE_H
#define TSFP_GUEST_IO_SHARE_H
#include <stddef.h>
#include <stdint.h>
#define TSFP_SHARE_OK UINT32_C(0)
#define TSFP_SHARE_BAD_ARGUMENT UINT32_C(0xc000000d)
#define TSFP_SHARE_CONFLICT UINT32_C(0xc0000043)
/* File prefix (3 bytes) and seven sharing bytes must be disjoint stable writable
 * mappings. Caller serializes all operations on the same file/share group.
 * No host structures, allocation, driver callbacks or native file opens here.
 * Unsupported sharing flags, inconsistent counters, overflow/underflow fail
 * without writing either destination. Check-only preserves sharing counters.
 * Successful remove clears only the six access/share flags to reject reuse.
 * Access masks must already be granted/expanded by the caller, not GENERIC_*.
 */
uint32_t tsfp_io_share_check(void *,size_t,void *,size_t,uint32_t,uint32_t,int update);
uint32_t tsfp_io_share_set(void *,size_t,void *,size_t,uint32_t,uint32_t);
uint32_t tsfp_io_share_remove(void *,size_t,void *,size_t);
#endif
